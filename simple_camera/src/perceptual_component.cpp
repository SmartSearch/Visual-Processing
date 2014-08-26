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

#include "perceptual_component.hpp"

/* nkat TODO: Replace the old C interface of OpenCV with the new C++*/
int perceptual_component(const inpstruct *input,outstruct *output)
{
	CvScalar avgIntensity, avgAbsDiff;
	static CvSize stdSize = cvSize(STD_WIDTH, STD_HEIGHT);
	static IplImage *frameStd, *grayscaleOld, *grayscaleNew, *absDiff;
	static bool initialised=false;

	//If input is null, release internal images/data and return
	if (input==NULL)
	{
		cvReleaseImage(&frameStd);
		cvReleaseImage(&grayscaleOld);
		cvReleaseImage(&grayscaleNew);
		cvReleaseImage(&absDiff);
		cvDestroyAllWindows();
		initialised = false;
		return 1;
	}

	/* Perform initialization the first time that the function is called*/
	if (!initialised)
	{
		cvNamedWindow("Camera");
		frameStd = cvCreateImage(stdSize, IPL_DEPTH_8U, 3);
		grayscaleOld = cvCreateImage(stdSize, IPL_DEPTH_8U, 1);
		grayscaleNew = cvCreateImage(stdSize, IPL_DEPTH_8U, 1);
		absDiff = cvCreateImage(stdSize, IPL_DEPTH_8U, 1);
		cvResize(input->frame, frameStd);
		cvConvertImage(frameStd, grayscaleOld);
		initialised=true;
		return 0;
	}
	
	cvResize(input->frame, frameStd);
	cvConvertImage(frameStd, grayscaleNew);
	cvShowImage("Camera", grayscaleNew);
	if (output!=NULL)
	{
		avgIntensity = cvAvg(grayscaleNew);
		cvAbsDiff(grayscaleOld, grayscaleNew, absDiff);
		avgAbsDiff = cvAvg(absDiff);
		cvCopy(grayscaleNew, grayscaleOld);
		output->intensity=avgIntensity.val[0];
		output->framediff=avgAbsDiff.val[0];
	}
	return 0;
}

int create_message(char *buff, size_t buffmax, outstruct output)
{
	const char direct_interface_sample[]="{\n"
		"\t\"_id\" : \"%s\",\n"
		"\t\"timestamp\" : %lld,\n"
		"\t\"data\" : {\n"
		"\t\t\"time\" : \"%s\",\n"
		"\t\t\"camera\" : {\n"
		"\t\t\t\"@ID\" : \"my_webcam\",\n"
		"\t\t\t\"mean_intensity\" : %.4llf\n"
		"\t\t},\n"
		"\t\t\"visual_processing\" : {\n"
		"\t\t\t\"@ID\" : \"my_webcam_processing\",\n"
		"\t\t\t\"frame_difference\" : %.4llf\n"
		"\t\t}\n"
		"\t}\n"
		"}\n";

	char _id[25]=""; /* Document ID */
	long long timestamp; /* Timestamp for data */
	/* Check that the given length can hold the resulting message */
	if (buffmax<strlen(direct_interface_sample)+150)
		return -1;

	timestamp = getMillis();
	
	/* Set the document ID to the xml dateTime string */
	millis2string(timestamp, _id, sizeof(_id));
	/* Also set the time attribute to the xml dateTime string */
	return sprintf(buff,direct_interface_sample,_id,timestamp,_id,output.intensity,output.framediff);
}
