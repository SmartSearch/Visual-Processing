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


/* nkat TODO: Replace the old C interface of OpenCV with the new C++*/
#include <opencv2/core/core_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>

#include "time_funcs.h"


// Standard Size (VGA)
#define STD_WIDTH 640
#define STD_HEIGHT 480

typedef struct _outstruct
{
	double intensity;
	double framediff;
} outstruct;

typedef struct _inpstruct
{
	IplImage *frame;
	int frame_num;
} inpstruct;

int perceptual_component(const inpstruct *input,outstruct *output);
int create_message(char *buff, size_t buffmax, outstruct output);
