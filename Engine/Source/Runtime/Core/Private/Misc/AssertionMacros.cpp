// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Misc/AssertionMacros.h"
#include "Misc/VarArgs.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Misc/CString.h"
#include "Misc/Crc.h"
#include "Containers/UnrealString.h"
#include "Containers/StringConv.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "HAL/PlatformStackWalk.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "Misc/CoreMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/OutputDeviceError.h"
#include "Stats/StatsMisc.h"
#include "Misc/CoreDelegates.h"
#include "HAL/ExceptionHandling.h"
#include "HAL/ThreadHeartBeat.h"

bool FDebug::bHasAsserted = false;

#define FILE_LINE_DESC TEXT(" [File:%s] [Line: %i] ")

/** Lock used to synchronize the fail debug calls. */
static FCriticalSection	FailDebugCriticalSection;

/** Number of top function calls to hide when dumping the callstack as text. */
#if PLATFORM_LINUX

	// Rationale: check() and ensure() handlers have different depth - worse, ensure() can optionally end up calling the same path as check().
	// It is better to show the full callstack as is than accidentaly ignore a part of the problem
	#define CALLSTACK_IGNOREDEPTH 0

#else
	#define CALLSTACK_IGNOREDEPTH 2
#endif // PLATFORM_LINUX

void PrintScriptCallstack(bool bEmptyWhenDone)
{
#if DO_BLUEPRINT_GUARD
	// Walk the script stack, if any
	FBlueprintExceptionTracker& BlueprintExceptionTracker = FBlueprintExceptionTracker::Get();
	if( BlueprintExceptionTracker.ScriptStack.Num() > 0 )
	{
		FString ScriptStack = TEXT( "\n\nScript Stack:\n" );
		for (int32 FrameIdx = BlueprintExceptionTracker.ScriptStack.Num() - 1; FrameIdx >= 0; --FrameIdx)
		{
			ScriptStack += BlueprintExceptionTracker.ScriptStack[FrameIdx].GetStackDescription() + TEXT( "\n" );
		}

		UE_LOG( LogOutputDevice, Warning, TEXT( "%s" ), *ScriptStack );

		if (bEmptyWhenDone)
		{
			BlueprintExceptionTracker.ScriptStack.Empty();
		}
	}
#endif
}

/**
 *	Prints error to the debug output, 
 *	prompts for the remote debugging if there is not debugger, breaks into the debugger 
 *	and copies the error into the global error message.
 */
void StaticFailDebug( const TCHAR* Error, const ANSICHAR* File, int32 Line, const TCHAR* Description, bool bIsEnsure )
{
	// For ensure log should be flushed in the engine loop.
	if( !bIsEnsure )
	{
		GLog->PanicFlushThreadedLogs();
	}

	FScopeLock Lock( &FailDebugCriticalSection );
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s") FILE_LINE_DESC TEXT("\n%s\n"), Error, ANSI_TO_TCHAR(File), Line, Description);

	// Copy the detailed error into the error message.
	FCString::Snprintf( GErrorMessage, ARRAY_COUNT( GErrorMessage ), TEXT( "%s" ) FILE_LINE_DESC TEXT( "\n%s\n" ), Error, ANSI_TO_TCHAR( File ), Line, Description );

	// Copy the error message to the error history.
	FCString::Strncpy( GErrorHist, GErrorMessage, ARRAY_COUNT( GErrorHist ) );
	FCString::Strncat( GErrorHist, TEXT( "\r\n\r\n" ), ARRAY_COUNT( GErrorHist ) );
}

