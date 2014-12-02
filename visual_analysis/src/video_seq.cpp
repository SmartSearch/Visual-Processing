#include "video_seq.hpp"

#ifdef __linux__
#include <dirent.h>
/*The below defines may be needed for unix stat64*/
//#define _FILE_OFFSET_BITS 64
//#define __USE_LARGEFILE64
//#define _LARGEFILE_SOURCE
//#define _LARGEFILE64_SOURCE
#else /*not __linux__*/
#include "dirent.h" // Updated versions can be found at http://softagalleria.net/dirent.php
#define stat64 _stat64 //stat64 is not defined in Windows
#endif /*__linux__*/
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>
#include <fstream>

#include <vector>
#include <cstring>
#include <algorithm>

#include <time.h>

#include "time_funcs.h" // For converting strings to millisec and vice versa


using namespace std;

//DIR *baserecordingdir=NULL, *secondrecordingdir=NULL;

vector<string> dirList, fileList;
int dirIdx=-1, fileIdx=-1;
string inputpath, curDir, curFile;
long long videostart=-1;
long long videoDuration=-1;
double frameDuration=-1;

bool is_live=false;

// Capture device, can open camera or video
cv::VideoCapture capture;

//Returns file and directory lists of the contents of input path,
//you can use NULL for fileList or directoryList to skip that output
int create_list(const char *inputPath, vector<string> *fileList, vector<string> *directoryList, bool doSort=true, bool append=false)
{
	DIR *givendir=opendir( inputPath );
	//Error if not able to open the path
	if (givendir==NULL)
		return -1;

	if (!append && fileList!=NULL)
		fileList->clear();
	if (!append && directoryList!=NULL)
		directoryList->clear();

	struct dirent *direntp;
	struct stat64 currstat;
	string fullpath;

	while((direntp = readdir( givendir )) !=NULL)
    {
		if (strcmp(direntp->d_name,".")==0 || strcmp(direntp->d_name,"..")==0)
			continue;
    	fullpath=string(inputPath)+"/"+string(direntp->d_name);
		if (stat64(fullpath.c_str(),&currstat)!=0)
   		{
    		cout << "create_list: Could not access " << fullpath << endl;
    		continue;
   		}
		//Add directories into directoryList
		if (directoryList!=NULL && S_ISDIR(currstat.st_mode))
			directoryList->push_back(fullpath);
		//and files into fileList
		else if (fileList!=NULL && S_ISREG(currstat.st_mode))
			fileList->push_back(fullpath);
		else //Skip others
    		continue;
    }
	if (doSort && directoryList!=NULL)
		std::sort(directoryList->begin(),directoryList->end());
	if (doSort && fileList!=NULL)
		std::sort(fileList->begin(),fileList->end());
	return 0;
}

/* parse very simple xml tags that are not nested
	returns 0 if success and the parsed value is stored in result
	-1 if missing starting tag, -2 if missing ending tag
*/
int xml_parse(const string &input, const string &tag, string &result)
{
	size_t tagStart = input.find("<"+tag+">")+tag.length()+2;
	if (tagStart==string::npos)
		return -1;
	size_t tagEnd = input.substr(tagStart,string::npos).find("</"+tag+">");
	if (tagEnd==string::npos)
		return -2;
	result=input.substr(tagStart,tagEnd);
	return 0;
}


void sequence_release()
{
	if (capture.isOpened())
		capture.release();
}

int sequence_open(int num)
{
	capture.open(num);
	// check if capture device has been successfully opened
	if (!capture.isOpened())
	{
		cout << "Can not open capture device. Check that a webcam is connected\n";
		return -1;
	}
	videostart=getMillis();
	is_live=true;
	return 0;
}

int open_video_file(const string &filename)
{
	long long stopmils;

	//Try to open the relevant xml
	string xmlName=filename.substr(0,filename.length()-3)+"xml";
	ifstream myfile;
	myfile.open(xmlName.c_str());
	if (myfile.is_open())
	{
		string line, startTime, stopTime;
		getline (myfile,line);
		myfile.close();
		if (xml_parse(line,"StartTime",startTime)!=0)
			return -1;
		if (xml_parse(line,"StopTime",stopTime)!=0)
			return -2;
		string2millis(startTime.c_str(),&videostart);
		string2millis(stopTime.c_str(),&stopmils);
		videoDuration=stopmils-videostart;
	}

	capture.open(filename);
	if (!capture.isOpened())
		return -1;
	//double trueFps=1000.0*frameCount/videoduration;
	if (videoDuration==-1)
	{
		videoDuration=cvRound(capture.get(CV_CAP_PROP_FRAME_COUNT)*1000/capture.get(CV_CAP_PROP_FPS));
		frameDuration=1000/capture.get(CV_CAP_PROP_FPS);
		videostart=getMillis();
	}
	else
		frameDuration=(double)videoDuration/cvRound(capture.get(CV_CAP_PROP_FRAME_COUNT));

	return 0;
}

