SMART FP7 - Search engine for MultimediA enviRonment generated contenT
Webpage: http://smartfp7.eu
------------------------------

This is the compiled version created using Microsoft Visual Studio 2010 Express for 
Windows of a simple client capturing video frames from a webcam or a video file. These
frames are processed using OpenCV and a crowd density metric is calculated and sent to 
the given SMART EdgeNode using CURL.

Prerequisites:
If the executable does not start, please make sure that the 
Microsoft Visual C++ 2010 SP1 Redistributable (or later version)
(current link: http://www.microsoft.com/en-us/download/details.aspx?id=8328)
is installed on your computer.

Contents (as of 2013-07-11):

Files:
opencv_core246.dll        : Required OpenCV 2.4.6 library
opencv_highgui246.dll     : Required OpenCV 2.4.6 library
opencv_imgproc246.dll     : Required OpenCV 2.4.6 library
opencv_video246.dll       : Required OpenCV 2.4.6 library
Readme.txt                : This readme file
simple_crowd_analysis.exe : The compiled executable

Note:
If you have already installed OpenCV 2.4.6 on your computer and added the 
<opencv-2.4.6-install-path>\build\x86\vc10\bin folder to your system path,
the opencv_*.dll files are not required.

-------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------

Simple crowd analysis perceptual component

Usage:
  simple_crowd_analysis <server_address> (video_name)

Parameters:
    <server_address>: Address of an existing feed on a smart edge node, where metadata will be stored
        e.g. http://dusk.ait.gr/couchdb/simple_crowd_analysis_feed
        or http://localhost:5984/simple_crowd_analysis_feed

Detailed info on http://opensoftware.smartfp7.eu/projects/smart/wiki/simple_crowd_analysis

-------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------
