// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocLeakDetection.cpp: Helper class to track memory allocations
=============================================================================*/

#include "CorePrivatePCH.h"
#include "MemoryMisc.h"
#include "MallocLeakDetection.h"

#if MALLOC_LEAKDETECTION

FAutoConsoleCommand InvalidateCachedShaders(
	TEXT("MallocLeak"),
	TEXT("Usage:\n")
	TEXT("'MallocLeak Start'  Begins accumulating unique callstacks\n")
	TEXT("'MallocLeak Stop [Size]'  Dumps outstanding unique callstacks and stops accumulation (with an optional filter size in bytes, if not specified 128 KB is assumed).  Also clears data.\n")
	TEXT("'MallocLeak Clear'  Clears all accumulated data.  Does not change start/stop state.\n")
	TEXT("'MallocLeak Dump [Size]' Dumps oustanding unique callstacks with optional size filter in bytes (if size is not specified it is assumed to be 128 KB).\n"),
	FConsoleCommandWithArgsDelegate::CreateStatic(FMallocLeakDetection::HandleMallocLeakCommand),
	ECVF_Default	
	);

FMallocLeakDetection::FMallocLeakDetection()
	: bCaptureAllocs(false)
	, bDumpOustandingAllocs(false)
{
}

FMallocLeakDetection& FMallocLeakDetection::Get()
{
	static FMallocLeakDetection Singleton;
	return Singleton;
}

FMallocLeakDetection::~FMallocLeakDetection()
{	
}

void FMallocLeakDetection::HandleMallocLeakCommandInternal(const TArray< FString >& Args)
{
	if (Args.Num() >= 1)
	{
		if (Args[0].Compare(TEXT("Start"), ESearchCase::IgnoreCase) == 0)
		{		
			UE_LOG(LogConsoleResponse, Display, TEXT("Starting allocation tracking."));
			SetAllocationCollection(true);
		}
		else if (Args[0].Compare(TEXT("Stop"), ESearchCase::IgnoreCase) == 0)
		{
			uint32 FilterSize = 128 * 1024;
			if (Args.Num() >= 2)
			{
				FilterSize = FCString::Atoi(*Args[1]);
			}

			UE_LOG(LogConsoleResponse, Display, TEXT("Stopping allocation tracking and clearing data."));
			SetAllocationCollection(false);

			UE_LOG(LogConsoleResponse, Display, TEXT("Dumping unique calltacks with %i bytes or more oustanding."), FilterSize);
			DumpOpenCallstacks(FilterSize);

			ClearData();
		}
		else if (Args[0].Compare(TEXT("Clear"), ESearchCase::IgnoreCase) == 0)
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("Clearing tracking data."));
			ClearData();
		}
		else if (Args[0].Compare(TEXT("Dump"), ESearchCase::IgnoreCase) == 0)
		{
			uint32 FilterSize = 128 * 1024;
			if (Args.Num() >= 2)
			{
				FilterSize = FCString::Atoi(*Args[1]);
			}
			UE_LOG(LogConsoleResponse, Display, TEXT("Dumping unique calltacks with %i bytes or more oustanding."), FilterSize);
			DumpOpenCallstacks(FilterSize);
		}
	}
}

void FMallocLeakDetection::HandleMallocLeakCommand(const TArray< FString >& Args)
{
	Get().HandleMallocLeakCommandInternal(Args);
}


void FMallocLeakDetection::AddCallstack(FCallstackTrack& Callstack)
{
	FScopeLock Lock(&AllocatedPointersCritical);
	uint32 CallstackHash = FCrc::MemCrc32(Callstack.CallStack, sizeof(Callstack.CallStack), 0);	
	FCallstackTrack& UniqueCallstack = UniqueCallstacks.FindOrAdd(CallstackHash);
	//if we had a hash collision bail and lose the data rather than corrupting existing data.
	if (UniqueCallstack.Count > 0 && UniqueCallstack != Callstack)
	{
		ensureMsgf(false, TEXT("Callstack hash collision.  Throwing away new stack."));
		return;
	}

	if (UniqueCallstack.Count == 0)
	{
		UniqueCallstack = Callstack;
	}
	else
	{
		UniqueCallstack.Size += Callstack.Size;
	}
	UniqueCallstack.Count++;	
}

