// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxOutputDevices.cpp: Linux implementations of OutputDevices functions
=============================================================================*/


#include "CorePrivatePCH.h"
#include "LinuxPlatformOutputDevices.h"
#include "FeedbackContextAnsi.h"
#include "LinuxApplication.h"
#include "LinuxPlatformOutputDevicesPrivate.h"
#include "LinuxPlatformFeedbackContextPrivate.h"
#include "Misc/App.h"

#define CONSOLE_RED		"\x1b[31m"
#define CONSOLE_GREEN	"\x1b[32m"
#define CONSOLE_YELLOW	"\x1b[33m"
#define CONSOLE_BLUE	"\x1b[34m"
#define CONSOLE_NONE	"\x1b[0m"

//////////////////////////////////
// FLinuxOutputDevices
//////////////////////////////////

void								FLinuxOutputDevices::SetupOutputDevices()
{
	check(GLog);
	check(GLogConsole);

	// add file log
	GLog->AddOutputDevice(FPlatformOutputDevices::GetLog());

	// @todo: set to false for minor utils?
	bool bLogToConsole = !NO_LOGGING && !FParse::Param(FCommandLine::Get(), TEXT("NOCONSOLE"));

	if (bLogToConsole)
	{
		GLog->AddOutputDevice(GLogConsole);
	}

	// debug and event logging is not really supported on Linux. 
}

FString								FLinuxOutputDevices::GetAbsoluteLogFilename()
{
	// FIXME: this function should not exist once FGenericPlatformOutputDevices::GetAbsoluteLogFilename() returns absolute filename (see UE-25650)
	return FPaths::ConvertRelativePathToFull(FGenericPlatformOutputDevices::GetAbsoluteLogFilename());
}

class FOutputDeviceError*			FLinuxOutputDevices::GetError()
{
	static FOutputDeviceLinuxError Singleton;
	return &Singleton;
}

class FOutputDevice*				FLinuxOutputDevices::GetEventLog()
{
	return NULL; // @TODO No event logging
}

class FOutputDeviceConsole*			FLinuxOutputDevices::GetLogConsole()
{
	// this is a slightly different kind of singleton that gives ownership to the caller and should not be called more than once
	return new FOutputDeviceConsoleLinux();
}

class FFeedbackContext*				FLinuxOutputDevices::GetWarn()
{
	static FFeedbackContextLinux Singleton;
	return &Singleton;
}

//////////////////////////////////
// FOutputDeviceLinuxError
//////////////////////////////////

FOutputDeviceLinuxError::FOutputDeviceLinuxError()
:	ErrorPos(0)
{}

void FOutputDeviceLinuxError::Serialize(const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	FPlatformMisc::DebugBreak();

	if (!GIsCriticalError)
	{
		// First appError.
		GIsCriticalError = 1;
		TCHAR ErrorBuffer[1024];
		ErrorBuffer[0] = 0;
		// pop up a crash window if we are not in unattended mode; 
		if (WITH_EDITOR && FApp::IsUnattended() == false)
		{
			// @TODO Not implemented
			UE_LOG(LogLinux, Error, TEXT("appError called: %s"), Msg );
		}
		// log the warnings to the log
		else
		{
			UE_LOG(LogLinux, Error, TEXT("appError called: %s"), Msg );
		}
		FCString::Strncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) - 5 );
		FCString::Strncat( GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist) - 1 );
		ErrorPos = FCString::Strlen(GErrorHist);
	}
	else
	{
		UE_LOG(LogLinux, Error, TEXT("Error reentered: %s"), Msg );
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
		FPlatformMisc::RequestExit(true);
	}
}

void FOutputDeviceLinuxError::HandleError()
{
	// make sure we don't report errors twice
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		UE_LOG(LogLinux, Error, TEXT("HandleError re-entered.") );
		return;
	}

	// Trigger the OnSystemFailure hook if it exists
	FCoreDelegates::OnHandleSystemError.Broadcast();

#if !PLATFORM_EXCEPTIONS_DISABLED
	try
	{
#endif // !PLATFORM_EXCEPTIONS_DISABLED
		GIsGuarded = 0;
		GIsRunning = 0;
		GIsCriticalError = 1;
		GLogConsole = NULL;
		GErrorHist[ARRAY_COUNT(GErrorHist)-1] = 0;

		// Dump the error and flush the log.
		UE_LOG(LogLinux, Log, TEXT("=== Critical error: ===") LINE_TERMINATOR TEXT("%s") LINE_TERMINATOR, GErrorExceptionDescription);
		UE_LOG(LogLinux, Log, GErrorHist);

		GLog->Flush();

		// do not copy if graphics have not been initialized or if we're on the wrong thread
		if (FApp::CanEverRender() && IsInGameThread())
		{
			FPlatformMisc::ClipboardCopy(GErrorHist);
		}

		FPlatformMisc::SubmitErrorReport(GErrorHist, EErrorReportMode::Interactive);
		FCoreDelegates::OnShutdownAfterError.Broadcast();
#if !PLATFORM_EXCEPTIONS_DISABLED
	}
	catch(...)
	{}
#endif // !PLATFORM_EXCEPTIONS_DISABLED
}

////////////////////////////////////////
// FOutputDeviceConsoleLinux
////////////////////////////////////////

FOutputDeviceConsoleLinux::FOutputDeviceConsoleLinux()
	: bOverrideColorSet(false)
{
}

FOutputDeviceConsoleLinux::~FOutputDeviceConsoleLinux()
{
}

void FOutputDeviceConsoleLinux::Show(bool bShowWindow)
{
}

bool FOutputDeviceConsoleLinux::IsShown()
{
	return true;
}

void FOutputDeviceConsoleLinux::Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	static bool bEntry=false;
	if (!GIsCriticalError || bEntry)
	{
		if (Verbosity == ELogVerbosity::SetColor)
		{
			printf("%s", TCHAR_TO_UTF8(Data));
		}
		else
		{
			bool bNeedToResetColor = false;
			if (!bOverrideColorSet)
			{
				if (Verbosity == ELogVerbosity::Error)
				{
					printf(CONSOLE_RED);
					bNeedToResetColor = true;
				}
				else if (Verbosity == ELogVerbosity::Warning)
				{
					printf(CONSOLE_YELLOW);
					bNeedToResetColor = true;
				}
			}

			printf("%ls\n", *FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Data, GPrintLogTimes));

			if (bNeedToResetColor)
			{
				printf(CONSOLE_NONE);
			}
		}
	}
	else
	{
		bEntry=true;
#if !PLATFORM_EXCEPTIONS_DISABLED
		try
		{
#endif // !PLATFORM_EXCEPTIONS_DISABLED
			// Ignore errors to prevent infinite-recursive exception reporting.
			Serialize(Data, Verbosity, Category);
#if !PLATFORM_EXCEPTIONS_DISABLED
		}
		catch (...)
		{
		}
#endif // !PLATFORM_EXCEPTIONS_DISABLED
		bEntry = false;
	}
}
