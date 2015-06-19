// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HAL/Platform.h"

#define		MAKEAFFINITYMASK1(x)				(1<<x)
#define		MAKEAFFINITYMASK2(x,y)				((1<<x)+(1<<y))
#define		MAKEAFFINITYMASK3(x,y,z)			((1<<x)+(1<<y)+(1<<z))
#define		MAKEAFFINITYMASK4(w,x,y,z)			((1<<w)+(1<<x)+(1<<y)+(1<<z))
#define		MAKEAFFINITYMASK5(v,w,x,y,z)		((1<<v)+(1<<w)+(1<<x)+(1<<y)+(1<<z))
#define		MAKEAFFINITYMASK6(u,v,w,x,y,z)		((1<<u)+(1<<v)+(1<<w)+(1<<x)+(1<<y)+(1<<z))
#define		MAKEAFFINITYMASK7(t,u,v,w,x,y,z)	((1<<t)+(1<<u)+(1<<v)+(1<<w)+(1<<x)+(1<<y)+(1<<z))

class FGenericPlatformAffinity
{
public:
	static const CORE_API uint64 GetMainGameMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetRenderingThreadMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetRHIThreadMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetRTHeartBeatMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetPoolThreadMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetTaskGraphThreadMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetStatsThreadMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetNoAffinityMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}
};


