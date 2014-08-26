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

#include <iostream>
#include <iomanip>


/* nkat TODO: Replace the old C interface of OpenCV with the new C++*/
#include "perceptual_component.hpp"

/* If using the static linked version of curl, make sure you have CURL_STATICLIB defined */
#include "curl_helper.h"

using namespace std;

void printUsage()
{
	cout << "SMART FP7 - Search engine for MultimediA enviRonment generated contenT" << endl;
	cout << "Webpage: http://smartfp7.eu" << endl;
	cout << setfill('-') << setw(30) << "-" << endl;
	cout << "Simple camera perceptual component" << endl << endl;
	cout << "Usage: " << endl << "  simple_camera <server_address>" << endl << endl;
	cout << "Parameters:" << endl;
	cout << "    <server_address>: Address of an existing feed on a smart edge node, where metadata will be stored" << endl;
	cout << "        e.g. http://dusk.ait.gr/couchdb/simple_camera_feed" << endl;
	cout << "        or http://localhost:5984/simple_camera_feed" << endl << endl;
	cout << "Detailed info on http://opensoftware.smartfp7.eu/projects/smart/wiki/simple_camera" << endl << endl;
}

int main(int argc, char *argv[])
{
	CvCapture *cap; 
	char c=0;
	outstruct output;
	inpstruct input;
	char buff[500];
	int datalen;

	// If no arguments given, print usage and return
	if (argc<2)
	{
		printUsage();
		exit(-1);
	}
	cap = cvCaptureFromCAM(0); // 0 is the default webcam

	if (cap == NULL)
	{
		printf("Error opening camera\n");
		return -1;
	}

	if (curl_init(argv[1])<0)
	{
		fprintf(stderr,"Could not initialise curl with address %s", argv[1]);
		return -1;
	}

	curl_set_debug_level(0);

	input.frame = cvQueryFrame(cap);
	input.frame_num = 1;
	//Call the perceptual_component for the first time to initialise
	perceptual_component(&input,NULL);

	// Press "q" or ESCAPE to exit
	// also stop if error getting frame
	while (c!='q' && c!= 'Q' && c!=27/*ESC*/ 
		&& (input.frame = cvQueryFrame(cap))!=NULL)
	{
		input.frame_num++;
		perceptual_component(&input,&output);
		if (input.frame_num%10==0) printf("Frame %5d: Average Intensity %7.3lf, Average Frame-by-Frame Difference %6.3lf\n", input.frame_num, output.intensity, output.framediff);
		/* Send message only if created successfully*/
		if ((datalen=create_message(buff,sizeof(buff),output))>0)
		{
			/* Stop at the first error */
			if (curl_send(buff, datalen)!=CURLE_OK)
			{
				int tmp = curl_show_error();
					printf("\nData was: \n%s\n", buff);
				break;
			}
			else
				curl_show_warning();
		}
		c = (char) cvWaitKey(10); 	
	}

	/* Release the perceptual component */
	perceptual_component(NULL,NULL);
	cvReleaseCapture(&cap);

	return 0;
}

