// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "ExceptionHandling.h"
#include "VarargsHelper.h"
#include "OutputDeviceHelper.h"
#include "HAL/ThreadHeartBeat.h"

DEFINE_LOG_CATEGORY_STATIC(LogOutputDevice, Log, All);


namespace
{
	/**
	 * Singleton to only create error title text if needed (and after localization system is in place)
	 */
	const FText& GetDefaultMessageTitle()
	{
		// Will be initialised on first call
		static FText DefaultMessageTitle(NSLOCTEXT("MessageDialog", "DefaultMessageTitle", "Message"));
		return DefaultMessageTitle;
	}
}

const TCHAR* FOutputDevice::VerbosityToString(ELogVerbosity::Type Verbosity)
{
	return FOutputDeviceHelper::VerbosityToString(Verbosity);
}

FString FOutputDevice::FormatLogLine( ELogVerbosity::Type Verbosity, const class FName& Category, const TCHAR* Message /*= nullptr*/, ELogTimes::Type LogTime /*= ELogTimes::None*/, const double Time /*= -1.0*/ )
{
	return FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Message, LogTime, Time);
}

void FOutputDevice::Log( ELogVerbosity::Type Verbosity, const TCHAR* Str )
{
	Serialize( Str, Verbosity, NAME_None );
}
void FOutputDevice::Log( ELogVerbosity::Type Verbosity, const FString& S )
{
	Serialize( *S, Verbosity, NAME_None );
}
void FOutputDevice::Log( const class FName& Category, ELogVerbosity::Type Verbosity, const TCHAR* Str )
{
	Serialize( Str, Verbosity, Category );
}
void FOutputDevice::Log( const class FName& Category, ELogVerbosity::Type Verbosity, const FString& S )
{
	Serialize( *S, Verbosity, Category );
}
void FOutputDevice::Log( const TCHAR* Str )
{
	FScopedCategoryAndVerbosityOverride::FOverride* TLS = FScopedCategoryAndVerbosityOverride::GetTLSCurrent();
	Serialize( Str, TLS->Verbosity, TLS->Category );
}
void FOutputDevice::Log( const FString& S )
{
	FScopedCategoryAndVerbosityOverride::FOverride* TLS = FScopedCategoryAndVerbosityOverride::GetTLSCurrent();
	Serialize( *S, TLS->Verbosity, TLS->Category );
}
void FOutputDevice::Log( const FText& T )
{
	FScopedCategoryAndVerbosityOverride::FOverride* TLS = FScopedCategoryAndVerbosityOverride::GetTLSCurrent();
	Serialize( *T.ToString(), TLS->Verbosity, TLS->Category );
}
/*-----------------------------------------------------------------------------
	Formatted printing and messages.
-----------------------------------------------------------------------------*/


/** Number of top function calls to hide when dumping the callstack as text. */
#if PLATFORM_LINUX

	// Rationale: check() and ensure() handlers have different depth - worse, ensure() can optionally end up calling the same path as check().
	// It is better to show the full callstack as is than accidentaly ignore a part of the problem
	#define CALLSTACK_IGNOREDEPTH 0

#else
	#define CALLSTACK_IGNOREDEPTH 2
#endif // PLATFORM_LINUX

VARARG_BODY( void, FOutputDevice::CategorizedLogf, const TCHAR*, VARARG_EXTRA(const class FName& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity)  )
{
	GROWABLE_LOGF(Serialize(Buffer, Verbosity, Category))
}
VARARG_BODY( void, FOutputDevice::Logf, const TCHAR*, VARARG_EXTRA(ELogVerbosity::Type Verbosity) )
{
	// call serialize with the final buffer
	GROWABLE_LOGF(Serialize(Buffer, Verbosity, NAME_None))
}
VARARG_BODY( void, FOutputDevice::Logf, const TCHAR*, VARARG_NONE )
{
	FScopedCategoryAndVerbosityOverride::FOverride* TLS = FScopedCategoryAndVerbosityOverride::GetTLSCurrent();
	GROWABLE_LOGF(Serialize(Buffer, TLS->Verbosity, TLS->Category))
}

#define FILE_LINE_DESC TEXT(" [File:%s] [Line: %i] ")


/** Critical errors. */
CORE_API FOutputDeviceError* GError = NULL;

/** Lock used to synchronize the fail debug calls. */
static FCriticalSection	FailDebugCriticalSection;

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
void StaticFailDebug( const TCHAR* Error, const ANSICHAR* File, int32 Line, const TCHAR* Description, bool bIsEnsure = false )
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

