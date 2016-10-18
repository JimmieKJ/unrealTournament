// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformAffinity.h"

class CORE_API FIOSPlatformAffinity : public FGenericPlatformAffinity
{
public:
	static const uint64 GetMainGameMask();
	static const uint64 GetRenderingThreadMask();
	static const uint64 GetRTHeartBeatMask();
	static const uint64 GetPoolThreadMask();
	static const uint64 GetTaskGraphThreadMask();
	static const uint64 GetStatsThreadMask();
	static const uint64 GetNoAffinityMask();
};

typedef FIOSPlatformAffinity FPlatformAffinity;

typedef FSystemWideCriticalSectionNotImplemented FSystemWideCriticalSection;
