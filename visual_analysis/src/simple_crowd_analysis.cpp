/* 
* SMART FP7 - Search engine for MultimediA enviRonment generated contenT
* Webpage: http://smartfp7.eu
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* The Original Code is Copyright (c) 2012-2013 Athens Information Technology
* All Rights Reserved
*
* Contributor:
*  Nikolaos Katsarakis nkat@ait.edu.gr
*/

// Note: if linking with the static version of CURL, make sure you have #defined CURL_STATICLIB
#include "simple_crowd_analysis.hpp"
#include "bgfg_ait.hpp"
#include <iostream>

using namespace std;

//If height of incoming images>MAX_HEIGHT, images will be rescaled to have processed frame height = MAX_HEIGHT
static const int MAX_HEIGHT=100;

/* Camera or video frame rate
If <=0, timestamps should be generated from current time
Otherwise, the first timestamp will be the video start time (if available) or program start
and the next ones are start+num_frame*fps
*/
extern double fps;


typedef struct
{
	Rect	rect;		// Rect defining the ROI
	int		dec;		// Dec factor to process ROI
	int		region;		// Reporting region the ROI belongs to
	double  regAreaRatio;// The ratio of the ROI area compared to the total region area
	int     weightedArea;// ROI area weighted based on the y_center
	Mat		roiImg;	    // Small image to be processed
	Mat		frgImg;		// Small foreground image
	Size	frgSize;	// Decimated small size of image to be processed
	Size	roiSize;	// Size of the ROI (same as rect.width, rect.height)
	int     area;       // Total number of the pixels contained in the decimated roi
	ait::BackgroundSubtractorAIT backgroundSegment; //Foreground/Background segmentator for this processing region

	// For colour support
	Mat     roi32f;     // Small image of ROI in 32bit float representation
	Mat     roiHSV;     // Small image of ROI in HSV colorspace
	Mat     frgColours; // Current foreground color histogram
	Mat     bkgColours; // Current background color histogram
	Mat     colourModel;// Accumulated colour model
	double  colourRate; // Current colour learning rate
} ROIstruct;

typedef struct
{
	int		r;
	int		g;
	int		b;
	double	freq;
} colour;

typedef struct
{
	string	name;	// Name of reporting region to be used in the XML output
	int	 smallMotion;	// Indication of total pixels belonging to blobs that are not large enough to be targets
	int  area; // Total region area in pixels

	double acc_crowd_density; // Accumulated crowd density

	double crowd_density_sum; // Current crowd density sum
	// value used to normalise the crowd density to [0,1]
	double crowd_density_denominator; 

	Mat    colourModel;		// Foreground colour distribution
//	double rate; // colour update rate
	Rect   colourDisp;  // Position for displaying colour histogram

	vector<int>  containedROIs; //Indexes of rois included in the region
} REGstruct;

typedef struct
{
	string name;
	double Hlimits[2];
	double Slimits[2];
	double Vlimits[2];
	Scalar bgr_color;
} Colourstruct;


typedef struct
{
	Rect rect;
	double rate;
	double peak;
	double minPeak;
	bool objCounted;
	double occupancy;
	double probOfObj;
	string name;
} ObjCountstruct;

/* Reporting and processing regions */
int	NREG = 0;		// Number of reporting regions
int NROI = 0;		// Number of processing ROIs
/* Regions had to be converted to dynamic size*/
vector<ROIstruct>	ROI;
vector<REGstruct>	REG;
vector<ObjCountstruct> objcounts;

/* Temporary variables, filled by setRegionParam() and used by initRegions()*/
vector<string> regionNames;
vector<int> roiData;
vector<string> colorPos;

/*colour begins*/
vector<Colourstruct> definedColours; //predefined colours loaded from file
Mat coloursRGB; //For displaying definedColours

// Colour modeling parameters
//How many milliseconds after video start we should start reporting colours
int REPORT_TIME = 20000;
//Colour model learning rate
double COLOUR_RATE = 0.05;
//Minimum foreground area ratio to consider colours of current frame and avoid stray pixel errors
double COLOUR_FRG_MIN = 0.03;
//Maximum foreground area ratio to consider colours of current frame and avoid flickering errors
double COLOUR_FRG_MAX = 0.85;

/*colour ends*/


//Drawing colours
Scalar colors[]={cvScalar(0,150,0),cvScalar(150,0,0),cvScalar(0,150,150),cvScalar(0,0,150)};
int ncolors = sizeof(colors)/sizeof(CvScalar);


