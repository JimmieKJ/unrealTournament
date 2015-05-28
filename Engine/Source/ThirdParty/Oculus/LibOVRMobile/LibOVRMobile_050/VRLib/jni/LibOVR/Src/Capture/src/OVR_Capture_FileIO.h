/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_Capture_FileIO.h
Content     :   Misc File IO functionality....
Created     :   January, 2015
Notes       :   Will be replaced by OVR Kernel versions soon!

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_FILEIO_H
#define OVR_CAPTURE_FILEIO_H

#include <OVR_Capture_Config.h>

// Using POSIX file handles improves perf by about 2x over stdio's fopen/fread/fclose...
#define OVR_CAPTURE_USE_POSIX_FILES 1

#if OVR_CAPTURE_USE_POSIX_FILES
    #include <fcntl.h>  // for posix IO
    #include <stdlib.h> // for atoi
#else
    #include <stdio.h>  // for std IO
#endif


namespace OVR
{
namespace Capture
{

#if OVR_CAPTURE_USE_POSIX_FILES
    typedef int FileHandle;
    static const FileHandle NullFileHandle = -1;
#else
    typedef FILE* FileHandle;
    static const FileHandle NullFileHandle = NULL;
#endif

    FileHandle OpenFile(const char *path, bool writable=false);
    void       CloseFile(FileHandle file);
    int        ReadFile(FileHandle file, void *buf, int bufsize);
    int        WriteFile(FileHandle file, const void *buf, int bufsize);

    bool       CheckFileExists(const char *path);

    int        ReadFileLine(const char *path, char *buf, int bufsize);

    int        ReadIntFile(FileHandle file);
    int        ReadIntFile(const char *path);

    void       WriteIntFile(FileHandle  file, int value);
    void       WriteIntFile(const char *path, int value);

} // namespace Capture
} // namespace OVR

#endif