void OutputMultiLineCallstack(const ANSICHAR* File, int32 Line, const FName& LogName, const TCHAR* Heading, TCHAR* Message, ELogVerbosity::Type Verbosity)
{
	const bool bWriteUATMarkers = FParse::Param(FCommandLine::Get(), TEXT("CrashForUAT")) && FParse::Param(FCommandLine::Get(), TEXT("stdout"));

	if (bWriteUATMarkers)
	{
		FMsg::Logf(File, Line, LogName, Verbosity, TEXT("begin: stack for UAT"));
	}

	FMsg::Logf(File, Line, LogName, Verbosity, TEXT("%s"), Heading);
	FMsg::Logf(File, Line, LogName, Verbosity, TEXT(""));

	for (TCHAR* LineStart = Message;; )
	{
		// Find the end of the current line
		TCHAR* LineEnd = LineStart;
		while (*LineEnd != 0 && *LineEnd != '\r' && *LineEnd != '\n')
		{
			LineEnd++;
		}

		// Output it
		TCHAR LineEndCharacter = *LineEnd;
		*LineEnd = 0;
		FMsg::Logf(File, Line, LogName, Verbosity, TEXT("%s"), LineStart);
		*LineEnd = LineEndCharacter;

		// Quit if this was the last line
		if (*LineEnd == 0)
		{
			break;
		}

		// Move to the next line
		LineStart = (LineEnd[0] == '\r' && LineEnd[1] == '\n') ? (LineEnd + 2) : (LineEnd + 1);
	}

	if (bWriteUATMarkers)
	{
		FMsg::Logf(File, Line, LogName, Verbosity, TEXT("end: stack for UAT"));
	}
}

#if DO_CHECK || DO_GUARD_SLOW
//
// Failed assertion handler.
//warning: May be called at library startup time.
//
void VARARGS FDebug::LogAssertFailedMessage(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format/*=TEXT("")*/, ...)
{
	// Print out the blueprint callstack
	PrintScriptCallstack(true);

	// Ignore this assert if we're already forcibly shutting down because of a critical error.
	if( !GIsCriticalError )
	{
		TCHAR DescriptionString[4096];
		GET_VARARGS( DescriptionString, ARRAY_COUNT( DescriptionString ), ARRAY_COUNT( DescriptionString ) - 1, Format, Format );

		TCHAR ErrorString[MAX_SPRINTF];
		FCString::Sprintf( ErrorString, TEXT( "Assertion failed: %s" ), ANSI_TO_TCHAR( Expr ) );

		if( FPlatformProperties::AllowsCallStackDumpDuringAssert() )
		{
			ANSICHAR StackTrace[4096];
			if( StackTrace != NULL )
			{
				StackTrace[0] = 0;
				FPlatformStackWalk::StackWalkAndDump( StackTrace, ARRAY_COUNT(StackTrace), CALLSTACK_IGNOREDEPTH );

				FCString::Strncat( DescriptionString, ANSI_TO_TCHAR( StackTrace ), ARRAY_COUNT( DescriptionString ) - 1 );
			}
		}

		StaticFailDebug( ErrorString, File, Line, DescriptionString, false );
	}
}

/**
 * Called when an 'ensure' assertion fails; gathers stack data and generates and error report.
 *
 * @param	Expr	Code expression ANSI string (#code)
 * @param	File	File name ANSI string (__FILE__)
 * @param	Line	Line number (__LINE__)
 * @param	Msg		Informative error message text
 */