bool FDebug::bHasAsserted = false;

void FDebug::OutputMultiLineCallstack(const ANSICHAR* File, int32 Line, const FName& LogName, const TCHAR* Heading, TCHAR* Message, ELogVerbosity::Type Verbosity)
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

		StaticFailDebug( ErrorString, File, Line, DescriptionString );
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
		FDebug::OutputMultiLineCallstack(__FILE__, __LINE__, LogOutputDevice.GetCategoryName(), TEXT("=== Handled ensure: ==="), ErrorMsg, ELogVerbosity::Error);
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

void VARARGS FError::LowLevelFatal(const ANSICHAR* File, int32 Line, const TCHAR* Format, ...)
{
	TCHAR DescriptionString[4096];
	GET_VARARGS( DescriptionString, ARRAY_COUNT(DescriptionString), ARRAY_COUNT(DescriptionString)-1, Format, Format );

	StaticFailDebug(TEXT("LowLevelFatalError"),File,Line,DescriptionString);
}

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

void FMessageDialog::Debugf( const FText& Message, const FText* OptTitle )
{
	if( FApp::IsUnattended() == true )
	{
		GLog->Logf( *Message.ToString() );
	}
	else
	{
		FText Title = OptTitle ? *OptTitle : GetDefaultMessageTitle();
		if ( GIsEditor && FCoreDelegates::ModalErrorMessage.IsBound() )
		{
			FCoreDelegates::ModalErrorMessage.Execute(EAppMsgType::Ok, Message, Title);
		}
		else
		{
			FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, *Message.ToString(), *NSLOCTEXT("MessageDialog", "DefaultDebugMessageTitle", "ShowDebugMessagef").ToString() );
		}
	}
}

void FMessageDialog::ShowLastError()
{
	uint32 LastError = FPlatformMisc::GetLastError();

	TCHAR TempStr[MAX_SPRINTF]=TEXT("");
	TCHAR ErrorBuffer[1024];
	FCString::Sprintf( TempStr, TEXT("GetLastError : %d\n\n%s"), LastError, FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, 0) );
	if( FApp::IsUnattended() == true )
	{
		UE_LOG(LogOutputDevice, Fatal, TempStr);
	}
	else
	{
		FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, TempStr, *NSLOCTEXT("MessageDialog", "DefaultSystemErrorTitle", "System Error").ToString() );
	}
}

EAppReturnType::Type FMessageDialog::Open( EAppMsgType::Type MessageType, const FText& Message, const FText* OptTitle )
{
	if( FApp::IsUnattended() == true )
	{
		if (GWarn)
		{
			GWarn->Logf( *Message.ToString() );
		}

		switch(MessageType)
		{
		case EAppMsgType::Ok:
			return EAppReturnType::Ok;
		case EAppMsgType::YesNo:
			return EAppReturnType::No;
		case EAppMsgType::OkCancel:
			return EAppReturnType::Cancel;
		case EAppMsgType::YesNoCancel:
			return EAppReturnType::Cancel;
		case EAppMsgType::CancelRetryContinue:
			return EAppReturnType::Cancel;
		case EAppMsgType::YesNoYesAllNoAll:
			return EAppReturnType::No;
		case EAppMsgType::YesNoYesAllNoAllCancel:
		default:
			return EAppReturnType::Yes;
			break;
		}
	}
	else
	{
		FText Title = OptTitle ? *OptTitle : GetDefaultMessageTitle();
		if ( GIsEditor && !IsRunningCommandlet() && FCoreDelegates::ModalErrorMessage.IsBound() )
		{
			return FCoreDelegates::ModalErrorMessage.Execute( MessageType, Message, Title );
		}
		else
		{
			return FPlatformMisc::MessageBoxExt( MessageType, *Message.ToString(), *Title.ToString() );
		}
	}
}

/*-----------------------------------------------------------------------------
	Exceptions.
-----------------------------------------------------------------------------*/

//
// Throw a string exception with a message.
//
#if HACK_HEADER_GENERATOR
void VARARGS FError::ThrowfImpl(const TCHAR* Fmt, ...)
{
	static TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );
#if !PLATFORM_EXCEPTIONS_DISABLED
	throw( TempStr );
#else
	UE_LOG(LogOutputDevice, Fatal, TEXT("THROW: %s"), TempStr);
#endif
}					
#endif