int setRegionParam(char *name, char *value)
{
	int ret=-1;
	int data[5];
	double data1;
	char buff[500];
	if (strcmp(name,"region_name")==0)
	{
		regionNames.push_back(value);
		colorPos.push_back("top-left");
		ret=0;
	}
	else if (strcmp(name,"proc_rect")==0 && 
		sscanf(value,"%d %d %d %d %d",&data[0],&data[1],&data[2],&data[3],&data[4])==5)
	{
		if (regionNames.size()==0)
		{
			regionNames.push_back("default");
			colorPos.push_back("top-left");
			printf("No region name found, setting to \"default\", to avoid this warning set 'region_name' before 'proc_rect'\n");
		}
		roiData.push_back(regionNames.size()-1);
		for (int i=0;i<5;i++)
			roiData.push_back(data[i]);
		ret=0;
	}
	else if (strcmp(name,"count_rect")==0 && 
		sscanf(value,"%d %d %d %d %lf %499s",&data[0],&data[1],&data[2],&data[3], &data1, buff)==6)
	{
		ObjCountstruct tmp;
		tmp.rate = 0;
		tmp.rect = Rect(data[0],data[1],data[2],data[3]);
		tmp.minPeak = tmp.peak = .5*data1; //Set minimum measurable peak to half of user's threshold
		tmp.objCounted = false;
		tmp.occupancy = 0;
		tmp.probOfObj = 0;
		tmp.name = string(buff);
		objcounts.push_back(tmp);
		ret=0;
	}
	else if (strcmp(name,"color_pos")==0)
	{
		if (regionNames.size()==0)
		{
			regionNames.push_back("default");
			colorPos.push_back("top-left");
			printf("color_pos called before region_name, setting region to \"default\", to avoid this warning set 'region_name' before 'color_pos'\n");
		}
		colorPos[regionNames.size()-1]=string(value);
		ret=0;
	}
	return ret;
}

// Weighting function for the rows, gives ~70% weight to the top half of image
static inline double row_weight(int row_num)
{
	return 1/sqrt(row_num+1.0f);
}

int initRegions()
{
	int i, roiNo;
	if (ROI.size()!=0 || REG.size()!=0 || regionNames.size()==0 || roiData.size()==0 || roiData.size()%6!=0)
	{
		cout << "Regions not correctly configured, please check your configuration file\n";
		return -1;
	}
	NREG=regionNames.size();
	NROI=roiData.size()/6;
	REG.resize(NREG);
	ROI.resize(NROI);

	unsigned int colourSize=definedColours.size();
	// Make sure that the colours have been initialised
	if (colourSize==0)
	{
		if (initColours("colours.ini")<0)
		{
			cout << "Error initializing colour descriptions, make sure you have a valid colours.ini file\n";
			return -1;
		}
		colourSize=definedColours.size();
		if (colourSize==0)
		{
			cout << "Error initializing colour descriptions, make sure you have a valid colours.ini file\n";
			return -1;
		}
	}

	// Define REGs where metadata are reported
	for (i=0;i<NREG;i++)
	{
		REG[i].name=regionNames[i];
		REG[i].smallMotion=0;
		REG[i].acc_crowd_density=REG[i].crowd_density_sum=REG[i].crowd_density_denominator=0;
		REG[i].area = 0;
		REG[i].colourModel.create(1,colourSize,CV_32FC1);
		REG[i].colourModel.setTo(Scalar(0));
	}
	vector<Rect> boundBox(NREG); // bounding boxes for each region

	// Use roiData to fill in the corresponding processing ROIs
	for (i=0;i<(int)roiData.size();i+=6)
	{
		if (roiData[i]>NREG)
		{
			printf("Invalid region defined for the proczone number %d, was %d, should be <%d\n", i, roiData[i], NREG);
			return -1;
		}
		roiNo=i/6;
		ROI[roiNo].region=roiData[i];
		ROI[roiNo].dec=roiData[i+1];
		ROI[roiNo].rect=Rect(roiData[i+2],roiData[i+3],roiData[i+4],roiData[i+5]);
		ROI[roiNo].roiSize=Size(roiData[i+4],roiData[i+5]);
		ROI[roiNo].area=roiData[i+4]*roiData[i+5]/roiData[i+1]/roiData[i+1];
		// Create a foreground/background segmentator
		ROI[roiNo].backgroundSegment = ait::BackgroundSubtractorAIT();
		// Add the rectangle to the corresponding region
		REG[roiData[i]].containedROIs.push_back(roiNo);
		// If this is the first ROI added to the region, set the rect
		if (REG[roiData[i]].containedROIs.size()==1)
			boundBox[roiData[i]] = ROI[roiNo].rect;
		else //else calculate the union 
			boundBox[roiData[i]] |= ROI[roiNo].rect;
		// Each ROI area is weighted by the row_weight of its center
		ROI[roiNo].weightedArea = cvRound(ROI[roiNo].rect.area()*row_weight(roiData[i+3]+roiData[i+5]/2));
		// Add the weighted areas to the total region area
		REG[roiData[i]].area+=ROI[roiNo].weightedArea;

		ROI[roiNo].bkgColours.create(1,colourSize,CV_16UC1);
		ROI[roiNo].frgColours.create(1,colourSize,CV_16UC1);
		ROI[roiNo].colourModel.create(1,colourSize,CV_32FC1);
		ROI[roiNo].colourModel.setTo(Scalar(0));
		ROI[roiNo].colourRate = 1;
	}

	// After all ROIs have been processed, we can update each area ratio
	for (i=0;i<NROI;i++)
		ROI[i].regAreaRatio=(double) ROI[i].weightedArea/REG[ROI[i].region].area;

	// Also we now have all regions properly defined, fix the colour display region
	for (i=0;i<NREG;i++)
	{
		int colourWidth=100, colourHeight=20;
		Point startPoint = boundBox[i].tl(); //default top-left position
		if (colorPos[i].compare("top-right")==0)
			startPoint += Point(boundBox[i].width-colourWidth,0);
		else if (colorPos[i].compare("bottom-left")==0)
			startPoint += Point(0,boundBox[i].height-colourHeight);
		else if (colorPos[i].compare("bottom-right")==0)
			startPoint += Point(boundBox[i].width-colourWidth,boundBox[i].height-colourHeight);
		else if (colorPos[i].compare("center")==0)
			startPoint += Point(boundBox[i].width/2-colourWidth/2,boundBox[i].height/2-colourHeight/2);
			//Anything else than top-left, ignore it and print warning
		else if (colorPos[i].compare("top-left")!=0)
			cout << "Warning, invalid colour position " << colorPos[i] << " for region " << regionNames[i] << ", using default top-left\n";

		REG[i].colourDisp=Rect(startPoint,startPoint+Point(colourWidth,colourHeight));
	}
	return 0;
}


