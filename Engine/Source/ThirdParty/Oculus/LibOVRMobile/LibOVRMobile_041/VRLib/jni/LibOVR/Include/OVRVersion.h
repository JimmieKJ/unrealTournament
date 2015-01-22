/************************************************************************************

Filename    :   OVRVersion.h
Content     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef _OVR_VERSION_H
#define _OVR_VERSION_H

#define STRINGIZE( x )			#x
#define STRINGIZE_VALUE( x )	STRINGIZE( x )

#define OVR_MAJOR_VERSION 0
#define OVR_MINOR_VERSION 4
#define OVR_BUILD_VERSION 1
#define OVR_REV_VERSION	  0
#define OVR_VERSION_STRING		STRINGIZE_VALUE(OVR_MAJOR_VERSION)	\
							"." STRINGIZE_VALUE(OVR_MINOR_VERSION)	\
							"." STRINGIZE_VALUE(OVR_BUILD_VERSION)	\
							"." STRINGIZE_VALUE(OVR_REV_VERSION)	\
							" " STRINGIZE_VALUE(__TIMESTAMP__)		\

#endif