void FDebug::EnsureFailed(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Msg)
{

#if STATS
	FString EnsureFailedPerfMessage = FString::Printf(TEXT("FDebug::EnsureFailed"));
	SCOPE_LOG_TIME_IN_SECONDS(*EnsureFailedPerfMessage, nullptr)
#endif

	// You can set bShouldCrash to true to cause a regular assertion to trigger (stopping program execution) when an ensure() error occurs
	const bool bShouldCrash = false;		// By default, don't crash on ensure()
	if( bShouldCrash )
	{
		// Just trigger a regular assertion which will crash via GError->Logf()
		FDebug::LogAssertFailedMessage( Expr, File, Line, Msg );
		return;
	}

	// Print out the blueprint callstack
	PrintScriptCallstack(false);

	// Print initial debug message for this error
	TCHAR ErrorString[MAX_SPRINTF];
	FCString::Sprintf(ErrorString,TEXT("Ensure condition failed: %s"),ANSI_TO_TCHAR(Expr));

	StaticFailDebug( ErrorString, File, Line, Msg, true );

	// Is there a debugger attached?  If not we'll submit an error report.
	if (FPlatformMisc::IsDebuggerPresent())
	{
#if !NO_LOGGING
		UE_LOG(LogOutputDevice, Error, TEXT("%s [File:%s] [Line: %i]"), ErrorString, ANSI_TO_TCHAR(File), Line);
#endif
		return;
	}

	// If we determine that we have not sent a report for this ensure yet, send the report below.
	bool bShouldSendNewReport = false;

	// No debugger attached, so generate a call stack and submit a crash report
	// Walk the stack and dump it to the allocated memory.
	const SIZE_T StackTraceSize = 65535;
	ANSICHAR* StackTrace = (ANSICHAR*) FMemory::SystemMalloc( StackTraceSize );
	if( StackTrace != NULL )
	{
		// Stop checking heartbeat for this thread. Ensure can take a lot of time (when stackwalking)
		// Thread heartbeat will be resumed the next time this thread calls FThreadHeartBeat::Get().HeartBeat();
		// The reason why we don't call HeartBeat() at the end of this function is that maybe this thread
		// Never had a heartbeat checked and may not be sending heartbeats at all which would later lead to a false positives when detecting hangs.
		FThreadHeartBeat::Get().KillHeartBeat();

		{
#if STATS
			FString StackWalkPerfMessage = FString::Printf(TEXT("FPlatformStackWalk::StackWalkAndDump"));
			SCOPE_LOG_TIME_IN_SECONDS(*StackWalkPerfMessage, nullptr)
#endif
			StackTrace[0] = 0;
			FPlatformStackWalk::StackWalkAndDumpEx( StackTrace, StackTraceSize, CALLSTACK_IGNOREDEPTH, FGenericPlatformStackWalk::EStackWalkFlags::FlagsUsedWhenHandlingEnsure );
		}

		// Create a final string that we'll output to the log (and error history buffer)
		TCHAR ErrorMsg[16384];
		FCString::Snprintf( ErrorMsg, ARRAY_COUNT( ErrorMsg ), TEXT( "Ensure condition failed: %s [File:%s] [Line: %i]" ) LINE_TERMINATOR TEXT( "%s" ) LINE_TERMINATOR TEXT( "Stack: " ) LINE_TERMINATOR, ANSI_TO_TCHAR( Expr ), ANSI_TO_TCHAR( File ), Line, Msg );

		// Also append the stack trace
		FCString::Strncat( ErrorMsg, ANSI_TO_TCHAR(StackTrace), ARRAY_COUNT(ErrorMsg) - 1 );
		FMemory::SystemFree( StackTrace );

		// Dump the error and flush the log.
#if !NO_LOGGING
		OutputMultiLineCallstack(__FILE__, __LINE__, LogOutputDevice.GetCategoryName(), TEXT("=== Handled ensure: ==="), ErrorMsg, ELogVerbosity::Error);
#endif
		GLog->Flush();

		// Submit the error report to the server! (and display a balloon in the system tray)
		{
			// How many unique previous errors we should keep track of
			const uint32 MaxPreviousErrorsToTrack = 4;
			static uint32 StaticPreviousErrorCount = 0;
			if( StaticPreviousErrorCount < MaxPreviousErrorsToTrack )
			{
				// Check to see if we've already reported this error.  No point in blasting the server with
				// the same error over and over again in a single application session.
				bool bHasErrorAlreadyBeenReported = false;

				// Static: Array of previous unique error message CRCs
				static uint32 StaticPreviousErrorCRCs[ MaxPreviousErrorsToTrack ];

				// Compute CRC of error string.  Note that along with the call stack, this includes the message
				// string passed to the macro, so only truly redundant errors will go unreported.  Though it also
				// means you shouldn't pass loop counters to ensureMsgf(), otherwise failures may spam the server!
				const uint32 ErrorStrCRC = FCrc::StrCrc_DEPRECATED( ErrorMsg );

				for( uint32 CurErrorIndex = 0; CurErrorIndex < StaticPreviousErrorCount; ++CurErrorIndex )
				{
					if( StaticPreviousErrorCRCs[ CurErrorIndex ] == ErrorStrCRC )
					{
						// Found it!  This is a redundant error message.
						bHasErrorAlreadyBeenReported = true;
						break;
					}
				}

				// Add the element to the list and bump the count
				StaticPreviousErrorCRCs[ StaticPreviousErrorCount++ ] = ErrorStrCRC;

				if( !bHasErrorAlreadyBeenReported )
				{
#if STATS
					FString SubmitErrorReporterfMessage = FString::Printf(TEXT("SubmitErrorReport"));
					SCOPE_LOG_TIME_IN_SECONDS(*SubmitErrorReporterfMessage, nullptr)
#endif

					FCoreDelegates::OnHandleSystemEnsure.Broadcast();

					FPlatformMisc::SubmitErrorReport( ErrorMsg, EErrorReportMode::Balloon );

					bShouldSendNewReport = true;
				}
			}
		}
	}
	else
	{
		// If we fail to generate the string to identify the crash we don't know if we should skip sending the report,
		// so we will just send the report anyway.
		bShouldSendNewReport = true;

		// Add message to log even without stacktrace. It is useful for testing fail on ensure.
#if !NO_LOGGING
		UE_LOG(LogOutputDevice, Error, TEXT("%s [File:%s] [Line: %i]"), ErrorString, ANSI_TO_TCHAR(File), Line);
#endif
	}

	if ( bShouldSendNewReport )
	{
#if STATS
		FString SendNewReportMessage = FString::Printf(TEXT("SendNewReport"));
		SCOPE_LOG_TIME_IN_SECONDS(*SendNewReportMessage, nullptr)
#endif

#if PLATFORM_DESKTOP
		FScopeLock Lock( &FailDebugCriticalSection );

		NewReportEnsure( GErrorMessage );

		GErrorHist[0] = 0;
		GErrorMessage[0] = 0;
		GErrorExceptionDescription[0] = 0;
#endif
	}
}

