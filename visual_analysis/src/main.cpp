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

//Remove annoying Visual Studio warnings about unsafe functions
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif //_CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <iomanip>
#include <signal.h>
#include "simple_crowd_analysis.hpp"

// Note: if linking with the static version of CURL, make sure you have #defined CURL_STATICLIB
#ifndef CURL_STATICLIB
//#define CURL_STATICLIB
#endif //CURL_STATICLIB
#include "curl_helper.h"

#include "video_seq.hpp"

//#include "ait_funcs.hpp"

// Size for character buffers
#define CHAR_BUFF_SZ 1000

#ifndef ESC //The ASCII number for Escape
#define ESC 27
#endif //ESC

/*define as C to suppress some warnings*/
extern "C"
{
//timing structure
typedef struct timeticks 
{
	int64	start;
	int64	capture;
	int64	background;
	int64	colorconvert;
	int64	crowdanalysis;
	int64	createmessage;
	int64	curlsend;
	int64	drawing;
	int64	total;
}timeticks;
}

using namespace std;

/*Global variables*/
//bool is_live=false; //Is the input from a live camera?

/* Background parameters */
int TRAIN_FRAMES=100; //How many frames to init background model, useful to reduce camera flicker at start
double TRAIN_BKG_RATE=.1; // Maximum background learning rate used at first TRAIN_FRAMES
double BASE_BKG_RATE=.0008; // Base background learning rate

/* Reporting parameters */
int POST_FREQUENCY_MSEC=60000; //Minimum time between two consecutive posts to CouchDB, disabled when <=0
int REPORT_STEP = 500; //Every how many frames to report timing statistics on the console
int DRAW_STEP = 50; //Every how many frames to draw the output window

/* Display parameters */
int SHOW_ROIS = 0; //show the ROIs on the screen
int SHOW_FOREGROUND =0; //display the foreground window
int SHOW_HELP = 1; // display help message

int SKIP_FRAMES = 0; //Number of frames to skip if conditions change due to mean intensity jump or updateRate2() detection

bool SERVER_POST = false; //Whether to post results to a server
bool QUIT = false;

double fps ; //Current fps

void my_handler(int s){
	printf("Caught signal %d\n",s);
	QUIT = true;
}

void print_times(timeticks *ticks, int64 framenum, int64 skipframes)
{
	static double tickfreq = cv::getTickFrequency();
	if (ticks==NULL)
	{
		// Print labels
		cout << setw(60) << "average time per frame (ms)" << endl;
		cout << setw(10) << "Frame";
		cout << setw(10) << "capture";
		cout << setw(10) << "bckgrnd";
		cout << setw(10) << "colrcnv";
		cout << setw(10) << "crwdanl";
		cout << setw(10) << "crtmesg";
		cout << setw(10) << "curlsnd";
		cout << setw(10) << "drawing";
		cout << setw(10) << "total";
		cout << setw(8) << "fps" << endl;
	}
	else
	{
		int64 elapsedframes=framenum-skipframes;
		cout << cv::format("\r%10ld",framenum)
			<< cv::format("%10.3f",1000*ticks->capture/(tickfreq*elapsedframes))
			<< cv::format("%10.3f",1000*ticks->background/(tickfreq*elapsedframes))
			<< cv::format("%10.3f",1000*ticks->colorconvert/(tickfreq*elapsedframes))
			<< cv::format("%10.3f",1000*ticks->crowdanalysis/(tickfreq*elapsedframes))
			<< cv::format("%10.3f",1000*ticks->createmessage/(tickfreq*elapsedframes))
			<< cv::format("%10.3f",1000*ticks->curlsend/(tickfreq*elapsedframes))
			<< cv::format("%10.3f",1000*ticks->drawing/(tickfreq*elapsedframes))
			<< cv::format("%10.3f",1000*ticks->total/(tickfreq*elapsedframes))
			<< cv::format("%8.1f",elapsedframes/(ticks->total/tickfreq)) << endl;
	}
}

