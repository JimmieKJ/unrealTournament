// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	WinRTProcess.h: WinRT platform Process functions
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
 * WinRT implementation of the Process OS functions
 */
struct CORE_API FWinRTProcess : public FGenericPlatformProcess
{
	static const TCHAR* BaseDir();
	static void Sleep( float Seconds );
	static void SleepInfinite();
	static class FEvent* CreateSynchEvent(bool bIsManualReset = false);
	static class FRunnableThread* CreateRunnableThread();
};

typedef FWinRTProcess FPlatformProcess;

#include "../WinRT/WinRTCriticalSection.h"
typedef FWinRTCriticalSection FCriticalSection;