//
//int create_message(char *buff, size_t buffmax, double crowd_density)
//{
//	const char direct_interface_sample[]=
//		"{\n"
//		"\t\"_id\" : \"%s\",\n"
//		"\t\"timestamp\" : %lld,\n"
//		"\t\"data\" :\n"
//		"\t{\n"
//		"\t\t\"time\" : \"%s\",\n"
//		"\t\t\"crowd\" :\n"
//		"\t\t{\n"
//		"\t\t\t\"@ID\" : \"crowd1\",\n"
//		"\t\t\t\"density\" : %.3f\n"
//		"\t\t}\n"
//		"\t}\n"
//		"}\n";
//	int res;//, charcount=0;
//
//	char _id[25]=""; /* Document ID */
//	static long long timestamp=0; /* Timestamp for data */
//	/* Check that the given length can hold the resulting message, 
//	assuming we need an extra 100 chars for the timestamps */
//	if (buffmax<strlen(direct_interface_sample)+100)
//		return -1;
//
//	if (timestamp==0) {
//		getMillis(); // Init the clock
//		timestamp = getMillis();
//	}
//	else {
//		if (fps>0)
//			timestamp += (int)(1000/fps); //use the video fps
//		else
//			timestamp = getMillis();
//	}
//
//	/* Set the document ID to the xml dateTime string */
//	millis2string(timestamp, _id, sizeof(_id));
//
//	/* Set both the id the time attribute to the xml dateTime string */
//	res = sprintf(buff,direct_interface_sample,_id,timestamp,_id,crowd_density);
//	return res;
//}

string create_message(long long timestamp, double crowd_density)
{
	const char direct_interface_sample[]=
		"{\n"
		"\t\"_id\" : \"%s\",\n"
		"\t\"timestamp\" : %lld,\n"
		"\t\"data\" :\n"
		"\t{\n"
		"\t\t\"time\" : \"%s\",\n"
		"\t\t\"crowd\" :\n"
		"\t\t[\n"
		"%s"
		"\t\t],\n"
		"\t\t\"objcounter\" :\n"
		"\t\t[\n"
		"%s"
		"\t\t]\n"
		"\t}\n"
		"}\n";
//	int charcount=0;

	char _id[25]=""; /* Document ID */

	char tmp[1000*100]="";
	char tmp2[1000*100]="";

	if (NREG==0 || NROI==0)
	{
		sprintf(tmp, "%s\t\t\t{"
			"\n\t\t\t\t\"@ID\" : \"crowd1\",\n", tmp);
		sprintf(tmp, "%s\t\t\t\t\"density\" : %.4lf\n", tmp, crowd_density);
		sprintf(tmp, "%s\t\t\t}\n", tmp);
	}

	else
	{
		static int colourSize=definedColours.size();
		for (int i=0; i<NREG; i++)
		{
			sprintf(tmp, "%s\t\t\t{"
				"\n\t\t\t\t\"@ID\" : \"%s\",\n", tmp, REG[i].name.c_str());
			sprintf(tmp, "%s\t\t\t\t\"density\" : %.4lf", tmp, REG[i].acc_crowd_density);
			string colournames = "";
			string colourpercent = "";
			for (int j=0; j<colourSize; j++)
			{
				float perc=REG[i].colourModel.at<float>(j);
				if (perc>0.05)
				{
					colournames+="\""+definedColours[j].name+"\", ";
					colourpercent+=format("%4.2f, ",REG[i].colourModel.at<float>(j));
				}
			}

			if (colournames.length()>2)
			{
				colournames.erase(colournames.end()-2,colournames.end());
				colourpercent.erase(colourpercent.end()-2,colourpercent.end());
				sprintf(tmp, "%s,\n\t\t\t\t\"colournames\" : [%s],\n", tmp, colournames.c_str());
				sprintf(tmp, "%s\t\t\t\t\"colourpercentages\" : [%s]\n", tmp, colourpercent.c_str());
			}
			else
				sprintf(tmp,"%s\n", tmp);

			if (i==NREG-1)
				sprintf(tmp, "%s\t\t\t}\n", tmp);
			else
				sprintf(tmp, "%s\t\t\t},\n", tmp);
		}
	}

	int NCOUNTS=objcounts.size();
	for (int i=0;i<NCOUNTS;i++)
	{
		sprintf(tmp2, "%s\t\t\t{"
			"\n\t\t\t\t\"@ID\" : \"%s\",\n", tmp2, objcounts[i].name.c_str());
		sprintf(tmp2, "%s\t\t\t\t\"obj_per_min\" : %.2lf\n", tmp2, objcounts[i].rate);
		if (i==NCOUNTS-1)
			sprintf(tmp2, "%s\t\t\t}\n", tmp2);
		else
			sprintf(tmp2, "%s\t\t\t},\n", tmp2);
	}

	/* Set the document ID to the xml dateTime string */
	millis2string(timestamp, _id, sizeof(_id));

	/* Set both the id the time attribute to the xml dateTime string */
	return cv::format(direct_interface_sample,_id,timestamp,_id,tmp,tmp2);
}