// Print program usage
void printUsage()
{
	cout << "SMART FP7 - Search engine for MultimediA enviRonment generated contenT" << endl;
	cout << "Webpage: http://smartfp7.eu" << endl;
	cout << setfill('-') << setw(30) << "-" << endl;
	cout << "Visual analysis perceptual component" << endl << endl;
	cout << "Usage: " << endl << "  visual_analysis <server_address> <video_name> <configuration_file>" << endl << endl;
	cout << "Parameters:" << endl;
	cout << "    <server_address>: Address of an existing feed on a smart edge node, where metadata will be stored" << endl;
	cout << "        e.g. http://dusk.ait.gr/couchdb/simple_crowd_analysis_feed" << endl;
	cout << "        or http://localhost:5984/simple_crowd_analysis_feed" << endl;
	cout << "        use the word 'localhost' to avoid sending metadata" << endl << endl;
	cout << "    <video_name>: The name of a video to process"<< endl;
	cout << "        Provide a folder name without leading slash/backslash to process a recording from AXIS cameras" << endl;
	cout << "        Provide an rtsp:// address to open an RTSP stream (not reliable in high resolutions)" << endl;
	cout << "        Use a number (usually 0) to select the corresponding capturing device (webcam)" << endl << endl;
	cout << "    <configuration_file>: The name of a text file containing the configuration parameters"<< endl;
	cout << "        Check the supplied params*.txt for example configurations" << endl << endl;
	cout << "Detailed info on http://opensoftware.smartfp7.eu/projects/smart/wiki/simple_camera" << endl << endl;
}

int parseHeader(char *line)
{
	int state=-1;  /*0: General, 1: Regions, 2: Stauffer, 3: Colours*/
	
	if (strcmp(line,"General")==0)
		state=0;
	else if (strcmp(line,"Regions")==0)
		state=1;
	else if (strcmp(line,"Background")==0)
		state=2;
	else if (strcmp(line,"Colour")==0)
		state=3;
	if (state==-1)
		printf("Invalid header: [%s]\n", line);
	return state;
}

int setGeneralParam(char *name, char *value)
{
	int data;
	double fdata;
	/*
	if (strcmp(name,"WRITEVIDEO")==0 && sscanf(value,"%d",&data[0])==1)
		WRITEVIDEO=data[0];
	else if (strcmp(name,"numThreads")==0 && sscanf(value,"%d",&data[0])==1)
	{
		if(data[0]<=MAX_THREADS)
			numThreads=data[0];
		else
		{
			printf("Warning: numThreads=%d is larger than MAX_THREADS=%d, setting numThreads=%d\n",data[0], MAX_THREADS, MAX_THREADS); 
			numThreads=MAX_THREADS;
		}
	}
	else if (strcmp(name,"START_FRAME")==0 && sscanf(value,"%d",&data[0])==1)
		START_FRAME=data[0];
	else if (strcmp(name,"FRG_ONLY_FRAMES")==0 && sscanf(value,"%d",&data[0])==1)
		FRG_ONLY_FRAMES=data[0];
	else if (strcmp(name,"RECORDEVENTS")==0 && sscanf(value,"%d",&data[0])==1)
		RECORDEVENTS=data[0];
	else if (strcmp(name,"REPORT_STEP")==0 && sscanf(value,"%d",&data[0])==1)
		REPORT_STEP=data[0];
	else if (strcmp(name,"NCHANNELS")==0 && sscanf(value,"%d",&data[0])==1)
	{
		if(data[0]==1 || data[0]==3)
			NCHANNELS=data[0];
		else
			printf("Warning: NCHANNELS=%d is not a valid value, leaving NCHANNELS=%d\n",data[0], NCHANNELS); 
	}
	else*/
	if (strcmp(name,"SHOW_HELP")==0 && sscanf(value,"%d",&data)==1)
		SHOW_HELP=data;
	else if (strcmp(name,"POST_FREQUENCY_MSEC")==0 && sscanf(value,"%d",&data)==1)
		POST_FREQUENCY_MSEC=data;
	else if (strcmp(name,"REPORT_STEP")==0 && sscanf(value,"%d",&data)==1)
		REPORT_STEP=data;
	else if (strcmp(name,"DRAW_STEP")==0 && sscanf(value,"%d",&data)==1)
		DRAW_STEP=data;
	else if (strcmp(name,"SKIP_FRAMES")==0 && sscanf(value,"%d",&data)==1)
		SKIP_FRAMES=data;
	else if (strcmp(name,"TRAIN_FRAMES")==0 && sscanf(value,"%d",&data)==1)
		TRAIN_FRAMES=data;
	else if (strcmp(name,"TRAIN_BKG_RATE")==0 && sscanf(value,"%lf",&fdata)==1)
		TRAIN_BKG_RATE=fdata;
	else if (strcmp(name,"BASE_BKG_RATE")==0 && sscanf(value,"%lf",&fdata)==1)
		BASE_BKG_RATE=fdata;
	else
		return -1;
	return 0;
}