int sequence_open(const char *input)
{
	struct stat64 buf;

    //Save the input to inputpath string for future directory listings
	inputpath=input;

	//If the given name starts with rtsp://, open the address
	if (inputpath.length()>7 && inputpath.compare(0,7,"rtsp://")==0)
	{
		capture.open(inputpath);
		if (!capture.isOpened())
		{
			cout << "Can not open input address" << endl;
			return -1;
		}
		else
		{
			cout << "RTSP stream detected, there may be delays/dropped frames" << endl;
			is_live=true;
			videostart=getMillis();
			return 0;
		}
	}
	else if (inputpath.length()<3)
	{
//		//Try to see if it is a number
//		int inp=atoi(input);
//		if (inp!=0)
		if (strcmp(input,"0")==0)
			capture.open(0);
			if (!capture.isOpened())
			{
				cout << "Can not open input address" << endl;
				return -1;
			}
			else
			{
				cout << "Camera opened" << endl;
				capture.set(CV_CAP_PROP_FRAME_WIDTH,1920);
				capture.set(CV_CAP_PROP_FRAME_HEIGHT,1080);
				is_live=true;
				videostart=getMillis();
				return 0;
			}
	}

	//Otherwise it should be a normal file or folder, check the stats
   if(stat64( input,&buf ) != 0 )
   {
      cout << "Problem getting stat information, ";
#ifdef WIN32
      switch (errno)
      {
         case ENOENT:
           cout << "file " << input << " not found.\n";
           break;
         case EINVAL:
           cout << "invalid parameter to stat64.\n";
           break;
         default:
           /* Should never be reached. */
           cout << "unexpected error in stat64.\n";
      }
#else
      cout << "unexpected error in stat64.\n";
#endif /*WIN32*/
	  return -1;
   }

	//If it is a regular file
    if (S_ISREG(buf.st_mode))
    {
    	//And large enough to be a video
    	if (buf.st_size > 51200 /*50KB*/)
    	{
//			open_video_file(inputpath);
//    		capture.open(inputpath);
    		if (open_video_file(inputpath)<0)
    		{
    			cout << "Can not open given video, check that the appropriate codecs have been installed and ffmpeg is available in OpenCV" << endl;
    			return -1;
    		}
//			videostart=getMillis();
			return 0;
    	}
    	else
    		cout << "Input file " << input << " is too small, are you sure it contains video?\n";
    		return -1;
    }
    //Or a symbolic link / named pipe on Unix
    else if (S_ISFIFO(buf.st_mode) || S_ISLNK(buf.st_mode))
    {
//   		capture.open(inputpath);
   		if (open_video_file(inputpath)<0)
   		{
   			cout << "Can not open given video, check that the appropriate codecs have been installed and ffmpeg is available in OpenCV" << endl;
   			return -1;
    	}
		videostart=getMillis();
    	return 0;
    }
    //If nothing of the above it should be a directory, issue error if unknown input
    else if (!S_ISDIR(buf.st_mode))
    {
		cout << "Invalid file type of input: " << input << endl;
    	return -1;
    }

    //List the contents of the input directory
    create_list(input,NULL,&dirList);
    for(dirIdx=0;dirIdx<(int)dirList.size();dirIdx++)
    {
        create_list(dirList[dirIdx].c_str(),&fileList,NULL);
   		for(fileIdx=0;fileIdx<(int)fileList.size();fileIdx++)
   		{
			//skip xml files
			if (fileList[fileIdx].substr(fileList[fileIdx].length()-3,fileList[fileIdx].length()-1).compare("xml")==0)
				continue;
			if (open_video_file(fileList[fileIdx])==0)
				break;
    	}
  		if (capture.isOpened())
  			break;
    }
    if (capture.isOpened())
    {
		curDir=dirList[dirIdx];
		curFile=fileList[fileIdx];
		return 0;
    }
    else
    {
   		cout << "Could not find any files in subdirectories of " << input << endl;
        	return -1;
    }
}

