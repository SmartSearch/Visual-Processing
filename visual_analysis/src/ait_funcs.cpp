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

//Internal functions, at least for now
#include "ait_funcs.hpp"

// Draw text into an image. Defaults to top-left-justified text, but you can give negative x coords for right-justified text,
// and/or negative y coords for bottom-justified text.
// Returns the bounding rect around the drawn text.
//
// This text drawing function is based on WebcamFaceRec.cpp, by Shervin Emami (www.shervinemami.info)
// Ch8 of the book "Mastering OpenCV with Practical Computer Vision Projects"
// Full source code available at https://github.com/MasteringOpenCV/code
//
cv::Rect drawString(cv::Mat img, std::string text, cv::Point coord, cv::Scalar color, float fontScale = 0.6f, int thickness = 1, int fontFace = cv::FONT_HERSHEY_COMPLEX)
{
	// Get the text size & baseline.
	int baseline=0;
	cv::Size textSize = cv::getTextSize(text, fontFace, fontScale, thickness, &baseline);
	baseline += thickness;

	// Adjust the coords for left/right-justified or top/bottom-justified.
	if (coord.y >= 0) {
		// Coordinates are for the top-left corner of the text from the top-left of the image, so move down by one row.
		coord.y += textSize.height;
	}
	else {
		// Coordinates are for the bottom-left corner of the text from the bottom-left of the image, so come up from the bottom.
		coord.y += img.rows - baseline + 1;
	}
	// Become right-justified if desired.
	if (coord.x < 0) {
		coord.x += img.cols - textSize.width + 1;
	}

	// Get the bounding box around the text.
	cv::Rect boundingRect = cv::Rect(coord.x, coord.y - textSize.height, textSize.width, baseline + textSize.height);

	// Draw anti-aliased text.
	cv::putText(img, text, coord, fontFace, fontScale, color, thickness, CV_AA);

	// Let the user know how big their text is, in case they want to arrange things.
	return boundingRect;
}

//Rescales the given image to original_size and saves it as "frgXXXXXX.png", XXXX=framecount
int save_frg_image(cv::InputArray _image, cv::Size original_size, int framecount)
{
	const int FRAME_SAVE = 50; // Every how many frames to save output, set to 0 to disable saving
	const int START_SAVE = 3000; // When to start saving images
	const int END_SAVE = 3000; // When to stop saving images and exit

	if(FRAME_SAVE<=0 || framecount%FRAME_SAVE!=0 || framecount>=START_SAVE)
		return 1;
	if(framecount > END_SAVE)
		return -11;

	//Parameters for saving output images
	char buff[1000]; //Temporary character buffer for sprintf
	cv::Mat image = _image.getMat();
//	cv::Size original_size=_image.size();
	cv::Rect top = cv::Rect(0,0,original_size.width,(int)(original_size.height*0.0973));
	cv::Rect bottomRight = cv::Rect(original_size.width/2,(int)(original_size.height*0.6056),
		original_size.width/2,(int)(original_size.height*0.3945));
	cv::Mat saveframe; // image for saving results

	//resize to original dimension
	cv::resize(image,saveframe,original_size,0,0,CV_INTER_LINEAR);
	//remove non-processed regions of crowdanalysis
	saveframe(top).setTo(0);
	saveframe(bottomRight).setTo(0);
	sprintf(buff,"frg%06d.png",framecount);
	cv::imwrite(buff,saveframe);
	return 0;
}