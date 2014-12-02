#include "tools.h"
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace std;

//Extract the name and extension from a full path
string getName (const string& str, string* ext)
{
	string name;
	//Remove the path
	unsigned found = str.find_last_of("/\\");
//	cout << " path: " << str.substr(0,found) << '\n';
//	cout << " file: " << str.substr(found+1) << '\n';
	name = str.substr(found+1);
	//Remove the extension
	found = name.find_last_of(".");
	if (ext!=NULL)
		*ext=name.substr(found+1);
	return name.substr(0,found);
}

int createFolder(const string& path)
{
	//TODO: check LINUX compatibility
	return _mkdir(path.c_str());
}

//Returns false if folder does not exist, true if it exists
bool folderExists(const string& path)
{
    struct stat info;

    if(stat( path.c_str(), &info ) != 0)
        return false;
    else if(info.st_mode & S_IFDIR)
        return true;
    else
        return false;
}