void calculate_foreground(cv::InputArray _image, cv::OutputArray _frg_img, double rate)
{
	static int frame_count=0;
	static double dec=0; //How much to rescale incoming images for faster processing

	// Images for video processing
	cv::Mat small_img;// decimated frame
	static cv::Size frame_size; //Size of the incoming image
	static cv::Size frg_size; //Size of the foreground image

	// Initialise morphological operator for erode/dilate
	int element_size =1;
	static cv::Mat element = cv::getStructuringElement( cv::MORPH_ELLIPSE,
		cv::Size( 2*element_size + 1, 2*element_size+1 ),
		cv::Point( element_size, element_size ) );

	// Create a foreground/background segmentator
	static ait::BackgroundSubtractorAIT background_segment;

	int i;

//	static float tau=(float)background_segment.get<float>("fTau");
	frame_count++;
	cv::Mat image = _image.getMat(); // current video frame

	// Initialise image sizes if needed
	if (frame_size!=image.size())
	{
		//frame_size = cv::Size((int)capture.get(CV_CAP_PROP_FRAME_WIDTH), (int)capture.get(CV_CAP_PROP_FRAME_HEIGHT));
		frame_size=image.size();
		if (NROI==0) //If processing full frame
		{
			// If needed, reduce image sizes for faster processing
			if (frame_size.height>MAX_HEIGHT)
				dec=(double)frame_size.height/MAX_HEIGHT;
			else
				dec=1;
			frg_size = cv::Size(cvRound(frame_size.width/dec), cvRound(frame_size.height/dec));
		}
		else //Processing the configured ROIs
		{
			for (i=0;i<NROI;i++)
			{
				ROI[i].frgSize = Size(ROI[i].rect.width/ROI[i].dec,ROI[i].rect.height/ROI[i].dec);
			}
			frg_size = frame_size;
		}
	}
	_frg_img.create(frg_size,CV_8U);
	cv::Mat foreground=_frg_img.getMat();
	foreground.setTo(0);

	if (NROI==0)
	{
		// Reduce image size for faster processing
		cv::resize(image,small_img,frg_size,0,0,CV_INTER_LANCZOS4);
		//cv::imshow("Input Video, press ESC to stop",small_img);

		// invoke the background segmentation
		background_segment(small_img,foreground,rate);
		//imshow("frg",foreground);
		//if (frame_count%200==0)
		//{
		//	imwrite(format("frame_%03d.png",frame_count),small_img);
		//	imwrite(format("frg_%03d_%04.2f.png",frame_count,tau),foreground);
		//}

		// Delete shadows (that have default value 127 in BackgroundSubtractorMOG2) from the foreground image
		cv::threshold(foreground,foreground,128,255,cv::THRESH_BINARY);

		// Remove stray pixels
		cv::erode(foreground,foreground,element);
		cv::dilate(foreground,foreground,element);
	}
	else
	{
		for (i=0;i<NROI;i++)
		{
			ROI[i].frgSize = Size(ROI[i].rect.width/ROI[i].dec,ROI[i].rect.height/ROI[i].dec);
			// Reduce image size for faster processing
			cv::resize(image(ROI[i].rect),ROI[i].roiImg,ROI[i].frgSize,0,0,CV_INTER_LINEAR);
//			cv::resize(image(ROI[i].rect),ROI[i].roiImg,ROI[i].frgSize,0,0,CV_INTER_LANCZOS4);
			//cv::imshow("Input Video, press ESC to stop",small_img);
			// invoke the background segmentation
			ROI[i].backgroundSegment(ROI[i].roiImg,ROI[i].frgImg,rate);
//			imshow(format("roi%d",i),ROI[i].roiImg);
//			imshow(format("frg%d",i),ROI[i].frgImg);
			//if (frame_count%200==0)
			//{
			//	imwrite(format("frame_%03d.png",frame_count),small_img);
			//	imwrite(format("frg_%03d_%04.2f.png",frame_count,tau),foreground);
			//}

			// Delete shadows (that have default value 127 in BackgroundSubtractorMOG2) from the foreground image
			cv::threshold(ROI[i].frgImg,ROI[i].frgImg,128,255,cv::THRESH_BINARY);

			// Remove stray pixels
			cv::erode(ROI[i].frgImg,ROI[i].frgImg,element);
			cv::dilate(ROI[i].frgImg,ROI[i].frgImg,element);
			// Restore foreground to original size
			cv::resize(ROI[i].frgImg,foreground(ROI[i].rect),ROI[i].roiSize,0,0,CV_INTER_NN);
//			imshow("foregr",foreground);
//			waitKey();
		}


	}
}

