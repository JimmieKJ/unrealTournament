// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	IOSPlatformProcess.h: iOS platform Process functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformProcess.h"
#import "IOSAsyncTask.h"

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
	static bool CanLaunchURL(const TCHAR* URL);
	static FString GetGameBundleId();
	static void SetRealTimeMode();
	static void SetupGameThread();
	static void SetupRenderThread();
	static void SetThreadAffinityMask(uint64 AffinityMask);
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);

private:
	static void SetupThread(int Priority);
};

typedef FIOSPlatformProcess FPlatformProcess;

typedef FSystemWideCriticalSectionNotImplemented FSystemWideCriticalSection;