/** Statics to prevent FMsg::Logf from allocating too much stack memory. */
static FCriticalSection					MsgLogfStaticBufferGuard;
/** Increased from 4096 to fix crashes in the renderthread without autoreporter. */
static TCHAR							MsgLogfStaticBuffer[8192];

VARARG_BODY(void, FMsg::Logf, const TCHAR*, VARARG_EXTRA(const ANSICHAR* File) VARARG_EXTRA(int32 Line) VARARG_EXTRA(const class FName& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity))
{
#if !NO_LOGGING
	if (Verbosity != ELogVerbosity::Fatal)
	{
		// SetColour is routed to GWarn just like the other verbosities and handled in the 
		// device that does the actual printing.
		FOutputDevice* LogDevice = NULL;
		switch (Verbosity)
		{
		case ELogVerbosity::Error:
		case ELogVerbosity::Warning:
		case ELogVerbosity::Display:
		case ELogVerbosity::SetColor:
			if (GWarn)
			{
				LogDevice = GWarn;
				break;
			}
		default:
		{
			LogDevice = GLog;
		}
		break;
		}
		GROWABLE_LOGF(LogDevice->Log(Category, Verbosity, Buffer))
	}
	else
	{
		// Keep Message buffer small, in some cases, this code is executed with 16KB stack.
		TCHAR Message[4096];
		{
			// Simulate Sprintf_s
			// @todo: implement platform independent sprintf_S
			// We're using one big shared static buffer here, so guard against re-entry
			FScopeLock MsgLock(&MsgLogfStaticBufferGuard);
			// Print to a large static buffer so we can keep the stack allocation below 16K
			GET_VARARGS(MsgLogfStaticBuffer, ARRAY_COUNT(MsgLogfStaticBuffer), ARRAY_COUNT(MsgLogfStaticBuffer) - 1, Fmt, Fmt);
			// Copy the message to the stack-allocated buffer)
			FCString::Strncpy(Message, MsgLogfStaticBuffer, ARRAY_COUNT(Message) - 1);
			Message[ARRAY_COUNT(Message) - 1] = '\0';
		}

		StaticFailDebug(TEXT("Fatal error:"), File, Line, Message);
		FDebug::AssertFailed("", File, Line, Message);
	}
#endif
}

VARARG_BODY(void, FMsg::Logf_Internal, const TCHAR*, VARARG_EXTRA(const ANSICHAR* File) VARARG_EXTRA(int32 Line) VARARG_EXTRA(const class FName& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity))
{
#if !NO_LOGGING
	if (Verbosity != ELogVerbosity::Fatal)
	{
		// SetColour is routed to GWarn just like the other verbosities and handled in the 
		// device that does the actual printing.
		FOutputDevice* LogDevice = NULL;
		switch (Verbosity)
		{
		case ELogVerbosity::Error:
		case ELogVerbosity::Warning:
		case ELogVerbosity::Display:
		case ELogVerbosity::SetColor:
			if (GWarn)
			{
				LogDevice = GWarn;
				break;
			}
		default:
		{
			LogDevice = GLog;
		}
		break;
		}
		GROWABLE_LOGF(LogDevice->Log(Category, Verbosity, Buffer))
	}
	else
	{
		// Keep Message buffer small, in some cases, this code is executed with 16KB stack.
		TCHAR Message[4096];
		{
			// Simulate Sprintf_s
			// @todo: implement platform independent sprintf_S
			// We're using one big shared static buffer here, so guard against re-entry
			FScopeLock MsgLock(&MsgLogfStaticBufferGuard);
			// Print to a large static buffer so we can keep the stack allocation below 16K
			GET_VARARGS(MsgLogfStaticBuffer, ARRAY_COUNT(MsgLogfStaticBuffer), ARRAY_COUNT(MsgLogfStaticBuffer) - 1, Fmt, Fmt);
			// Copy the message to the stack-allocated buffer)
			FCString::Strncpy(Message, MsgLogfStaticBuffer, ARRAY_COUNT(Message) - 1);
			Message[ARRAY_COUNT(Message) - 1] = '\0';
		}

		StaticFailDebug(TEXT("Fatal error:"), File, Line, Message);
	}
#endif
}

/** Sends a formatted message to a remote tool. */
void VARARGS FMsg::SendNotificationStringf( const TCHAR *Fmt, ... )
{
	GROWABLE_LOGF(SendNotificationString(Buffer));
}

void FMsg::SendNotificationString( const TCHAR* Message )
{
	FPlatformMisc::LowLevelOutputDebugString(Message);
}
