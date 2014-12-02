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
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

/*extension of CV_CAP_PROP_ to return current time of video capture in millis since epoch*/
#define MY_CAP_PROP_CUR_TIME_MSEC 100000 

int sequence_open(const char *input);
int sequence_open(int num);

bool sequence_isOpened();


void sequence_release();


bool sequence_read(cv::Mat &frame);
double sequence_get(int propId);
bool sequence_set(int propId, double value);