int open_next()
{
	if(dirIdx<0) 	// No directory open
		return -1;
	fileIdx++;
	while(dirIdx<(int)dirList.size())
	{
		//If there are more files in list, just open those
   		while(fileIdx<(int)fileList.size())
   		{
			//skip xml files
			if (fileList[fileIdx].substr(fileList[fileIdx].length()-3,fileList[fileIdx].length()-1).compare("xml")==0)
			{
				fileIdx++;
				continue;
			}
			cout << "\nOpening" << fileList[fileIdx] << endl;
			int ret=open_video_file(fileList[fileIdx]);
			if (ret==0)
			{
				curFile=fileList[fileIdx];
				return 0;
			}
			else if (ret==-2)
			{
				//Recording is in progress in that file, wait with timeout to finish
				double waitTimeout=3600;
				clock_t start_clocks = clock();
				cout << "Waiting for recording to finish for   " << waitTimeout << " seconds";
				while (1)
				{
					ret=open_video_file(fileList[fileIdx]);
					if (ret==0)
					{
						curFile=fileList[fileIdx];
						break;
					}
					double remain = waitTimeout - (clock() - start_clocks)/(double)CLOCKS_PER_SEC;
					cout << cv::format("\b\b\b\b\b\b\b\b\b\b\b\b\b\b%6.1lf seconds", remain);
					if (remain<=0)
						break;
				}
				if (ret==0)
				{
					//A recording has finished, there are new files, update file list/index
					create_list(dirList[dirIdx].c_str(),&fileList,NULL);
					fileIdx=std::find(fileList.begin(),fileList.end(),curFile)-fileList.begin();
					return 0;
				}
				else
					return -1;
			}

			if (fileIdx+1>=(int)fileList.size())
			{
				//Finished this folder, recheck as sometimes a file was created while processing the last one
				string lastFile=fileList[fileIdx];
				create_list(dirList[dirIdx].c_str(),&fileList,NULL);
				fileIdx=std::find(fileList.begin(),fileList.end(),lastFile)-fileList.begin();
				//If no longer last in the list, restart the loop
				if (fileIdx+1<(int)fileList.size())
				{
					fileIdx++;
					continue;
				}
			}
			fileIdx++;
		}



		//if there are more folders, just go to the next one
		if (dirIdx+1<(int)dirList.size())
		{
			create_list(dirList[++dirIdx].c_str(),&fileList,NULL);
			fileIdx=0;
			continue;
		}
		//Else we have reached the end, list again
		string lastPath=dirList[dirIdx];
		create_list(inputpath.c_str(),NULL,&dirList);
		dirIdx=std::find(dirList.begin(),dirList.end(),lastPath)-dirList.begin();
		//If that is still the last available path, we have finished
		if (dirIdx+1>=(int)dirList.size())
			return -1;
		else
		{
			//List the next directory and restart the loop
			create_list(dirList[++dirIdx].c_str(),&fileList,NULL);
			fileIdx=0;
			continue;
		}
	}

	//Should not reach here but avoid compiler warnings
	return -1;
}

bool sequence_read(cv::Mat &frame)
{
	if (!capture.isOpened())
		return false;
	if (capture.read(frame))
		return true;

	// Else try to open next file in sequence
	capture.release();

	if (open_next()==0)
		return capture.read(frame);
	else return false;
}

bool sequence_isOpened()
{
	return capture.isOpened();
}

double calculateFps(int countFrames)
{
	cv::Mat frame;
	//Get a few frames to initialise before timing
	for (int i=0;i<5;i++)
		capture.read(frame);
	int64 start_time = cv::getTickCount();
	for (int i=0;i<countFrames;i++)
		capture.read(frame);
	double exec_time = (cv::getTickCount() - start_time)/cv::getTickFrequency();
	return countFrames/exec_time;
}


double sequence_get(int propId)
{
	//sometimes framerate is invalid, estimate it
	if (propId==CV_CAP_PROP_FPS)
	{
		double fps = capture.get(CV_CAP_PROP_FPS);
		if (fps<=0 || fps> 200)
		{
			cout << "Estimating camera frame rate, please wait ...";
			fps=calculateFps(10);
			cout << " Done!, frame rate is " << fps << endl;
		}
		else
			cout << "Camera returned fps=" << fps << endl;
		return fps;
	}

	if (propId==MY_CAP_PROP_CUR_TIME_MSEC)
	{
		if (is_live)
			return (double) getMillis();
		else
			return (videostart + frameDuration*capture.get(CV_CAP_PROP_POS_FRAMES));
	}
	else
		return capture.get(propId);
}

bool sequence_set(int propId, double value)
{
	return capture.set(propId,value);
}
