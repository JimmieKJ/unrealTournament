// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "CorePrivatePCH.h"
#include "ThreadHeartBeat.h"
#include "ExceptionHandling.h"

#ifndef UE_ASSERT_ON_HANG
#define UE_ASSERT_ON_HANG 0
#endif

FThreadHeartBeat::FThreadHeartBeat()
: Thread(nullptr)
, bReadyToCheckHeartbeat(false)
, HangDuration(25.0)
{
	if (GConfig)
	{
		GConfig->GetDouble(TEXT("Core.System"), TEXT("HangDuration"), HangDuration, GEngineIni);
		const double MinHangDuration = 5.0;
		if (HangDuration > 0.0 && HangDuration < 5.0)
		{
			UE_LOG(LogCore, Warning, TEXT("HangDuration is set to %.4llfs which is a very short time for hang detection. Changing to %.2llfs."), HangDuration, MinHangDuration);
			HangDuration = MinHangDuration;
		}
	}

	const bool bAllowThreadHeartBeat = FPlatformMisc::AllowThreadHeartBeat() && HangDuration > 0.0;

	// We don't care about programs for now so no point in spawning the extra thread
#if !IS_PROGRAM
	if (bAllowThreadHeartBeat && FPlatformProcess::SupportsMultithreading())
	{
		Thread = FRunnableThread::Create(this, TEXT("FHeartBeatThread"), 0, TPri_BelowNormal);
	}
#endif

	if (!bAllowThreadHeartBeat)
	{
		// Disable the check
		HangDuration = 0.0;
	}
}

FThreadHeartBeat::~FThreadHeartBeat()
{
	delete Thread;
	Thread = nullptr;
}

FThreadHeartBeat& FThreadHeartBeat::Get()
{
	static FThreadHeartBeat Singleton;
	return Singleton;
}

	//~ Begin FRunnable Interface.
bool FThreadHeartBeat::Init()
{
	return true;
}

uint32 FThreadHeartBeat::Run()
{
	while (StopTaskCounter.GetValue() == 0)
	{
		uint32 ThreadThatHung = CheckHeartBeat();
		if (ThreadThatHung != FThreadHeartBeat::InvalidThreadId)
		{
			const SIZE_T StackTraceSize = 65535;
			ANSICHAR* StackTrace = (ANSICHAR*)GMalloc->Malloc(StackTraceSize);
			StackTrace[0] = 0;
			// Walk the stack and dump it to the allocated memory. This process usually allocates a lot of memory.
			FPlatformStackWalk::ThreadStackWalkAndDump(StackTrace, StackTraceSize, 0, ThreadThatHung);
			FString StackTraceText(StackTrace);
			TArray<FString> StackLines;
			StackTraceText.ParseIntoArrayLines(StackLines);

			// Dump the callstack and the thread name to log
			FString ThreadName(ThreadThatHung == GGameThreadId ? TEXT("GameThread") : FThreadManager::Get().GetThreadName(ThreadThatHung));
			if (ThreadName.IsEmpty())
			{
				ThreadName = FString::Printf(TEXT("unknown thread (%u)"), ThreadThatHung);
			}
			UE_LOG(LogCore, Error, TEXT("Hang detected on %s (thread hasn't sent a heartbeat for %.2llf seconds):"), *ThreadName, HangDuration);
			for (FString& StackLine : StackLines)
			{
				UE_LOG(LogCore, Error, TEXT("  %s"), *StackLine);
			}
			
			// Assert (on the current thread unfortunately) with a trimmed stack.
			FString StackTrimmed;
			for (int32 LineIndex = 0; LineIndex < StackLines.Num() && StackTrimmed.Len() < 512; ++LineIndex)
			{
				StackTrimmed += TEXT("  ");
				StackTrimmed += StackLines[LineIndex];
				StackTrimmed += LINE_TERMINATOR;
			}

			const FString ErrorMessage = FString::Printf(TEXT("Hang detected on %s:%s%s%sCheck log for full callstack."), *ThreadName, LINE_TERMINATOR, *StackTrimmed, LINE_TERMINATOR);
#if UE_ASSERT_ON_HANG
			UE_LOG(LogCore, Fatal, TEXT("%s"), *ErrorMessage);
#else
			UE_LOG(LogCore, Error, TEXT("%s"), *ErrorMessage);
#if PLATFORM_DESKTOP
			GLog->PanicFlushThreadedLogs();
			// GErrorMessage here is very unfortunate but it's used internally by the crash context code.
			FCString::Strcpy(GErrorMessage, ARRAY_COUNT(GErrorMessage), *ErrorMessage);
			// Skip macros and FDebug, we always want this to fire
			NewReportEnsure(*ErrorMessage);
			GErrorMessage[0] = '\0';
#endif
#endif
		}
		FPlatformProcess::SleepNoStats(0.5f);
	}

	return 0;
}

void FThreadHeartBeat::Stop()
{
	bReadyToCheckHeartbeat = false;
	StopTaskCounter.Increment();
}

void FThreadHeartBeat::Start()
{
	bReadyToCheckHeartbeat = true;
}

void FThreadHeartBeat::HeartBeat()
{
	uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
	FScopeLock HeartBeatLock(&HeartBeatCritical);
	FHeartBeatInfo& HeartBeatInfo = ThreadHeartBeat.FindOrAdd(ThreadId);
	HeartBeatInfo.LastHeartBeatTime = FPlatformTime::Seconds();
}

uint32 FThreadHeartBeat::CheckHeartBeat()
{
	// Editor and debug builds run too slow to measure them correctly
#if !WITH_EDITORONLY_DATA && !IS_PROGRAM && !UE_BUILD_DEBUG
	if (HangDuration > 0.0 && bReadyToCheckHeartbeat && !GIsRequestingExit && !FPlatformMisc::IsDebuggerPresent())
	{
		// Check heartbeat for all threads and return thread ID of the thread that hung.
		const double CurrentTime = FPlatformTime::Seconds();
		FScopeLock HeartBeatLock(&HeartBeatCritical);
		for (TPair<uint32, FHeartBeatInfo>& LastHeartBeat : ThreadHeartBeat)
		{
			FHeartBeatInfo& HeartBeatInfo =  LastHeartBeat.Value;
			if (HeartBeatInfo.SuspendedCount == 0 && (CurrentTime - HeartBeatInfo.LastHeartBeatTime) > HangDuration)
			{
				HeartBeatInfo.LastHeartBeatTime = CurrentTime;
				return LastHeartBeat.Key;
			}
		}
	}
#endif
	return InvalidThreadId;
}

void FThreadHeartBeat::KillHeartBeat()
{
	uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
	FScopeLock HeartBeatLock(&HeartBeatCritical);
	ThreadHeartBeat.Remove(ThreadId);
}

void FThreadHeartBeat::SuspendHeartBeat()
{
	uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
	FScopeLock HeartBeatLock(&HeartBeatCritical);
	FHeartBeatInfo* HeartBeatInfo = ThreadHeartBeat.Find(ThreadId);
	if (HeartBeatInfo)
	{
		HeartBeatInfo->SuspendedCount++;
	}
}
void FThreadHeartBeat::ResumeHeartBeat()
{
	uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
	FScopeLock HeartBeatLock(&HeartBeatCritical);
	FHeartBeatInfo* HeartBeatInfo = ThreadHeartBeat.Find(ThreadId);
	if (HeartBeatInfo)
	{
		check(HeartBeatInfo->SuspendedCount > 0);
		if (--HeartBeatInfo->SuspendedCount == 0)
		{
			HeartBeatInfo->LastHeartBeatTime = FPlatformTime::Seconds();
		}
	}
}
