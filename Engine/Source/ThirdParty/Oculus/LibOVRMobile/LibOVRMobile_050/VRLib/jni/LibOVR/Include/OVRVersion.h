/************************************************************************************

Filename    :   OVRVersion.h
Content     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef _OVR_VERSION_H
#define _OVR_VERSION_H

#define OVR_MAJOR_VERSION 0
#define OVR_MINOR_VERSION 5
#define OVR_BUILD_VERSION 0
#define OVR_REV_VERSION	  0

// Note: this is an array rather than a char* because this makes it
// more convenient to extract the value of the symbol from the binary
extern const char OVR_VERSION_STRING[];

#endif
