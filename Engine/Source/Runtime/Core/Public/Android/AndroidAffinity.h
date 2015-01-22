// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
AndroidAffinity.h: Android affinity profile masks definitions.
==============================================================================================*/

#pragma once

#include "GenericPlatform/GenericPlatformAffinity.h"

class FAndroidAffinity : public FGenericPlatformAffinity
{
public:
	static const CORE_API uint64 GetMainGameMask()
	{
		return MAKEAFFINITYMASK1(0);
	}

	static const CORE_API uint64 GetRenderingThreadMask()
	{
		return MAKEAFFINITYMASK1(1);
	}

	static const CORE_API uint64 GetRTHeartBeatMask()
	{
		return MAKEAFFINITYMASK1(4);
	}

	static const CORE_API uint64 GetPoolThreadMask()
	{
		return MAKEAFFINITYMASK1(4);
	}

	static const CORE_API uint64 GetTaskGraphThreadMask()
	{
		return MAKEAFFINITYMASK3(0, 2, 3);
	}

	static const CORE_API uint64 GetStatsThreadMask()
	{
		return MAKEAFFINITYMASK1(5);
	}
};

typedef FAndroidAffinity FPlatformAffinity;