double calculate_crowd_density(cv::InputArray _foreground)
{
	static int frame_count=0;

	// Images for video processing
	static cv::Size frame_size; //Size of the incoming image

	//Crowd Density statistics

	/*Accumulated crowd density is calculated as:
		(1-learning_rate)*previous_value + learning_rate*current_value;
	  
	  Therefore, by increasing the learning rate the output follows
	  the current value faster, but is also more prone to fluctuations.

	  Furthermore, in the first frames the learning rate starts as 1 to allow
	  for faster initialisation and is gradually reduced to the given value
	 */

	static double acc_crowd_density=0; // Accumulated crowd density
	static double acc_lr=0.08; //learning rate for acc crowd density

	double crowd_density=0; // Current crowd density
	// value used to normalise the crowd density to [0,1]
	static double crowd_density_denominator; 

	frame_count++;
	cv::Mat foreground = _foreground.getMat(); // current video frame

	if (NROI!=0)
		return -1;

	// Initialise image sizes if needed
	if (frame_size!=foreground.size())
	{
		frame_size=foreground.size();

		//Precalculate crowd_density_denominator that remains the same for all frames
		//It is the sum of weights for all rows
		crowd_density_denominator = 0;
		for (int r=0;r<frame_size.height;r++)
		{
			crowd_density_denominator+=row_weight(r);
		}
		crowd_density_denominator*=frame_size.width*255;
	}

	/* Typically the crowd density would be calculated as:
	
	crowd_density = cv::sum(foreground)[0]/(small_size.width*small_size.height*255);

	However this does not take into account the perspective correction, 
	i.e. that the top rows further away from the camera seem smaller
	
	Therefore we calculate the crowd_density row by row and weighting
	the results as 1/sqrt(row_num+1)
	This has the effect to allocate ~70% weight to the upper half of the image
	*/

	//Crowd density has been set to zero at start
	for (int r=0;r<frame_size.height;r++)
		crowd_density+=(cv::sum(foreground.row(r))[0]*row_weight(r));
	//Normalise the result to [0,1]
	crowd_density/=crowd_density_denominator;

	double lr=acc_lr;//current learning rate
	//Increase learning rate for the first frames
	if (frame_count<=(1-acc_lr)*100)
		lr=(100-frame_count+1.0f)/100;
	acc_crowd_density=(1-lr)*acc_crowd_density+lr*crowd_density;
	return acc_crowd_density;
}

int calculate_crowd_density(cv::InputArray _foreground, std::vector<double> &densities, std::vector<std::string> *names)
{
	static double acc_lr=0.08; //learning rate for acc crowd density
	static int frame_count=0;
	int i;

	frame_count++;
	double lr=acc_lr;//current learning rate
	//Increase learning rate for the first frames
	if (frame_count<=(1-acc_lr)*100)
		lr=(100-frame_count+1.0f)/100;

	cv::Mat foreground = _foreground.getMat(); // current video frame

	if (NROI==0 || NREG==0)
	{
		densities.resize(1);
		densities[0]=calculate_crowd_density(foreground);
		return 0;
	}

	if (frame_count==1)
	{
		for (i=0;i<NROI;i++)
		{
			REGstruct *curReg = &REG[ROI[i].region];
			for (int r=ROI[i].rect.y;r<ROI[i].rect.y+ROI[i].rect.height;r++)
			{
				curReg->crowd_density_denominator+=row_weight(r)*ROI[i].rect.width*255;
			}
		}
	}

	for (i=0;i<NROI;i++)
	{
		REGstruct *curReg = &REG[ROI[i].region];
		Mat frgROI = foreground(ROI[i].rect);
		//Crowd density sum has been set to zero before
		for (int r=ROI[i].rect.y;r<ROI[i].rect.y+ROI[i].rect.height;r++)
		{
			curReg->crowd_density_sum+=(cv::sum(frgROI.row(r-ROI[i].rect.y))[0]*row_weight(r));
		}
	}

	if (densities.size()!=NREG)
		densities.resize(NREG);
	if (names!=NULL)
		names->resize(NREG);

	for (i=0;i<NREG;i++)
	{
		//Normalise the result to [0,1]
		REG[i].crowd_density_sum/=REG[i].crowd_density_denominator;
		densities[i]=REG[i].acc_crowd_density=(1-lr)*REG[i].acc_crowd_density+lr*REG[i].crowd_density_sum;
		//Reset the sum for next iteration
		REG[i].crowd_density_sum=0;
		if (names!=NULL)
			names->at(i)=REG[i].name;
	}
	return 0;
}

void writeColorText(cv::Mat& image, string text, cv::Point offset, Scalar color, double fontScale)
{
	static int fontFace = cv::FONT_HERSHEY_DUPLEX;
	static Scalar outline=Scalar(0);
	static double prevScale=fontScale;
	static cv::Size textSize = cv::getTextSize(text,fontFace,fontScale,4,NULL);
	static cv::Point textHeight = cv::Point(0,textSize.height);

	//If always drawing at same scale, no need to recalculate height
	if (fontScale!=prevScale)
	{
		prevScale=fontScale;
		textSize = cv::getTextSize(text,fontFace,fontScale,4,NULL);
		textHeight = cv::Point(0,textSize.height);
	}

	// Write text with outline effect
	cv::putText(image,text,offset+textHeight,fontFace,fontScale,outline,4,CV_AA);
	cv::putText(image,text,offset+textHeight,fontFace,fontScale,color,1,CV_AA);
}

