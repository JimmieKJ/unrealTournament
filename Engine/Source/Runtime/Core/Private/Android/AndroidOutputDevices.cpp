// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidOutputDevices.cpp: Android implementations of OutputDevices functions
=============================================================================*/


#include "CorePrivatePCH.h"
#include "AndroidPlatformOutputDevicesPrivate.h"

class FOutputDeviceError* FAndroidOutputDevices::GetError()
{
	static FOutputDeviceAndroidError Singleton;
	return &Singleton;
}

/*class FOutputDevice* FAndroidOutputDevices::GetLog()
{
#if UE_BUILD_SHIPPING
	return NULL;
#else
	// Always enable logging via ADB
	static FOutputDeviceAndroidDebug Singleton;
	return &Singleton;
	return NULL;
#endif
}*/

FOutputDeviceAndroidDebug::FOutputDeviceAndroidDebug()
{}

void FOutputDeviceAndroidDebug::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::LowLevelOutputDebugString(*FOutputDevice::FormatLogLine(Verbosity, Category, Msg, GPrintLogTimes));
}

//////////////////////////////////
// FOutputDeviceAndroidError
//////////////////////////////////

FOutputDeviceAndroidError::FOutputDeviceAndroidError()
{}

void FOutputDeviceAndroidError::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::LowLevelOutputDebugString(*FOutputDevice::FormatLogLine(Verbosity, Category, Msg, GPrintLogTimes));
	if (GIsGuarded)
	{
		FPlatformMisc::DebugBreak();
	}
	else
	{
		HandleError();
		FPlatformMisc::RequestExit(true);
	}
}

void FOutputDeviceAndroidError::HandleError()
{
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);

	if (NewCallCount != 1)
	{
		UE_LOG(LogAndroid, Error, TEXT("HandleError re-entered."));
		return;
	}
	
	GIsGuarded = 0;
	GIsRunning = 0;
	GIsCriticalError = 1;
	GLogConsole = NULL;

	GLog->Flush();
}
