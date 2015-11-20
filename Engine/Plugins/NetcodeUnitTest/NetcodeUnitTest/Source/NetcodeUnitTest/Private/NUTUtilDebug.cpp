// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "ClientUnitTest.h"
#include "NUTActor.h"

#include "Net/UnitTestNetConnection.h"

#include "NUTUtilDebug.h"
#include "Net/NUTUtilNet.h"


/**
 * Globals
 */

FStackTraceManager GTraceManager;
FLogStackTraceManager GLogTraceManager;


/**
 * FScopedLog
 */

void FScopedLog::InternalConstruct(const TArray<FString>& InLogCategories, UClientUnitTest* InUnitTest, bool bInRemoteLogging)
{
	LogCategories = InLogCategories;
	UnitTest = InUnitTest;
	bRemoteLogging = bInRemoteLogging;


	// If you want to do remote logging, you MUST specify the client unit test doing the logging
	if (bRemoteLogging)
	{
		UNIT_ASSERT(UnitTest != NULL);
		UNIT_ASSERT(UnitTest->UnitConn != NULL);
	}


	UNetConnection* UnitConn = (UnitTest != NULL ? UnitTest->UnitConn : NULL);

	// Flush all current packets, so the log messages only relate to scoped code
	if (UnitConn != NULL)
	{
		UnitConn->FlushNet();
	}


	// If specified, enable logs remotely
	if (bRemoteLogging)
	{
		FOutBunch* ControlChanBunch = NUTNet::CreateChannelBunch(UnitTest->ControlBunchSequence, UnitConn, CHTYPE_Control, 0);

		uint8 ControlMsg = NMT_NUTControl;
		uint8 ControlCmd = ENUTControlCommand::Command_NoResult;
		FString Cmd = TEXT("");

		for (auto CurCategory : LogCategories)
		{
			Cmd = TEXT("Log ") + CurCategory + TEXT(" All");

			*ControlChanBunch << ControlMsg;
			*ControlChanBunch << ControlCmd;
			*ControlChanBunch << Cmd;
		}

		NUTNet::SendControlBunch(UnitConn, *ControlChanBunch);


		// Need to flush again now to get the above parsed on server first
		UnitConn->FlushNet();
	}


	// Now enable local logging
	FString Cmd = TEXT("");

	for (auto CurCategory : LogCategories)
	{
		Cmd = TEXT("Log ") + CurCategory + TEXT(" All");

		GEngine->Exec((UnitTest != NULL ? UnitTest->UnitWorld : NULL), *Cmd);
	}
}

FScopedLog::~FScopedLog()
{
	UNetConnection* UnitConn = (UnitTest != NULL ? UnitTest->UnitConn : NULL);

	// Flush all built-up packets
	if (UnitConn != NULL)
	{
		UnitConn->FlushNet();
	}


	// Reset local logging
	FString Cmd = TEXT("");

	for (int32 i=LogCategories.Num()-1; i>=0; i--)
	{
		Cmd = TEXT("Log ") + LogCategories[i] + TEXT(" Default");

		GEngine->Exec((UnitTest != NULL ? UnitTest->UnitWorld : NULL), *Cmd);
	}


	// Reset remote logging (and flush immediately)
	if (bRemoteLogging)
	{
		FOutBunch* ControlChanBunch = NUTNet::CreateChannelBunch(UnitTest->ControlBunchSequence, UnitConn, CHTYPE_Control, 0);

		uint8 ControlMsg = NMT_NUTControl;
		uint8 ControlCmd = ENUTControlCommand::Command_NoResult;

		for (int32 i=LogCategories.Num()-1; i>=0; i--)
		{
			Cmd = TEXT("Log ") + LogCategories[i] + TEXT(" Default");

			*ControlChanBunch << ControlMsg;
			*ControlChanBunch << ControlCmd;
			*ControlChanBunch << Cmd;
		}

		NUTNet::SendControlBunch(UnitConn, *ControlChanBunch);

		UnitConn->FlushNet();
	}
}


#if !UE_BUILD_SHIPPING
/**
 * FProcessEventHookBase
 */

FProcessEventHookBase::FProcessEventHookBase()
	: OrigEventHook()
{
	if (AActor::ProcessEventDelegate.IsBound())
	{
		OrigEventHook = AActor::ProcessEventDelegate;
	}

	AActor::ProcessEventDelegate.BindRaw(this, &FProcessEventHookBase::InternalProcessEventHook);
}

