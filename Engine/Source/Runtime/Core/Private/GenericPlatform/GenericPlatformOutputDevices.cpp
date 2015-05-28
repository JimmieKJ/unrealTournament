// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "HAL/FeedbackContextAnsi.h"
#include "Misc/App.h"
#include "Misc/OutputDeviceConsole.h"


void FGenericPlatformOutputDevices::SetupOutputDevices()
{
	check(GLog);

	GLog->AddOutputDevice(FPlatformOutputDevices::GetLog());

	if (!FParse::Param(FCommandLine::Get(), TEXT("NOCONSOLE")))
	{
		GLog->AddOutputDevice(GLogConsole);
	}

	// Only create debug output device if a debugger is attached or we're running on a console or build machine
	// A shipping build with logging explicitly enabled will fail the IsDebuggerPresent() check, but we still need to add the debug output device for logging purposes
	if (!FPlatformProperties::SupportsWindowedMode() || FPlatformMisc::IsDebuggerPresent() || (UE_BUILD_SHIPPING && !NO_LOGGING) || GIsBuildMachine)
	{
		GLog->AddOutputDevice(new FOutputDeviceDebug());
	}

	GLog->AddOutputDevice(FPlatformOutputDevices::GetEventLog());
};


FString FGenericPlatformOutputDevices::GetAbsoluteLogFilename()
{
	static TCHAR		Filename[1024] = { 0 };

	if (!Filename[0])
	{
		// The Editor requires a fully qualified directory to not end up putting the log in various directories.
		FCString::Strcpy(Filename, *FPaths::GameLogDir());

		if(	!FParse::Value(FCommandLine::Get(), TEXT("LOG="), Filename+FCString::Strlen(Filename), ARRAY_COUNT(Filename)-FCString::Strlen(Filename) )
			&&	!FParse::Value(FCommandLine::Get(), TEXT("ABSLOG="), Filename, ARRAY_COUNT(Filename) ) )
		{
			if (FCString::Strlen(FApp::GetGameName()) != 0)
			{
				FCString::Strcat(Filename, FApp::GetGameName());
			}
			else
			{
				FCString::Strcat( Filename, TEXT("UE4") );
			}
			FCString::Strcat( Filename, TEXT(".log") );
		}
	}

	return Filename;
}


class FOutputDevice* FGenericPlatformOutputDevices::GetLog()
{
	static FOutputDeviceFile Singleton;
	return &Singleton;
}


class FOutputDeviceError* FGenericPlatformOutputDevices::GetError()
{
	static FOutputDeviceAnsiError Singleton;
	return &Singleton;
}


class FFeedbackContext* FGenericPlatformOutputDevices::GetWarn()
{
	static FFeedbackContextAnsi Singleton;
	return &Singleton;
}
