SMART FP7 - Search engine for MultimediA enviRonment generated contenT
Webpage: http://smartfp7.eu
------------------------------

This is a project created using Microsoft Visual Studio 2010 Express for 
Windows of a simple client capturing video frames from a webcam and processing them 
using OpenCV. Two simple statistics, mean intensity and frame difference are 
calculated and sent to the given SMART EdgeNode using CURL.
The source code is standard C/C++ code and can be also compiled in other 
environments, provided the required libraries (CURL and OpenCV 2.4.3 or later) are available
on that environment.

Contents (as of 2014-08-26):

Folders:
src           : Source files in C programming language
win32_vs2010  : Visual Studio 2010 project files for Windows

Files:
Readme.txt       : This readme file


-------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------

Simple camera perceptual component

Usage:
  simple_camera <server_address>

Parameters:
    <server_address>: Address of an existing feed on a smart edge node, where metadata will be stored
        e.g. http://dusk.ait.gr/couchdb/simple_camera_feed
        or http://localhost:5984/simple_camera_feed

Detailed info on http://opensoftware.smartfp7.eu/projects/smart/wiki/simple_camera

-------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------


Dependencies: OpenCV and CURL

OpenCV should be installed and the environmental variable OPENCV_DIR should be set

For Windows, the required libraries for CURL can be found in the "libs" folder, as shown below:

   somedirectory
    |_simple_camera
    | |_src
    | |_win32_vs2010
    | |Readme.txt
    |
    |_libs
      |_curl-7.28.0
        |_builds
        | |_libcurl-debug-static-ipv6-sspi-spnego-winssl
        | | |_lib
        | |   |libcurl_a_debug.lib
        | |_libcurl-release-static-ipv6-sspi-spnego-winssl
        |   |_lib
        |     |libcurl_a.lib
        |
        |_include
        | |_curl
        |
        |_winbuild