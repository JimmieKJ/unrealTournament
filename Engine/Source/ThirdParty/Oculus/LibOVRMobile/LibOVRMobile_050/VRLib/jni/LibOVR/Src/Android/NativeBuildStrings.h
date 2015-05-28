
/************************************************************************************

Filename    :   App.h
Content     :   Query Android Build strings through JNI
Created     :   July 12, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_NATIVE_BUILD_STRINGS_H
#define OVR_NATIVE_BUILD_STRINGS_H

#include <jni.h>

void ovr_InitBuildStrings( JNIEnv * env );

enum eBuildString
{
	BUILDSTR_BRAND,
	BUILDSTR_DEVICE,
	BUILDSTR_DISPLAY,
	BUILDSTR_FINGERPRINT,
	BUILDSTR_HARDWARE,
	BUILDSTR_HOST,
	BUILDSTR_ID,
	BUILDSTR_MODEL,
	BUILDSTR_PRODUCT,
	BUILDSTR_SERIAL,
	BUILDSTR_TAGS,
	BUILDSTR_TYPE,
	BUILDSTR_MAX
};

// Returns the value of a specific build string.
char const * ovr_GetBuildString( eBuildString const id );

#endif // OVR_NATIVE_BUILD_STRINGS_H

