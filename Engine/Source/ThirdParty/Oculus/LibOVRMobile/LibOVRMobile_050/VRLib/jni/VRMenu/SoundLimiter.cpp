/************************************************************************************

Filename    :   SoundLimiter.cpp
Content     :   Utility class for limiting how often sounds play.
Created     :   June 23, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "SoundLimiter.h"

#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_TypesafeNumber.h"
#include "Kernel/OVR_BitFlags.h"
#include "Kernel/OVR_Math.h"
#include "Android/LogUtils.h"
#include "VrApi/VrApi.h"		// ovrPoseStatef

#include "../Input.h"
#include "../VrCommon.h"
#include "../App.h"
#include "../SoundManager.h"

namespace OVR {

//==============================
// SoundLimiter::PlaySound
void SoundLimiter::PlaySound( App * app, char const * soundName, double const limitSeconds )
{
	double curTime = ovr_GetTimeInSeconds();
	double t = curTime - LastPlayTime;
    //DROIDLOG( "VrMenu", "PlaySound( '%s', %.2f ) - t == %.2f : %s", soundName, limitSeconds, t, t >= limitSeconds ? "PLAYING" : "SKIPPING" );
	if ( t >= limitSeconds )
	{
		app->PlaySound( soundName );
		LastPlayTime = curTime;
	}
}

void SoundLimiter::PlayMenuSound( class App * app, char const * appendKey, char const * soundName, double const limitSeconds )
{
	char overrideSound[ 1024 ];
	OVR_sprintf( overrideSound, 1024, "%s_%s", appendKey, soundName );

	if ( app->GetSoundMgr().HasSound( overrideSound ) )
	{
		PlaySound( app, overrideSound, limitSeconds );
	}
	else
	{
		PlaySound( app, soundName, limitSeconds );
	}
}

} // namespace OVR
