#include <string>
#include <direct.h>

#ifdef WIN32
#include <io.h>
#endif

#pragma once

inline int filelength(const char *file_name)
{
  struct stat st;
  if (stat(file_name, &st))
    return -1;
  
  return (int)st.st_size;
}

inline bool DoesFileExist(const char* file_name)
{
#ifdef WIN32
	return _access(file_name, 0) ? false : true;
#else
	return access(file_name, 0) ? false : true;
#endif
}

#ifndef WIN32
inline bool DeleteFile(const char* file_name)
{
	if (DoesFileExist(file_name) == false)
		return true;
		
	return unlink(file_name) ? false : true;
}
#endif

inline bool make_unique_filename (std::string &unique_file_name, const char *file_name)
{
	for (int i=0; i<99; i++)
	{
		char ext[4];
		sprintf(ext,".%02d",i);
		unique_file_name = file_name;
		unique_file_name += ext;
		if (DoesFileExist (unique_file_name.c_str()) == false)
			return true;
	}
	
	return false;
}

inline bool CreateDirectoryIfNecessary(const char* directory)
{
	struct stat st;	
	if (stat(directory, &st))
	{
#ifdef WIN32
		if (_mkdir(directory) == 0)
#else
		if (mkdir(directory) == 0)
#endif
			return true;
		else
			return false;
	}

	if (st.st_mode != S_IFDIR)
		return false;

	return true;
}
