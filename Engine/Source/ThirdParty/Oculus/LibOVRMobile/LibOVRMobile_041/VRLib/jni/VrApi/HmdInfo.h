/************************************************************************************

Filename    :   HmdInfo.h
Content     :   Head Mounted Display Info
Created     :   January 30, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_HmdInfo_h
#define OVR_HmdInfo_h

#include <jni.h>
#include "OVR_Stereo.h"
#include "OVR_CAPI.h"

namespace OVR
{

struct hmdInfoInternal_t
{
	LensConfig lens;

	float	lensSeparation;		// in meters, 0.059f for prototype

	// These values are always as if the display is in landscape
	// mode, being swapped from the system values if the manifest
	// is configured for portrait.
	float	widthMeters;		// in meters, 0.1105f for Galaxy S4
	float	heightMeters;		// in meters, 0.06214f for Galaxy S4
	int		widthPixels;		// 1920
	int		heightPixels;		// 1080

	// This is a product of the lens distortion and the screen size,
	// but there is no truly correct answer.
	//
	// There is a tradeoff in resolution and coverage.
	// Too small of an fov will leave unrendered pixels visible, but too
	// large wastes resolution or fill rate.  It is unreasonable to
	// increase it until the corners are completely covered, but we do
	// want most of the outside edges completely covered.
	//
	// Applications might choose to render a larger fov when angular
	// acceleration is high to reduce black pull in at the edges by
	// TimeWarp.
	float	eyeTextureFov;
};

// The activity may not be a vrActivity if we are in Unity.
// We can't look up vrActivityClass here, because we may be on a non-startup thread
// in native apps.
hmdInfoInternal_t	GetDeviceHmdInfo( JNIEnv *env, jobject activity, jclass vrActivityClass,
	ovrHmd hmd, const char * buildModel);

}

#endif	// OVR_HmdInfo_h
