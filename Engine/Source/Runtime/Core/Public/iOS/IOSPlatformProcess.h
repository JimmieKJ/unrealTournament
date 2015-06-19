// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	IOSPlatformProcess.h: iOS platform Process functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformProcess.h"

/** Dummy process handle for platforms that use generic implementation. */
struct FProcHandle : public TProcHandle<void*, nullptr>
{
public:
	/** Default constructor. */
	FORCEINLINE FProcHandle()
		: TProcHandle()
	{}

	/** Initialization constructor. */
	FORCEINLINE explicit FProcHandle( HandleType Other )
		: TProcHandle( Other )
	{}
};

/**
* iOS implementation of the Process OS functions
**/
struct CORE_API FIOSPlatformProcess : public FGenericPlatformProcess
{
	static const TCHAR* ComputerName();
	static const TCHAR* BaseDir();
	static FRunnableThread* CreateRunnableThread();
	static void LaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error );
	static void SetRealTimeMode();
	static void SetupGameOrRenderThread(bool bIsRenderThread);
	static void SetThreadAffinityMask(uint64 AffinityMask);
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
};

typedef FIOSPlatformProcess FPlatformProcess;

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