int drawColourModel(Mat &frame, const Mat& colourModel, Rect position)
{
	int startx=position.x;
	const float *val = colourModel.ptr<float>(0);
	for(unsigned int i=0;i<definedColours.size();i++)
	{
		int width=cvRound(val[i]*position.width);
		rectangle(frame,Rect(startx,position.y,width,position.height),definedColours[i].bgr_color,CV_FILLED);
		startx+=width;
	}
	return 0;
}

// draw colour models, optionmask==1 draw regions, optionmask==2 draw rois, 1|2 draws both
void drawColours(Mat &dispImg, int optionmask)
{
	int i;
	if (optionmask & 1)
		for(i=0;i<NREG;i++)
		{
			//Allow for some space to add the text at the bottom
			if(REG[i].colourDisp.br().y+REG[i].colourDisp.height > dispImg.rows)
				REG[i].colourDisp.y -= REG[i].colourDisp.height;
			string text=REG[i].name+ format(", density: %.2lf", REG[i].acc_crowd_density);
			writeColorText(dispImg,text,REG[i].colourDisp.tl()+Point(0,REG[i].colourDisp.height),colors[i%ncolors]);
			double maxVal;
			minMaxLoc(REG[i].colourModel,NULL,&maxVal);
			if (maxVal>0.05)
			{
				rectangle(dispImg,REG[i].colourDisp, colors[i%ncolors]);
				drawColourModel(dispImg, REG[i].colourModel, REG[i].colourDisp);
			}
		}
	if (optionmask & 2)
		for(i=0;i<NROI;i++)
		{
			Size sz=REG[ROI[i].region].colourDisp.size();
			int colr=ROI[i].region%ncolors;
			rectangle(dispImg,ROI[i].rect, colors[colr]);
			drawColourModel(dispImg, ROI[i].colourModel, Rect(ROI[i].rect.x,ROI[i].rect.y - sz.height, sz.width, sz.height));
		}
}

void drawObjects(Mat &dispImg)
{
	int colr=0;
	for (int i=objcounts.size()-1;i>=0;i--)
	{
		writeColorText(dispImg,objcounts[i].name + format(": %.1lf",objcounts[i].rate),Point(objcounts[i].rect.x,objcounts[i].rect.y+objcounts[i].rect.height),colors[colr], 0.35);
		rectangle(dispImg,objcounts[i].rect,colors[colr]);
	}
}

void drawROIs(Mat &dispImg)
{
	static char dec_txt[50];
	int colr, i;
	int printLineNo = 3;
	for(i=0;i<NROI;i++)
	{
		colr=ROI[i].region%ncolors;
		dispImg(ROI[i].rect)+=colors[colr];
		rectangle(dispImg,ROI[i].rect, colors[colr]);
		sprintf(dec_txt,"dec=%d", ROI[i].dec);
		writeColorText(dispImg,dec_txt,cvPoint(ROI[i].rect.x+5,ROI[i].rect.y+ROI[i].rect.height-25),colors[colr]);
//		drawColourModel(dispImg, ROI[i].colourModel, Rect(ROI[i].rect.x,ROI[i].rect.y - 40, 150, 40));
	}
}


double count_objects(const cv::Mat& foreground)
{
	for (int i=objcounts.size()-1; i>=0; i--)
	{
		Mat frgROI = foreground(objcounts[i].rect);
		Scalar s = mean(frgROI);
		objcounts[i].occupancy = .5*objcounts[i].occupancy + .5*s.val[0]/255;
//		cout<<"occupancy: "<<objcounts[i].occupancy<<", peak: "<<objcounts[i].peak;
		if (!objcounts[i].objCounted && objcounts[i].occupancy>objcounts[i].peak)
			objcounts[i].peak = objcounts[i].occupancy;
		if (objcounts[i].objCounted)
		{
			if (objcounts[i].occupancy>1.1*objcounts[i].peak)
				objcounts[i].objCounted = false;
			if (objcounts[i].occupancy < objcounts[i].peak && objcounts[i].occupancy>objcounts[i].minPeak)
				objcounts[i].peak = objcounts[i].occupancy;
		}


		if (objcounts[i].occupancy<0.9*objcounts[i].peak && objcounts[i].peak>objcounts[i].minPeak)
		{
			// Object counted!
			objcounts[i].probOfObj = objcounts[i].peak;
			objcounts[i].objCounted = true;
			objcounts[i].peak = std::max(objcounts[i].minPeak,objcounts[i].occupancy);
//			cout<<", probOfObj: "<<objcounts[i].probOfObj<<endl;
//			return probOfObj;
		}
		else
			objcounts[i].probOfObj = 0;

		objcounts[i].rate*=(1-1/fps/60);

		// Accept as count if above user's threshold (minPeak was 0.5*thresh)

		if (objcounts[i].probOfObj > 2*objcounts[i].minPeak)
			objcounts[i].rate += 1; 
//		cout<<endl;
	}
	return 0;
}


