// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSPlatformOutputDevices.mm: iOS implementations of OutputDevices functions
=============================================================================*/

#include "CorePrivatePCH.h"

#include "FeedbackContextAnsi.h"
#include "../Private/IOS/IOSPlatformOutputDevicesPrivate.h"

//////////////////////////////////
// FIOSPlatformOutputDevices
//////////////////////////////////

class FOutputDeviceError* FIOSPlatformOutputDevices::GetError()
{
	static FOutputDeviceIOSError Singleton;
	return &Singleton;
}
class FOutputDevice*	FIOSPlatformOutputDevices::GetLog()
{
    static FOutputDeviceFile Singleton(nullptr, true);
    return &Singleton;
}

//class FFeedbackContext*				FIOSPlatformOutputDevices::GetWarn()
//{
//	static FOutputDeviceIOSDebug Singleton;
//	return &Singleton;
//}

//////////////////////////////////
// FOutputDeviceIOSDebug
//////////////////////////////////

FOutputDeviceIOSDebug::FOutputDeviceIOSDebug() 
{}

void FOutputDeviceIOSDebug::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s%s"),*FOutputDevice::FormatLogLine(Verbosity, Category, Msg, GPrintLogTimes),LINE_TERMINATOR); 
}

//////////////////////////////////
// FOutputDeviceIOSError
//////////////////////////////////

FOutputDeviceIOSError::FOutputDeviceIOSError()
:	ErrorPos(0)
{}

void FOutputDeviceIOSError::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	// @todo iosmerge: this was a big mess copied from Mac that we don't want at all. can we use default like other consoles?
	if( GIsGuarded )
	{
//		FOutputDevice::Serialize(Msg, Verbosity, Category);
		FPlatformMisc::DebugBreak();
	}
	else
	{
		// We crashed outside the guarded code (e.g. appExit).
		HandleError();
		FPlatformMisc::RequestExit( true );
	}
}

void FOutputDeviceIOSError::HandleError()
{
	// make sure we don't report errors twice
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		UE_LOG(LogIOS, Error, TEXT("HandleError re-entered.") );
		return;
	}

	GIsGuarded = 0;
	GIsRunning = 0;
	GIsCriticalError = 1;
	GLogConsole = NULL;

	GLog->Flush();
}
