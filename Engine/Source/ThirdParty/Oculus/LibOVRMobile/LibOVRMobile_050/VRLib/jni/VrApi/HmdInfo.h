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
#include "Sensors/OVR_Stereo.h"		// for LensConfig

namespace OVR
{

struct hmdInfoInternal_t
{
	LensConfig lens;

	float	lensSeparation;		// in meters

	// These values are always as if the display is in landscape
	// mode, being swapped from the system values if the manifest
	// is configured for portrait.
	float	widthMeters;		// in meters
	float	heightMeters;		// in meters
	int		widthPixels;		// in pixels
	int		heightPixels;		// in pixels 
	float	horizontalOffsetMeters; // the horizontal offset between the screen center and midpoint between lenses

	// Refresh rate of the display.
	float	displayRefreshRate;

	// Currently returns a conservative 1024x1024
	int		eyeTextureResolution[2];

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
	float	eyeTextureFov[2];
};

// The activity may not be a vrActivity if we are in Unity.
// We can't look up vrActivityClass here, because we may be on a non-startup thread
// in native apps.
hmdInfoInternal_t GetDeviceHmdInfo( const char * buildModel, JNIEnv * env, jobject activity, jclass vrActivityClass );
}

#endif	// OVR_HmdInfo_h