int	loadParams(const char* fname)
{
	vector<string> regionNames;
	vector<int> roiData;
	FILE *fp=fopen(fname,"rt");
	if (fp==NULL)
	{
		printf("Can not open parameters file %s:", fname);
		perror ("");
		return -1;
	}
	char line [500], name[500], header[500];
	int state=-1; /*0: General, 1: Rois, 2: Stauffer*/
	int ret;
	
	char *found;
	char *value;
	int linenum=0;

	while ( fgets ( line, sizeof(line), fp ) != NULL ) /* read a line */
	{
		linenum++;
		/* Skip comment lines*/
		if (line[0]=='#')
			continue;
		/* Skip lines with invalid headers */
		if (line[0]=='[' && (found=strstr(line,"]"))!=NULL)
		{
			int len = found-line+1;
			strncpy(header,line,len);
			*(header+len)='\0';
			*found='\0';
			state=parseHeader(line+1);
			continue;
		}
//		printf("%s",line);
		// Skip lines that do not contain '='
		if ( (found=strstr(line,"="))==NULL)
			continue;
		//Remove white spaces at the end of name
		char *strn_end=found-1;
		while (isspace(*strn_end) && strn_end>line)
			strn_end--;
		memcpy(name,line,strn_end-line+1); name[strn_end-line+1]='\0';
		value=found+1;
		//Remove white spaces at the end of value
		strn_end=value+strlen(value)-1;
		while (isspace(*strn_end) && strn_end>value)
			strn_end--;
		*(strn_end+1)='\0';

		if (state==0)
			ret=setGeneralParam(name,value);
		else if (state==1)
			ret=setRegionParam(name,value);
		else if (state==2)
//			ret=setForegroundParam(name,value);
			continue;
		else if (state==3)
			ret=setColourParam(name,value);
		else
			printf("invalid state %s when parsing %s in line %d\n", header, name, linenum);
		if (ret<0)
			printf("%s : Did not understand parameter %s in line %d\n", header, name, linenum);
	}
	fclose ( fp );
	return 0;
}


// Parse arguments and init video capture device and curl
int parse_init(int argc, char **argv)
{
	string paramsFile = "params_default.ini";
	// If no arguments given, print usage and return
	if (argc<2)
	{
		printUsage();
		return -1;
	}

	// If three parameters given, third  one is the file to open
	if (argc>3)
	{
		paramsFile=argv[3];
		cout << "Reading params from: " << paramsFile << endl;
	}
	// If at least two parameters given, second one is the file to open
	if (argc>2)
	{
		string test=argv[2];
		sequence_open(argv[2]);
//		if (test.length()>7 && test.compare(0,7,"rtsp://")==0)
//			is_live=true;
		cout << "Given video input: " << test << endl;
	}
	else // Second parameter not given, open default camera device
	{
		cout << "Opening default camera\n";
		sequence_open(0);
//		is_live=true;
	}

	if (!sequence_isOpened())
		return -1;

	//Initialise sending to server if provided
	if (strcmp(argv[1],"localhost")!=0)
	{
		SERVER_POST = true;
		if (curl_init(argv[1])<0)
		{
			cout << "Can not initialise CURL with the provided address: " << argv[1] << endl;
			sequence_release();
			return -1;
		}
		curl_set_debug_level(1);
	}
	return loadParams(paramsFile.c_str());
}