#endif // DO_CHECK || DO_GUARD_SLOW

void VARARGS FDebug::AssertFailed(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format/* = TEXT("")*/, ...)
{
	if (GIsCriticalError)
	{
		return;
	}

	// This is not perfect because another thread might crash and be handled before this assert
	// but this static varible will report the crash as an assert. Given complexity of a thread
	// aware solution, this should be good enough. If crash reports are obviously wrong we can
	// look into fixing this.
	bHasAsserted = true;

	TCHAR DescriptionString[4096];
	GET_VARARGS(DescriptionString, ARRAY_COUNT(DescriptionString), ARRAY_COUNT(DescriptionString) - 1, Format, Format);

	TCHAR ErrorString[MAX_SPRINTF];
	FCString::Sprintf(ErrorString, TEXT("Assertion failed: %s"), ANSI_TO_TCHAR(Expr));
	GError->Logf(TEXT("Assertion failed: %s") FILE_LINE_DESC TEXT("\n%s\n"), ErrorString, ANSI_TO_TCHAR(File), Line, DescriptionString);
}

#if DO_CHECK || DO_GUARD_SLOW
bool VARARGS FDebug::OptionallyLogFormattedEnsureMessageReturningFalse( bool bLog, const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* FormattedMsg, ... )
{
	if (bLog)
	{
		const int32 TempStrSize = 4096;
		TCHAR TempStr[ TempStrSize ];
		GET_VARARGS( TempStr, TempStrSize, TempStrSize - 1, FormattedMsg, FormattedMsg );
		EnsureFailed( Expr, File, Line, TempStr );
	}
	
	return false;
}
#endif

void VARARGS LowLevelFatalErrorHandler(const ANSICHAR* File, int32 Line, const TCHAR* Format, ...)
{
	TCHAR DescriptionString[4096];
	GET_VARARGS( DescriptionString, ARRAY_COUNT(DescriptionString), ARRAY_COUNT(DescriptionString)-1, Format, Format );

	StaticFailDebug(TEXT("LowLevelFatalError"), File, Line, DescriptionString, false);
}