void FMallocLeakDetection::RemoveCallstack(FCallstackTrack& Callstack)
{
	FScopeLock Lock(&AllocatedPointersCritical);
	uint32 CallstackHash = FCrc::MemCrc32(Callstack.CallStack, sizeof(Callstack.CallStack), 0);
	FCallstackTrack* UniqueCallstack = UniqueCallstacks.Find(CallstackHash);
	if (UniqueCallstack)
	{
		UniqueCallstack->Count--;
		UniqueCallstack->Size -= Callstack.Size;
		if (UniqueCallstack->Count == 0)
		{
			UniqueCallstack = nullptr;
			UniqueCallstacks.Remove(CallstackHash);
		}
	}
}

void FMallocLeakDetection::SetAllocationCollection(bool bEnabled)
{
	FScopeLock Lock(&AllocatedPointersCritical);
	bCaptureAllocs = bEnabled;
}

void FMallocLeakDetection::DumpOpenCallstacks(uint32 FilterSize)
{
	//could be called when OOM so avoid UE_LOG functions.
	FScopeLock Lock(&AllocatedPointersCritical);
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Dumping out of %i possible open callstacks filtered with more than %u bytes on frame: %i\n"), UniqueCallstacks.Num(), FilterSize, (int32)GFrameCounter);
	const int32 MaxCallstackLineChars = 2048;
	char CallstackString[MaxCallstackLineChars];
	TCHAR CallstackStringWide[MaxCallstackLineChars];
	FMemory::Memzero(CallstackString);
	for (const auto& Pair : UniqueCallstacks)
	{
		const FCallstackTrack& Callstack = Pair.Value;
		if (Callstack.Size >= FilterSize)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("AllocSize: %i, Num: %i, FirstFrameEverAllocated: %i\n"), Callstack.Size, Callstack.Count, Callstack.FrameNumber);
			for (int32 i = 0; i < FCallstackTrack::Depth; ++i)
			{
				FPlatformStackWalk::ProgramCounterToHumanReadableString(i, Callstack.CallStack[i], CallstackString, MaxCallstackLineChars);
				//convert ansi -> tchar without mallocs in case we are in OOM handler.
				for (int32 CurrChar = 0; CurrChar < MaxCallstackLineChars; ++CurrChar)
				{
					CallstackStringWide[CurrChar] = CallstackString[CurrChar];
				}
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s\n"), CallstackStringWide);
				FMemory::Memzero(CallstackString);
			}
		}
	}
}

void FMallocLeakDetection::ClearData()
{
	FScopeLock Lock(&AllocatedPointersCritical);
	OpenPointers.Empty();
	UniqueCallstacks.Empty();
}

bool FMallocLeakDetection::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{		
	return false;
}

void FMallocLeakDetection::Malloc(void* Ptr, SIZE_T Size)
{
	if (Ptr)
	{
		if (bCaptureAllocs)
		{
			FScopeLock Lock(&AllocatedPointersCritical);
			if (!bRecursive)
			{
				bRecursive = true;
				FCallstackTrack Callstack;
				FPlatformStackWalk::CaptureStackBackTrace(Callstack.CallStack, FCallstackTrack::Depth);
				Callstack.FrameNumber = GFrameCounter;
				Callstack.Size = Size;
				AddCallstack(Callstack);
				OpenPointers.Add(Ptr, Callstack);
				bRecursive = false;
			}
		}		
	}
}

void FMallocLeakDetection::Realloc(void* OldPtr, void* NewPtr, SIZE_T Size)
{	
	if (bCaptureAllocs)
	{
		FScopeLock Lock(&AllocatedPointersCritical);		
		Free(OldPtr);
		Malloc(NewPtr, Size);
	}
}

void FMallocLeakDetection::Free(void* Ptr)
{
	if (Ptr)
	{
		if (bCaptureAllocs)
		{
			FScopeLock Lock(&AllocatedPointersCritical);			
			if (!bRecursive)
			{
				bRecursive = true;
				FCallstackTrack* Callstack = OpenPointers.Find(Ptr);
				if (Callstack)
				{
					RemoveCallstack(*Callstack);
				}
				OpenPointers.Remove(Ptr);
				bRecursive = false;
			}
		}
	}
}

FMallocLeakDetectionProxy::FMallocLeakDetectionProxy(FMalloc* InMalloc)
	: UsedMalloc(InMalloc)
	, Verify(FMallocLeakDetection::Get())
{
	
}

#endif // MALLOC_LEAKDETECTION