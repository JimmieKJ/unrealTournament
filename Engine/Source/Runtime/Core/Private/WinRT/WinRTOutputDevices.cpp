// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WinRTOutputDevices.cpp: WinRT implementations of OutputDevices functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "Misc/App.h"

#include "FeedbackContextAnsi.h"
#include "WinRTOutputDevicesPrivate.h"
#include "WinRTFeedbackContextPrivate.h"

DEFINE_LOG_CATEGORY(LogWinRTOutputDevices);

//////////////////////////////////
// FWinRTOutputDevices
//////////////////////////////////

class FOutputDeviceError* FWinRTOutputDevices::GetError()
{
	static FOutputDeviceWinRTError Singleton;
	return &Singleton;
}

class FOutputDevice* FWinRTOutputDevices::GetEventLog()
{
#if WANTS_WinRT_EVENT_LOGGING
	static FOutputDeviceEventLog Singleton;
	return &Singleton;
#else // no event logging
	return NULL;
#endif //WANTS_WinRT_EVENT_LOGGING
}

class FOutputDeviceConsole* FWinRTOutputDevices::GetLogConsole()
{
	// this is a slightly different kind of singleton that gives ownership to the caller and should not be called more than once
	return new FOutputDeviceConsoleWinRT();
}

class FFeedbackContext* FWinRTOutputDevices::GetWarn()
{
	static FFeedbackContextAnsi Singleton;
	return &Singleton;
}

//////////////////////////////////
// FOutputDeviceWinRTError
//////////////////////////////////

FOutputDeviceWinRTError::FOutputDeviceWinRTError()
:	ErrorPos(0)
{}

void FOutputDeviceWinRTError::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::DebugBreak();
   
	int32 Error = GetLastError();
	if( !GIsCriticalError )
	{   
		// First appError.
		GIsCriticalError = 1;
		TCHAR ErrorBuffer[1024];
		ErrorBuffer[0] = 0;
		// pop up a crash window if we are not in unattended mode
		if( FApp::IsUnattended() == false )
		{ 
			UE_LOG(LogWinRTOutputDevices, Error, TEXT("appError called: %s"), Msg );

			// WinRT error.
			UE_LOG(LogWinRTOutputDevices, Error, TEXT("WinRT GetLastError: %s (%i)"), FPlatformMisc::GetSystemErrorMessage(ErrorBuffer,1024,Error), Error );
		} 
		// log the warnings to the log
		else
		{
			UE_LOG(LogWinRTOutputDevices, Error, TEXT("appError called: %s"), Msg );

			// WinRT error.
			UE_LOG(LogWinRTOutputDevices, Error, TEXT("WinRT GetLastError: %s (%i)"), FPlatformMisc::GetSystemErrorMessage(ErrorBuffer,1024,Error), Error );
		}
// 		appStrncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) - 5 );
// 		appStrncat( GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist) - 1  );
// 		ErrorPos = appStrlen(GErrorHist);
	}
	else
	{
		UE_LOG(LogWinRTOutputDevices, Error, TEXT("Error reentered: %s"), Msg );
	}

	if( GIsGuarded )
	{
		// Propagate error so structured exception handler can perform necessary work.
#if PLATFORM_EXCEPTIONS_DISABLED
		FPlatformMisc::DebugBreak();
#endif
		FPlatformMisc::RaiseException(1);
	}
	else
	{
		// We crashed outside the guarded code (e.g. appExit).
		HandleError();
		FPlatformMisc::RequestExit( true );
	}
}

void FOutputDeviceWinRTError::HandleError()
{
	// make sure we don't report errors twice
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		UE_LOG(LogWinRTOutputDevices, Error, TEXT("HandleError re-entered.") );
		return;
	}

	try
	{
		GIsGuarded				= 0;
		GIsRunning				= 0;
		GIsCriticalError		= 1;
		GLogConsole				= NULL;
//		GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;
		// Dump the error and flush the log.
//		UE_LOG(LogWinRTOutputDevices, Log, TEXT("=== Critical error: ===") LINE_TERMINATOR TEXT("%s"), GErrorHist);
		GLog->Flush();

//		appClipboardCopy(GErrorHist);

//		FPlatformMisc::SubmitErrorReport( GErrorHist, EErrorReportMode::Interactive );

		FCoreDelegates::OnShutdownAfterError.Broadcast();
	}
	catch( ... )
	{}
}

////////////////////////////////////////
// FOutputDeviceConsoleWinRT
////////////////////////////////////////

FOutputDeviceConsoleWinRT::FOutputDeviceConsoleWinRT()
	: ConsoleHandle(0)
{
}

FOutputDeviceConsoleWinRT::~FOutputDeviceConsoleWinRT()
{
	SaveToINI();

	// WRH - 2007/08/23 - This causes the process to take a LONG time to shut down when clicking the "close window"
	// button on the top-right of the console window.
	//FreeConsole();
}

void FOutputDeviceConsoleWinRT::SaveToINI()
{
}

void FOutputDeviceConsoleWinRT::Show(bool ShowWindow)
{
}

bool FOutputDeviceConsoleWinRT::IsShown()
{
	return ConsoleHandle != NULL;
}

void FOutputDeviceConsoleWinRT::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	if( ConsoleHandle )
	{
		static bool Entry=0;
		if( !GIsCriticalError || Entry )
		{
			// here we can change the color of the text to display, it's in the format:
			// ForegroundRed | ForegroundGreen | ForegroundBlue | ForegroundBright | BackgroundRed | BackgroundGreen | BackgroundBlue | BackgroundBright
			// where each value is either 0 or 1 (can leave off trailing 0's), so 
			// blue on bright yellow is "00101101" and red on black is "1"
			// An empty string reverts to the normal gray on black
			if (Verbosity == ELogVerbosity::SetColor)
			{
				if (FCString::Stricmp(Data, TEXT("")) != 0)
				{
					// turn the string into a bunch of 0's and 1's
					TCHAR String[9];
					FMemory::Memset(String, 0, sizeof(TCHAR) * ARRAY_COUNT(String));
					FCString::Strncpy(String, Data, ARRAY_COUNT(String));
					for (TCHAR* S = String; *S; S++)
					{
						*S -= '0';
					}
					// make the color
				}
			}
		}
		else
		{
			Entry=1;
			try
			{
				// Ignore errors to prevent infinite-recursive exception reporting.
				Serialize( Data, Verbosity, Category );
			}
			catch( ... )
			{}
			Entry=0;
		}
	}
}
