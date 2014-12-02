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

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/background_segm.hpp>
#include <vector>

#include "time_funcs.h"

/* Create a message for sending the crowd density to the direct CouchDB 
interface of a SMART Edge Node, based on the following sample.
The provided crowd density is inserted in the message and the 
timestamps are updated to the current ones.

{
	"_id" : "2013-03-13T18:17:20.000Z",
	"timestamp" : 1363198640000,
	"data" :
	{
		"time" : "2013-03-13T18:17:20.000Z",
		"crowd" :
		{
			"@ID" : "crowd1",
			"density" : 0.430
		}
	}
}
 */
//int create_message(char *buff, size_t buffmax, double crowd_density);

//std::string create_message(double crowd_density, bool is_live);
std::string create_message(long long timestamp, double crowd_density);

// Calculate foreground mask with given learning rate
// in the resulting foreground mask, 0 means background, 255 means foreground
void calculate_foreground(cv::InputArray _image, cv::OutputArray _frg_img, double rate);

// Calculate the crowd density of the given foreground mask
double calculate_crowd_density(cv::InputArray _foreground);
int calculate_crowd_density(cv::InputArray _foreground, std::vector<double> &densities, std::vector<std::string> *names);

// Call this after calculate_foreground has been called;
int	calculate_colours(cv::Mat* debugImg = NULL);

// Call first to parse "region_name" and "proc_rect" parameters
int setRegionParam(char *name, char *value); 
int initRegions(); // Then run initRegions to initialise the ROIs and processing zones

void drawROIs(cv::Mat &dispImg); //Draw the reporting regions and processing ROIS

// draw colour models, optionmask==1 draw regions, optionmask==2 draw rois, 1|2 draws both
void drawColours(cv::Mat &dispImg, int optionmask=1);

void drawObjects(cv::Mat &dispImg); //Draw the object rate

//write coloured text with outline effect, offset is the top-left corner
void writeColorText(cv::Mat& image, std::string text, cv::Point offset=cv::Point(), cv::Scalar color=CV_RGB(0,255,0), double fontScale=0.55);


double count_objects(const cv::Mat& foreground);

int setColourParam(char *name, char *value);
int	initColours(const char* fname);
