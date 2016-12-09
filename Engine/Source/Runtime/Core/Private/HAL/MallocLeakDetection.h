// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocLeakDetection.h: Helper class to track memory allocations
=============================================================================*/

#pragma once

#include "CoreTypes.h"
#include "HAL/MemoryBase.h"
#include "HAL/UnrealMemory.h"
#include "Containers/Array.h"
#include "Misc/Crc.h"
#include "Containers/UnrealString.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Misc/ScopeLock.h"

#ifndef MALLOC_LEAKDETECTION
	#define MALLOC_LEAKDETECTION 0
#endif

#if MALLOC_LEAKDETECTION


struct FMallocLeakReportOptions
{
	FMallocLeakReportOptions()
	{
		FMemory::Memzero(this, sizeof(FMallocLeakReportOptions));
	}
	uint32			FilterSize;
	float			LeakRate;
	bool			OnlyNonDeleters;
	uint32			FrameStart;
	uint32			FrameEnd;
	const TCHAR*	OutputFile;
};

/**
 * Maintains a list of all pointers to currently allocated memory.
 */
class FMallocLeakDetection
{

	struct FCallstackTrack
	{
		FCallstackTrack()
		{
			FMemory::Memzero(this, sizeof(FCallstackTrack));
		}
		static const int32 Depth = 32;
		uint64 CallStack[Depth];
		uint32 FirstFame;
		uint32 LastFrame;
		uint64 Size;
		uint32 Count;

		// least square line fit stuff
		uint32 NumCheckPoints;
		float SumOfFramesNumbers;
		float SumOfFramesNumbersSquared;
		float SumOfMemory;
		float SumOfMemoryTimesFrameNumber;

		// least square line results
		float Baseline;
		float BytesPerFrame;


		bool operator==(const FCallstackTrack& Other) const
		{
			bool bEqual = true;
			for (int32 i = 0; i < Depth; ++i)
			{
				if (CallStack[i] != Other.CallStack[i])
				{
					bEqual = false;
					break;
				}
			}
			return bEqual;
		}

		bool operator!=(const FCallstackTrack& Other) const
		{
			return !(*this == Other);
		}

		void GetLinearFit();
		
		uint32 GetHash() const 
		{
			return FCrc::MemCrc32(CallStack, sizeof(CallStack), 0);
		}
	};

private:

	FMallocLeakDetection();
	~FMallocLeakDetection();

	/** Track a callstack */

	/** Stop tracking a callstack */
	void AddCallstack(FCallstackTrack& Callstack);
	void RemoveCallstack(FCallstackTrack& Callstack);

	/** List of all currently allocated pointers */
	TMap<void*, FCallstackTrack> OpenPointers;

	/** List of all unique callstacks with allocated memory */
	TMap<uint32, FCallstackTrack> UniqueCallstacks;

	/** Set of callstacks that are known to delete memory (never cleared) */
	TSet<uint32>	KnownDeleters;

	/** Contexts that are associated with allocations */
	TMap<void*, FString>		PointerContexts;

	/** Stack of contexts */
	struct ContextString { TCHAR Buffer[128]; };
	TArray<ContextString>	Contexts;
		
	/** Critical section for mutating internal data */
	FCriticalSection AllocatedPointersCritical;	

	/** Set during mutating operationms to prevent internal allocations from recursing */
	bool	bRecursive;

	/** Is allocation capture enabled? */
	bool	bCaptureAllocs;

	/** Minimal size to capture? */
	int32	MinAllocationSize;

	SIZE_T	TotalTracked;

public:	

	static FMallocLeakDetection& Get();
	static void HandleMallocLeakCommand(const TArray< FString >& Args);

	/** Enable/disable collection of allocation with an optional filter on allocation size */
	void SetAllocationCollection(bool bEnabled, int32 Size = 0);

	/** Returns state of allocation collection */
	bool IsAllocationCollectionEnabled(void) const { return bCaptureAllocs; }

	/** Clear currently accumulated data */
	void ClearData();

	/** Dumps callstacks that appear to be leaks based on allocation trends */
	int32 DumpPotentialLeakers(const FMallocLeakReportOptions& Options = FMallocLeakReportOptions());

	/** Dumps currently open callstacks */
	int32 DumpOpenCallstacks(const FMallocLeakReportOptions& Options = FMallocLeakReportOptions());