FProcessEventHookBase::~FProcessEventHookBase()
{
	AActor::ProcessEventDelegate = OrigEventHook;
	OrigEventHook.Unbind();
}

bool FProcessEventHookBase::InternalProcessEventHook(AActor* Actor, UFunction* Function, void* Parameters)
{
	bool bReturnVal = false;

	// If there was originally already a ProcessEvent hook in place, transparently pass on the event, so it's not disrupted
	if (OrigEventHook.IsBound())
	{
		bReturnVal = OrigEventHook.Execute(Actor, Function, Parameters);
	}

	ProcessEventHook(Actor, Function, Parameters);

	return bReturnVal;
}

void FScopedProcessEventLog::ProcessEventHook(AActor* Actor, UFunction* Function, void* Parameters)
{
	UE_LOG(LogUnitTest, Log, TEXT("FScopedProcessEventLog: Actor: %s, Function: %s"),
			(Actor != NULL ? *Actor->GetName() : TEXT("NULL")),
			*Function->GetName());
}
#endif


/**
 * FNUTStackTrace
 */

FNUTStackTrace::FNUTStackTrace(FString InTraceName)
	: TraceName(InTraceName)
	, Tracker()
{
	Tracker.ResetTracking();
}

FNUTStackTrace::~FNUTStackTrace()
{
	Tracker.ResetTracking();
}
void FNUTStackTrace::Enable()
{
	if (!IsTrackingEnabled())
	{
		Tracker.ToggleTracking();
	}
}

void FNUTStackTrace::Disable()
{
	if (IsTrackingEnabled())
	{
		Tracker.ToggleTracking();
	}
}

void FNUTStackTrace::AddTrace(bool bLogAdd/*=false*/)
{
	if (IsTrackingEnabled())
	{
		if (bLogAdd)
		{
			UE_LOG(LogUnitTest, Log, TEXT("Adding stack trace for TraceName '%s'."), *TraceName);
		}

		Tracker.CaptureStackTrace(TRACE_IGNORE_DEPTH);
	}
}

void FNUTStackTrace::Dump(bool bKeepTraceHistory/*=false*/)
{
	UE_LOG(LogUnitTest, Log, TEXT("Dumping tracked stack traces for TraceName '%s':"), *TraceName);

	Tracker.DumpStackTraces(0, *GLog);

	if (!bKeepTraceHistory)
	{
		Tracker.ResetTracking();
	}
}


/**
 * FStackTraceManager
 */

FStackTraceManager::FStackTraceManager()
	: Traces()
{
}

FStackTraceManager::~FStackTraceManager()
{
	for (auto It=Traces.CreateConstIterator(); It; ++It)
	{
		FNUTStackTrace* Trace = It->Value;

		if (Trace != NULL)
		{
			delete Trace;
		}
	}

	Traces.Empty();
}

void FStackTraceManager::Enable(FString TraceName)
{
	FNUTStackTrace* Trace = GetOrCreateTrace(TraceName);

	Trace->Enable();
}

void FStackTraceManager::Disable(FString TraceName)
{
	FNUTStackTrace* Trace = GetTrace(TraceName);

	if (Trace != NULL)
	{
		Trace->Disable();
	}
	else
	{
		UE_LOG(LogUnitTest, Log, TEXT("Trace disable: No trace tracking found for TraceName '%s'."), *TraceName);
	}
}

void FStackTraceManager::AddTrace(FString TraceName, bool bLogAdd/*=false*/, bool bDump/*=false*/, bool bStartDisabled/*=false*/)
{
	bool bIsNewTrace = false;
	FNUTStackTrace* Trace = GetOrCreateTrace(TraceName, &bIsNewTrace);

	if (bIsNewTrace)
	{
		if (bStartDisabled)
		{
			Trace->Disable();
		}
		else
		{
			Trace->Enable();
		}
	}

	if (Trace->IsTrackingEnabled())
	{
		Trace->AddTrace(bLogAdd);

		if (bDump)
		{
			Trace->Dump(true);
		}
	}
}

void FStackTraceManager::Dump(FString TraceName, bool bKeepTraceHistory/*=false*/, bool bKeepTracking/*=true*/)
{
	FNUTStackTrace* Trace = GetTrace(TraceName);

	if (Trace != NULL)
	{
		Trace->Dump(bKeepTraceHistory);

		if (!bKeepTracking)
		{
			delete Trace;
			Traces.FindAndRemoveChecked(TraceName);
		}
	}
	else
	{
		UE_LOG(LogUnitTest, Log, TEXT("No trace tracking found for TraceName '%s'."), *TraceName);
	}
}