void drawResults(cv::InputArray _frame, cv::InputArray _frg_img, cv::InputOutputArray _disp_img, double crowd_density, string &message)
{
	cv::Mat foreground;
//	if (save_frg_image(foreground,frame_size,frame_count)<0) return -1;
	cv::Mat frame=_frame.getMat();
	_disp_img.create(frame.size(),CV_8UC3);
	cv::Mat disp_img = _disp_img.getMat();

	cv::Mat gray_img;
	cv::cvtColor(frame,gray_img,CV_BGR2GRAY);
	gray_img*=0.3;
	cv::cvtColor(gray_img,disp_img,CV_GRAY2BGR);
	disp_img+=cv::Scalar(30,0,0);
	cv::Mat frg_dec=_frg_img.getMat();
	if (frg_dec.size()!=frame.size())
	{
		// Restore foreground to original size
		cv::resize(frg_dec,foreground,frame.size(),0,0,CV_INTER_NN);
	}
	else
		foreground=frg_dec;

	frame.copyTo(disp_img,foreground);	

//	string text1=cv::format("Press ESC to stop, crowd density: %5.3lf", crowd_density)+message;
	string text1=string("Press ESC to stop")+message;
	writeColorText(disp_img,text1,cv::Point(0,frame.rows/50),cv::Scalar(0,255,0));

	if (SHOW_ROIS)
		drawROIs(disp_img);

	drawColours(disp_img,1);
	
	drawObjects(disp_img);


}

