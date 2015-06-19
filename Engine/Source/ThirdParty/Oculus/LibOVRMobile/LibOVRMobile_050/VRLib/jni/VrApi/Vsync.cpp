/************************************************************************************

Filename    :   Vsync.cpp
Content     :   Global native code interface to the raster vsync information
Created     :   September 19, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "Vsync.h"

#include <stdlib.h>
#include <jni.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>			// for usleep
#include <sys/time.h>
#include <sys/resource.h>

#include "Kernel/OVR_Timer.h"
#include "Android/LogUtils.h"
#include "VrApi.h"			// for ovr_GetTimeInSeconds()

#if defined(OVR_ENABLE_CAPTURE)
	#include <OVR_Capture.h>
#endif

/*
 * As of 6/30/2014, I am seeing vsync frame timings of 16.71 ms for the 1080 S5,
 * and 16.91 ms for the 1440 s5, both of which are farther from the expected
 * 16.66 ms than I would like.
 */

static bool LogVsync = false;
// console function for toggling vsync logging on and off
void SetLogVsync( void * appPtr, const char * cmd )
{
	int v = atoi( cmd );
	LogVsync = v != 0;
}

extern "C"
{
	// The nativeVsync function is called from java with timing
	// information that GetFractionalVsync() and FramePointTimeInSeconds()
	// can use to closely estimate raster position.
	void Java_com_oculusvr_vrlib_VrLib_nativeVsync( JNIEnv *jni, jclass clazz, jlong frameTimeNanos )
	{
	#if defined(OVR_ENABLE_CAPTURE)
		OVR::Capture::VSyncTimestamp(frameTimeNanos);
	#endif
		if ( LogVsync )
		{
			static long long prevFrameTimeNanos;
			LOG( "nativeSetVsync: %5.2f ms", ( frameTimeNanos - prevFrameTimeNanos ) * 0.000001 );
			prevFrameTimeNanos = frameTimeNanos;
		}

		OVR::VsyncState	state = OVR::UpdatedVsyncState.GetState();

		// Round this, because different phone models have slightly different periods.
		state.vsyncCount += floor( 0.5 + ( frameTimeNanos - state.vsyncBaseNano ) / state.vsyncPeriodNano );
		state.vsyncPeriodNano = 1e9 / 60.0;
		state.vsyncBaseNano = frameTimeNanos;

		OVR::UpdatedVsyncState.SetState( state );
	}
}	// extern "C"


namespace OVR
{

// This can be read without any locks, so a high priority rendering thread doesn't
// have to worry about being blocked by a sensor thread that got preempted.
OVR::LocklessUpdater<VsyncState>	UpdatedVsyncState;

// This is separate to allow easy switching to a fixed value.
VsyncState GetVsyncState()
{
	if ( 0 )
	{	// constant
		static VsyncState	state;
		if ( state.vsyncBaseNano == 0 ) {
			state.vsyncBaseNano = Timer::GetTicksNanos();
			state.vsyncPeriodNano = 1e9 / 60.0;
		}
		return state;
	}
	else
	{	// normal update
		return UpdatedVsyncState.GetState();
	}
}

double GetFractionalVsync()
{
	const VsyncState state = GetVsyncState();

	const jlong t = Timer::GetTicksNanos();
	if ( state.vsyncBaseNano == 0 )
	{
		return 0;
	}
	const double vsync = (double)state.vsyncCount + (double)(t - state.vsyncBaseNano ) / state.vsyncPeriodNano;
	return vsync;
}

double FramePointTimeInSeconds( const double framePoint )
{
	const VsyncState state = GetVsyncState();
	const double seconds = ( state.vsyncBaseNano + ( framePoint - state.vsyncCount ) * state.vsyncPeriodNano ) * 1e-9;
	return seconds;
}

double FramePointTimeInSecondsWithBlanking( const double framePoint )
{
	const VsyncState state = GetVsyncState();
	static const double startBias = 0.0; // 8.0/1920.0/60.0;	// about 8 pixels into a 1920 screen at 60 hz
	static const double activeFraction = 112.0 / 135;
	const double seconds = ( state.vsyncBaseNano + activeFraction * ( framePoint - state.vsyncCount ) * state.vsyncPeriodNano )
			* 1e-9 + startBias;
	return seconds;
}


float SleepUntilTimePoint( const double targetSeconds, const bool busyWait )
{
	const float sleepSeconds = targetSeconds - ovr_GetTimeInSeconds();
	if ( sleepSeconds > 0 )
	{
		if ( busyWait )
		{
			while( targetSeconds - ovr_GetTimeInSeconds() > 0 )
			{
			}
		}
		else
		{
			// I'm assuming we will never sleep more than one full second.
			timespec	t, rem;
			t.tv_sec = 0;
			t.tv_nsec = sleepSeconds * 1e9;
			nanosleep( &t, &rem );
			const double overSleep = ovr_GetTimeInSeconds() - targetSeconds;
			if ( overSleep > 0.001 )
			{
	//			LOG( "Overslept %f seconds", overSleep );
			}
		}
	}
	return sleepSeconds;
}

}	// namespace OVR