void FStackTraceManager::Clear(FString TraceName)
{
	FNUTStackTrace* Trace = GetTrace(TraceName);

	if (Trace != NULL)
	{
		delete Trace;
		Traces.FindAndRemoveChecked(TraceName);
	}
	else
	{
		UE_LOG(LogUnitTest, Log, TEXT("No trace tracking found for TraceName '%s'."), *TraceName);
	}
}

void FStackTraceManager::DumpAll(bool bKeepTraceHistory/*=false*/, bool bKeepTracking/*=true*/)
{
	UE_LOG(LogUnitTest, Log, TEXT("Dumping all tracked stack traces:"));

	for (auto It=Traces.CreateIterator(); It; ++It)
	{
		FNUTStackTrace* Trace = It->Value;

		if (Trace != NULL)
		{
			Trace->Dump(bKeepTraceHistory);

			if (!bKeepTracking)
			{
				delete Trace;
				It.RemoveCurrent();
			}
		}
	}
}

void FStackTraceManager::TraceAndDump(FString TraceName)
{
	FNUTStackTrace* Trace = GetTrace(TraceName);

	if (Trace == NULL || Trace->IsTrackingEnabled())
	{
		UE_LOG(LogUnitTest, Log, TEXT("Dumping once-off stack trace for TraceName '%s':"), *TraceName);

		FStackTracker TempTracker(NULL, NULL, true);

		TempTracker.CaptureStackTrace(TRACE_IGNORE_DEPTH);
		TempTracker.DumpStackTraces(0, *GLog);
		TempTracker.ResetTracking();
	}
}


/**
 * NUTDebug
 */

FString NUTDebug::HexDump(const TArray<uint8>& InBytes, bool bDumpASCII/*=true*/, bool bDumpOffset/*=true*/)
{
	FString ReturnValue;

	if (bDumpOffset)
	{
		// Spacer for row offsets
		ReturnValue += TEXT("        ");

		// Spacer between offsets and hex
		ReturnValue += TEXT("  ");

		// Top line offsets
		ReturnValue += TEXT("00 01 02 03  04 05 06 07  08 09 0A 0B  0C 0D 0E 0F");

		ReturnValue += LINE_TERMINATOR LINE_TERMINATOR;
	}

	for (int32 ByteRow=0; ByteRow<((InBytes.Num()-1) / 16)+1; ByteRow++)
	{
		FString HexRowStr;
		FString ASCIIRowStr;

		for (int32 ByteColumn=0; ByteColumn<16; ByteColumn++)
		{
			int32 ByteElement = (ByteRow*16) + ByteColumn;
			uint8 CurByte = (ByteElement < InBytes.Num() ? InBytes[ByteElement] : 0);

			if (ByteElement < InBytes.Num())
			{
				HexRowStr += FString::Printf(TEXT("%02X"), CurByte);

				// Printable ASCII-range limit
				if (bDumpASCII)
				{
					if (CurByte >= 0x20 && CurByte <= 0x7E)
					{
						TCHAR CurChar[2];

						CurChar[0] = CurByte;
						CurChar[1] = 0;

						ASCIIRowStr += CurChar;
					}
					else
					{
						ASCIIRowStr += TEXT(".");
					}
				}
			}
			else
			{
				HexRowStr += TEXT("  ");

				if (bDumpASCII)
				{
					ASCIIRowStr += TEXT(" ");
				}
			}


			// Add padding
			if (ByteColumn < 15)
			{
				if (ByteColumn > 0 && ((ByteColumn + 1) % 4) == 0)
				{
					HexRowStr += TEXT("  ");
				}
				else
				{
					HexRowStr += TEXT(" ");
				}
			}
		}


		// Add left-hand offset
		if (bDumpOffset)
		{
			ReturnValue += FString::Printf(TEXT("%08X"), ByteRow*16);

			// Spacer between offsets and hex
			ReturnValue += TEXT("  ");
		}

		// Add hex row
		ReturnValue += HexRowStr;

		// Add ASCII row
		if (bDumpASCII)
		{
			// Spacer between hex and ASCII
			ReturnValue += TEXT("  ");

			ReturnValue += ASCIIRowStr;
		}

		ReturnValue += LINE_TERMINATOR;
	}

	return ReturnValue;
}