int	initColours(const char* fname)
{
	vector<string> regionNames;
	vector<int> roiData;
	FILE *fp=fopen(fname,"rt");
	if (fp==NULL)
	{
		printf("Can not open colours file %s:", fname);
		perror ("");
		return -1;
	}
	char line [500], buff[500];
	int linenum=0;

	while ( fgets ( line, sizeof(line), fp ) != NULL ) /* read a line */
	{
		linenum++;
		/* Skip comment lines*/
		if (line[0]=='#')
			continue;

		Colourstruct tmp;

		if (sscanf(line,"%lf %lf %lf %lf %lf %lf %s", 
			tmp.Hlimits, tmp.Hlimits+1, 
			tmp.Slimits, tmp.Slimits+1, 
			tmp.Vlimits, tmp.Vlimits+1, buff)!=7)
			continue;
		tmp.name=string(buff);
		definedColours.push_back(tmp);
	}
	fclose ( fp );
	unsigned int colourSize=definedColours.size();
	if (colourSize==0) return -1;
	Mat coloursHSV=Mat(1,colourSize,CV_32FC3);
	coloursRGB=Mat(1,colourSize,CV_32FC3);
	float* dataHSV = coloursHSV.ptr<float>(0);

	for (unsigned int i=0;i<colourSize;i++, dataHSV+=3)
	{
		if (definedColours[i].Hlimits[1]>definedColours[i].Hlimits[0])
			dataHSV[0]=(float) (definedColours[i].Hlimits[1]+definedColours[i].Hlimits[0])/2;
		else
			dataHSV[0]=(float) (definedColours[i].Hlimits[1]-360+definedColours[i].Hlimits[0])/2;

		//The defined colors on http://en.wikipedia.org/wiki/Web_colors have binary saturation
		if ((definedColours[i].Slimits[1]+definedColours[i].Slimits[0]) < 1)
			dataHSV[1] = 0;
		else
			dataHSV[1] = 1;

		//They also have quantized value by 0.25
		dataHSV[2]= cvRound((definedColours[i].Vlimits[1]+definedColours[i].Vlimits[0])*2)/4.0f;
	}
	cvtColor(coloursHSV,coloursRGB,CV_HSV2BGR);
	Vec3f pixel;
	for (unsigned int i=0;i<colourSize;i++)
	{
		pixel = coloursRGB.at<Vec3f>(0, i);
		definedColours[i].bgr_color=Scalar(pixel[0]*255,pixel[1]*255,pixel[2]*255,0);
	}

	return 0;
}

int getColour(const float* dataHSV)
{
	static int numColours = definedColours.size();
	bool found=false;
	int i;
	for(i=0; i<numColours; i++)
	{
		//Check H
		if (definedColours[i].Hlimits[0]<definedColours[i].Hlimits[1])
			//H does not include the start of the circle
		{
			//continue if not within Hlimits
			if (dataHSV[0]<definedColours[i].Hlimits[0] || dataHSV[0]>=definedColours[i].Hlimits[1])
				continue;
		}
		else
		{
			//Handling values close to zero degrees
			if (dataHSV[0]<definedColours[i].Hlimits[0] && dataHSV[0]>=definedColours[i].Hlimits[1])
				continue;
		}

		//Check S
		if (dataHSV[1]<definedColours[i].Slimits[0] || dataHSV[1]>=definedColours[i].Slimits[1])
			continue;

		//Check V
		if (dataHSV[2]<definedColours[i].Vlimits[0] || dataHSV[2]>=definedColours[i].Vlimits[1])
			continue;
		found = true;
		break;
	}

	if(found)
		return i;
	else
		return -1;
}

int countColours(const Mat& srcHSV, const Mat& foreground, unsigned short *colourCountFrg, unsigned short *colourCountBkg, int coulourSize)
{
	//TODO: Check that srcHSV is CV_32FC3, foreground is CV_8UC1 and that sizes are equal
    int y0 = 0, y1 = srcHSV.rows;
    int ncols = srcHSV.cols, nchannels = srcHSV.channels();

	for( int y = y0; y < y1; y++ )
	{
		const float* dataHSV = srcHSV.ptr<float>(y);
		const unsigned char* dataFrg = foreground.ptr<unsigned char>(y);
		for( int x = 0; x < ncols; x++, dataHSV += nchannels, dataFrg++ )
		{
			int colorIdx = getColour(dataHSV);
			if (colorIdx>=0 && colorIdx<coulourSize)
			{
				if (*dataFrg>0)
					colourCountFrg[colorIdx]++;
				else
					colourCountBkg[colorIdx]++;
			}
			else
				cout << format("Unknown color HSV=(%5.1f %5.3f %5.3f)\n", dataHSV[0], dataHSV[1], dataHSV[2]);
		}
	}
	return 0;
}

//Finds minimum vector value greater than given number
inline float minGreaterThan(const float* vec, unsigned int len, float value = 0)
{
	float minVal=(float) ULLONG_MAX;
	for(unsigned int i=0;i<len;i++)
		if(vec[i]>value && vec[i]< minVal)
			minVal=vec[i];
	return minVal;
}

