// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ApplePlatformTime.mm: Apple implementations of time functions
=============================================================================*/

#include "CorePrivatePCH.h"

double FApplePlatformTime::InitTiming(void)
{
	// Time base is in nano seconds.
	mach_timebase_info_data_t Info;
	verify( mach_timebase_info( &Info ) == 0 );
	SecondsPerCycle = 1e-9 * (double)Info.numer / (double)Info.denom;
	return FPlatformTime::Seconds();
}
