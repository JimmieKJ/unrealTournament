/************************************************************************************

Filename    :   VrApi_local.h
Content     :   Minimum necessary API for mobile VR
Created     :   June 25, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_VrApi_Local_h
#define OVR_VrApi_Local_h

#include "DirectRender.h"
#include "HmdInfo.h"
#include "TimeWarp.h"

struct ovrMobile
{
	// To avoid problems with game loops that may inadvertantly
	// call WarpSwap() after doing LeaveVrMode(), the structure
	// is never actually freed, but just flagged as already destroyed.
	bool					Destroyed;

	// Valid for the thread that called ovr_EnterVrMode
	JNIEnv	*				Jni;

	// Thread from which VR mode was entered.
	pid_t					EnterTid;

	OVR::TimeWarp *			Warp;
	OVR::hmdInfoInternal_t	HmdInfo;
	ovrModeParms			Parms;
	OVR::TimeWarpInitParms	Twp;
};

#endif	// OVR_VrApi_Local_h