	/** Perform a linear fit checkpoint of all open callstacks */
	void CheckpointLinearFit();


	/** Cmd handler */
	bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);

	/** Handles new allocated pointer */
	void Malloc(void* Ptr, SIZE_T Size);

	/** Handles reallocation */
	void Realloc(void* OldPtr, void* NewPtr, SIZE_T Size);

	/** Removes allocated pointer from list */
	void Free(void* Ptr);	

	/** Push/Pop a context that will be associated with allocations. All open contexts will be displayed alongside
	callstacks in a report.  */
	void PushContext(const FString& Context);
	void PopContext();

	/** Returns */
	void GetOpenCallstacks(TArray<uint32>& OutCallstacks, const FMallocLeakReportOptions& Options = FMallocLeakReportOptions());
};

/**
 * A verifying proxy malloc that takes a malloc to be used and tracks unique callstacks with outstanding allocations
 * to help identify leaks.
 */
class FMallocLeakDetectionProxy : public FMalloc
{
private:
	/** Malloc we're based on, aka using under the hood */
	FMalloc* UsedMalloc;

	/* Verifier object */
	FMallocLeakDetection& Verify;

	FCriticalSection AllocatedPointersCritical;

public:
	explicit FMallocLeakDetectionProxy(FMalloc* InMalloc);	

	static FMallocLeakDetectionProxy& Get();

	virtual void* Malloc(SIZE_T Size, uint32 Alignment) override
	{
		FScopeLock SafeLock(&AllocatedPointersCritical);
		void* Result = UsedMalloc->Malloc(Size, Alignment);
		Verify.Malloc(Result, Size);
		return Result;
	}

	virtual void* Realloc(void* Ptr, SIZE_T NewSize, uint32 Alignment) override
	{
		FScopeLock SafeLock(&AllocatedPointersCritical);
		void* Result = UsedMalloc->Realloc(Ptr, NewSize, Alignment);
		Verify.Realloc(Ptr, Result, NewSize);
		return Result;
	}

	virtual void Free(void* Ptr) override
	{
		if (Ptr)
		{
			FScopeLock SafeLock(&AllocatedPointersCritical);
			Verify.Free(Ptr);
			UsedMalloc->Free(Ptr);
		}
	}

	virtual void InitializeStatsMetadata() override
	{
		UsedMalloc->InitializeStatsMetadata();
	}

	virtual void GetAllocatorStats(FGenericMemoryStats& OutStats) override
	{
		UsedMalloc->GetAllocatorStats(OutStats);
	}

	virtual void DumpAllocatorStats(FOutputDevice& Ar) override
	{
		FScopeLock Lock(&AllocatedPointersCritical);
		//Verify.DumpOpenCallstacks(1024 * 1024);
		UsedMalloc->DumpAllocatorStats(Ar);
	}

	virtual bool ValidateHeap() override
	{
		return UsedMalloc->ValidateHeap();
	}

	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		FScopeLock Lock(&AllocatedPointersCritical);
		if (Verify.Exec(InWorld, Cmd, Ar))
		{
			return true;
		}
		return UsedMalloc->Exec(InWorld, Cmd, Ar);
	}

	virtual bool GetAllocationSize(void* Original, SIZE_T& OutSize) override
	{
		return UsedMalloc->GetAllocationSize(Original, OutSize);
	}

	virtual SIZE_T QuantizeSize(SIZE_T Count, uint32 Alignment) override
	{
		return UsedMalloc->QuantizeSize(Count, Alignment);
	}

	virtual void Trim() override
	{
		return UsedMalloc->Trim();
	}
	virtual void SetupTLSCachesOnCurrentThread() override
	{
		return UsedMalloc->SetupTLSCachesOnCurrentThread();
	}
	virtual void ClearAndDisableTLSCachesOnCurrentThread() override
	{
		return UsedMalloc->ClearAndDisableTLSCachesOnCurrentThread();
	}

	virtual const TCHAR* GetDescriptiveName() override
	{ 
		return UsedMalloc->GetDescriptiveName();
	}

	void Lock()
	{
		AllocatedPointersCritical.Lock();
	}

	void Unlock()
	{
		AllocatedPointersCritical.Unlock();
	}
};

#endif // MALLOC_LEAKDETECTION
