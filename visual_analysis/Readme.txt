SMART FP7 - Search engine for MultimediA enviRonment generated contenT
Webpage: http://smartfp7.eu
------------------------------

This is a project created using Microsoft Visual Studio 2012 of a visual analysis
component that uses OpenCV to process video frames from a webcam or a video file.

A visual density metric is calculated and sent to a local or remote CouchDB database
using CURL.

The source code is standard C/C++ code and can be compiled in Linux by running the 
'compile.sh' shell script.

Contents (as of 2014-11-28):

Folders:
bin           : Windows executables created by Visual Studio will be stored here
                Additionally, this folder contains required initialisation files, 
                along with a few sample configuration files
src           : Source files in C programming language
vstudio       : Visual Studio project files for Windows

Files:
Readme.txt    : This readme file

---------------------------------

Requirements:
-------------
1. To successfully compile the project, you should also download the 'common' folder
that contains source code shared between multiple projects.

2. If you are using Visual Studio, you should additionally retrieve the 'libs' folder

-------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------

Visual analysis perceptual component

Usage:
  visual_analysis <server_address> <video_name> <configuration_file>

Parameters:
    <server_address>: Address of an existing feed on a smart edge node, where metadata will be stored
        e.g. http://dusk.ait.gr/couchdb/simple_crowd_analysis_feed
        or http://localhost:5984/simple_crowd_analysis_feed
        use the word 'localhost' to avoid sending metadata

    <video_name>: The name of a video to process
        Provide a folder name without leading slash/backslash to process a recording from AXIS cameras
        Provide an rtsp:// address to open an RTSP stream (not reliable in high resolutions)
        Use a number (usually 0) to select the corresponding capturing device (webcam)

    <configuration_file>: The name of a text file containing the configuration parameters
        Check the supplied params*.txt for example configurations

Detailed info on http://opensoftware.smartfp7.eu/projects/smart/wiki/simple_camera
-------------------------------------------------------------------------------------


Required libraries: OpenCV and CURL

In order to compile simple_crowd_analysis using Visual Studio on Windows follow the steps below:

OpenCV: 
1. Download the complete version of OpenCV for Windows from http://opencv.org/ and extract it
2. Create the OPENCV_DIR enviromental variable by opening a command window and typing
      setx -m OPENCV_DIR D:\OpenCV\Build\x86\vc10  (use the path where you extracted the library)
3. Add %OPENCV_DIR%\bin to your PATH variables

More details can be found at http://docs.opencv.org/trunk/doc/tutorials/introduction/windows_install/windows_install.htm

Curl:
CURL is the other required library, a precompiled version for Windows is included
in the 'libs' package that should be downloaded separately and placed along with
the provided project as shown below:

   somedirectory
    |_visual_analysis
    | |_bin
    | |_src
    | |_vstudio
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

If you want to compile curl yourself, you can follow these instructions:

1. Download (http://curl.haxx.se/download.html) and extract the source archive in the "libs" directory

2. In order to compile curl and create the proper builds directories, you should follow the instructions in curl-X.XX.X\winbuild\BUILD.WINDOWS.txt

3. Specifically, you need to open a Visual Studio Command Prompt, cd to the winbuild directory of Curl sources 
and run the following commands, replacing VC=10 with the appropriate version of your Visual Studio:
(10 for VS2010, 11 for VS2012, 12 for VS2013)

Create the debug version:
nmake Makefile.vc mode=static VC=10 DEBUG=yes

Create the release version:
nmake Makefile.vc mode=static VC=10 DEBUG=no
