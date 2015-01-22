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
#include "Log.h"
#include "../Input.h"
#include "../VrCommon.h"

#include "../App.h"

namespace OVR {

//==============================
// SoundLimiter::PlaySound
void SoundLimiter::PlaySound( App * app, char const * soundName, double const limitSeconds )
{
	double curTime = TimeInSeconds();
	double t = curTime - LastPlayTime;
    //DROIDLOG( "VrMenu", "PlaySound( '%s', %.2f ) - t == %.2f : %s", soundName, limitSeconds, t, t >= limitSeconds ? "PLAYING" : "SKIPPING" );
	if ( t >= limitSeconds )
	{
		app->PlaySound( soundName );
		LastPlayTime = curTime;
	}
}

} // namespace OVR