int main(int argc, char **argv)
{
   signal(SIGINT, my_handler);

//	initColours("colours.ini");
	//localhost:5984/testcrowd rtsp://infogijon4.axiscam.net:554/mpeg4/media.amp
	int framecount; //Current frame number

	cv::Mat frame; // current video frame
	cv::Mat foreground; // foreground mask, 0 means background, 255 means foreground

	vector<double> crowd_densities;
	vector<string> region_names;
	vector<double> color_freqs;
	cv::Mat color_values;

	// Read arguments, initialise capture device and curl, exit on error
	if (parse_init(argc, argv)<0) return -1;


	//get framerate of camera/video, note that some cameras return 0
	fps = sequence_get(CV_CAP_PROP_FPS);
	cv::Size imageSize = cv::Size((int)sequence_get(CV_CAP_PROP_FRAME_WIDTH),(int)sequence_get(CV_CAP_PROP_FRAME_HEIGHT));
	cout << "Video size: " << imageSize.width << "x" << imageSize.height << endl;
	if (initRegions()<0)
		return -1;
	//cv::namedWindow("frg",CV_WINDOW_NORMAL);
	//	std::vector<std::string> params;
	//	backgroundSegment.getParams(params);
	//	std::cout<<backsub.get<double>("detectShadows")<<" "<<mog.paramType("detectShadows")<<" "<<mog.get<double>("history")<<" "<< mog.paramType("history") << std::endl;

	timeticks ticks;
	ticks.capture=0;ticks.background=0;ticks.colorconvert=0;ticks.crowdanalysis=0;
	ticks.createmessage=0;ticks.curlsend=0;ticks.drawing=0;ticks.total=0;

	cout << "Press ESC in a window to stop, press ctrl+c if no window is visible" << endl;
	// cout << setfill('0'); // Add leading zeroes when printing frame numbers in standard output

	if (SKIP_FRAMES>0)
	{
		cout<<"Skipping " << SKIP_FRAMES << " frames\n";
		sequence_set(CV_CAP_PROP_POS_FRAMES,SKIP_FRAMES);
	}

	//Read TRAIN_FRAMES for startup
	while (TRAIN_FRAMES>0 && sequence_read(frame))
	{
		calculate_foreground(frame,foreground,TRAIN_BKG_RATE);
		TRAIN_FRAMES--;
		cout<<"\rInitialising frame: " << setw(5) << TRAIN_FRAMES;
		cout.flush();
	}

	double sent_message=-1;

	//Print header for times
	print_times(NULL,0,0);
	// Process all frames
	for (framecount=1;;framecount++) {
		cv::Mat disp_frame;
		int64 start_ticks;
		if (framecount%REPORT_STEP==0)
			print_times(&ticks,framecount,0);
		else if (framecount%20==0) 	// Show progress
		{
			cout<<"\rProcessing frame: " << setw(5) << framecount;
			cout.flush();
		}

		ticks.start=cv::getTickCount();
		// If not possible to get a frame, stop
		if (!sequence_read(frame) || QUIT)
			break;
		ticks.capture+=cv::getTickCount()-ticks.start;
		string message;
		start_ticks=cv::getTickCount();
		calculate_foreground(frame,foreground,BASE_BKG_RATE);
		ticks.background+=cv::getTickCount()-start_ticks;
		start_ticks=cv::getTickCount();
		calculate_crowd_density(foreground,crowd_densities,&region_names);
		//cv::Mat colframe = frame.clone();
		calculate_colours();
		count_objects(foreground);

		//if(prob>0.7)
		//	objcount++;

		ticks.crowdanalysis+=cv::getTickCount()-start_ticks;
		if (POST_FREQUENCY_MSEC>0)
		{
			double current_time = sequence_get(MY_CAP_PROP_CUR_TIME_MSEC);
//printf("time:%lf\n",current_time);
			if (current_time - sent_message > POST_FREQUENCY_MSEC)
			{
				start_ticks=cv::getTickCount();
				message=create_message((long long)current_time,crowd_densities[0]);
				ticks.createmessage+=cv::getTickCount()-start_ticks;
				start_ticks=cv::getTickCount();
				if (SERVER_POST)
					curl_send(message.c_str(),message.length());
				ticks.curlsend+=cv::getTickCount()-start_ticks;
				sent_message = current_time;
//				FILE* fp=fopen("test1.json","wt");
//				fprintf(fp,message.c_str());
//				fclose(fp);
				cout << message;
			}
		}
		start_ticks=cv::getTickCount();
		if(framecount%DRAW_STEP==0)
		{
			char dateTime[30];
			millis2string((long long)sequence_get(MY_CAP_PROP_CUR_TIME_MSEC),dateTime,30);
			message=cv::format(", frame#=%d, %s", framecount, dateTime);
			drawResults(frame,foreground,disp_frame,crowd_densities[0],message);

			if (DRAW_STEP>0)
			{
				cv::namedWindow("Crowd Density Output",CV_WINDOW_NORMAL);
				cv::imshow("Crowd Density Output",disp_frame);
			}
			else
			{
				cv::imwrite(cv::format("frames/frame%08d.png", framecount),disp_frame);
			}
		}
		ticks.drawing+=cv::getTickCount()-start_ticks;
		ticks.total+=cv::getTickCount()-ticks.start;
		if (DRAW_STEP>0)
		{
			char keypressed=(char)cv::waitKey(10);
			if (keypressed == 'q' || keypressed == 'Q' || keypressed == ESC )
				break;
			else if (keypressed == 'r' || keypressed == 'R')
				SHOW_ROIS=(SHOW_ROIS+1)%2; //Toggle showing the rois
			else if (keypressed == 'h' || keypressed == 'H')
				SHOW_HELP=(SHOW_HELP+1)%2; //Toggle showing help
			else if (keypressed == 'e' || keypressed == 'E')
			{
				SHOW_FOREGROUND=(SHOW_FOREGROUND+1)%2; //Toggle the evidence window
				if (SHOW_FOREGROUND)
					cv::namedWindow("Foreground",CV_WINDOW_NORMAL);
				else
					cv::destroyWindow("Foreground");
			}
		}
	}

	sequence_release();
	if (SERVER_POST)
		curl_cleanup();
	if (DRAW_STEP > 0)
		cv::destroyAllWindows();
	print_times(&ticks,framecount,0);
#ifdef WIN32
	//Pause if run within Visual Studio
	if (IsDebuggerPresent())
	{
		cout << "Press Enter to exit\n";
		getchar();
	}
#endif //WIN32
}