// Assume that ROIs have already been setup and decimated
// In each ROI Read the roiImg, frgImg and calculate roi32f, roiHSV
int	calculate_colours(Mat* debugImg)
{
	int i,j;
	static Scalar zero=Scalar(0);
	static unsigned int colourSize=definedColours.size();

	// colour rate has to be decreased by 1-COLOUR_RATE within REPORT_TIME
	// i.e. in REPORT_TIME*fps/1000 frames
	double colour_rate_decr = (1-COLOUR_RATE)*1000/(REPORT_TIME*fps);

	static Mat frgScaledMat, bkgScaledMat;
	double tmpSum;

	static Mat tmpColourModel=Mat(1,colourSize,CV_32FC1);

	for (i=0;i<NREG;i++)
	{
		tmpColourModel.setTo(zero);

		for (j=REG[i].containedROIs.size()-1;j>=0;j--)
		{
			tmpSum = sum(ROI[j].frgImg)[0]/255;
			double frgRatio = tmpSum/ROI[j].area;

			// Only update model if enough foreground pixels (avoid false alarms)
			// and not too many (avoid flickering)
			if (frgRatio >= COLOUR_FRG_MIN && frgRatio <= COLOUR_FRG_MAX)
			{
				unsigned short *frgPtr = ROI[j].frgColours.ptr<unsigned short>(0);
				unsigned short *bkgPtr = ROI[j].bkgColours.ptr<unsigned short>(0);

				//Clear the previous values
				//memset(frgPtr,0,colourSize*sizeof(unsigned int));
				//memset(bkgPtr,0,colourSize*sizeof(unsigned int));
				ROI[j].frgColours.setTo(zero);
				ROI[j].bkgColours.setTo(zero);
				ROI[j].roiImg.convertTo(ROI[j].roi32f,CV_32F,1.0/255);
				cvtColor(ROI[j].roi32f,ROI[j].roiHSV, CV_BGR2HSV);
				countColours(ROI[j].roiHSV, ROI[j].frgImg, frgPtr, bkgPtr, colourSize);

				ROI[j].frgColours.convertTo(frgScaledMat,CV_32F,1/tmpSum);
				//Background pixels are total - foreground
				ROI[j].bkgColours.convertTo(bkgScaledMat,CV_32F,1/(ROI[j].area - tmpSum));

				// Attenuate effect of colours that appear 2 times stronger in the background
				// Find minimum non-zero component in bckg
				float minbkg=minGreaterThan(bkgScaledMat.ptr<float>(0),colourSize);

				float *hm = frgScaledMat.ptr<float>(0);
				float *hb = bkgScaledMat.ptr<float>(0);

				// In nonzero foreground bins where bkg[i]>model[i]/2
				// model[i]*=minbkg/bkg[i]
				for (int k=0;k<(int)colourSize;k++)
				{
					if (hm[k] > 0 && hb[k] > hm[k]/2)
						hm[k] *= minbkg/hb[k];
				}
				//normalise foreground again
				tmpSum = sum(frgScaledMat)[0];
				frgScaledMat/=tmpSum;

				//update model
				ROI[j].colourModel = (1-ROI[j].colourRate)*ROI[j].colourModel + ROI[j].colourRate * frgScaledMat;
				//if learning rate still large, just reduce it
				if (ROI[j].colourRate > COLOUR_RATE)
					ROI[j].colourRate -= colour_rate_decr;

				if (debugImg!=NULL)
				{
					rectangle(*debugImg,ROI[j].rect,CV_RGB(0,255,0));
				}
			}
			tmpColourModel+=ROI[j].regAreaRatio * ROI[j].colourModel;

//			REG[i].colourModel = REG[i].colourModel*(1-COLOUR_RATE)+ COLOUR_RATE*ROI[j].regAreaRatio * ROI[j].colourModel;
//					//else we can update the region model;
//					REG[i].colourModel += ROI[j].regAreaRatio * ROI[j].colourModel;

		}
		//normalise the region ROI
		tmpSum = sum(tmpColourModel)[0];
		if (tmpSum>0)
		{
			tmpColourModel/=tmpSum;
			REG[i].colourModel = REG[i].colourModel*(1-COLOUR_RATE)+ tmpColourModel*COLOUR_RATE;
		}
	}

	return 0;
}

int setColourParam(char *name, char *value)
{
	double data;
	if (strcmp(name,"REPORT_TIME")==0 && sscanf(value, "%lf",&data) == 1)
	{
		if (data>0)
			REPORT_TIME = cvRound(data);
		else
			printf("Invalid REPORT_TIME, leaving at default %d\n", REPORT_TIME);
	}
	else if (strcmp(name,"COLOUR_RATE")==0 && sscanf(value, "%lf",&data) == 1)
	{
		if (data>=0 && data<=1)
			COLOUR_RATE = data;
		else
			printf("Invalid COLOUR_RATE, leaving at default %lf\n", COLOUR_RATE);
	}
	else if (strcmp(name,"COLOUR_FRG_MIN")==0 && sscanf(value, "%lf",&data) == 1)
	{
		if (data>=0.01 && data<0.5)
			COLOUR_FRG_MIN = data;
		else
			printf("Invalid COLOUR_FRG_MIN, leaving at default %lf\n", COLOUR_FRG_MIN);
	}
	else if (strcmp(name,"COLOUR_FRG_MAX")==0 && sscanf(value, "%lf",&data) == 1)
	{
		if (data>=0.5 && data<0.95)
			COLOUR_FRG_MAX = data;
		else
			printf("Invalid COLOUR_FRG_MAX, leaving at default %lf\n", COLOUR_FRG_MAX);
	}
	else
			return -1;
	return 0;
}
