// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealMemory.h"

#ifndef __MEMORYBASE_H__
#define __MEMORYBASE_H__


/** The global memory allocator. */
CORE_API extern class FMalloc* GMalloc;

/** Global FMallocProfiler variable to allow multiple malloc profilers to communicate. */
MALLOC_PROFILER( CORE_API extern class FMallocProfiler* GMallocProfiler; )

/** Holds generic memory stats, internally implemented as a map. */
struct FGenericMemoryStats;


/**
 * Inherit from FUseSystemMallocForNew if you want your objects to be placed in memory
 * alloced by the system malloc routines, bypassing GMalloc. This is e.g. used by FMalloc
 * itself.
 */
class CORE_API FUseSystemMallocForNew
{
public:
	/**
	 * Overloaded new operator using the system allocator.
	 *
	 * @param	Size	Amount of memory to allocate (in bytes)
	 * @return			A pointer to a block of memory with size Size or nullptr
	 */
	void* operator new( size_t Size )
	{
		return FMemory::SystemMalloc( Size );
	}

	/**
	 * Overloaded delete operator using the system allocator
	 *
	 * @param	Ptr		Pointer to delete
	 */
	void operator delete( void* Ptr )
	{
		FMemory::SystemFree( Ptr );
	}

	/**
	 * Overloaded array new operator using the system allocator.
	 *
	 * @param	Size	Amount of memory to allocate (in bytes)
	 * @return			A pointer to a block of memory with size Size or nullptr
	 */
	void* operator new[]( size_t Size )
	{
		return FMemory::SystemMalloc( Size );
	}

	/**
	 * Overloaded array delete operator using the system allocator
	 *
	 * @param	Ptr		Pointer to delete
	 */
	void operator delete[]( void* Ptr )
	{
		FMemory::SystemFree( Ptr );
	}
};

/** The global memory allocator's interface. */
class CORE_API FMalloc  : 
	public FUseSystemMallocForNew,
	public FExec
{
public:
	/**
	 *	Initializes stats metadata. We need to do this as soon as possible, but cannot be done in the constructor
	 *	due to the FName::StaticInit
	 */
	virtual void InitializeStatsMetadata();

	/** 
	 * Malloc
	 */
	virtual void* Malloc( SIZE_T Count, uint32 Alignment=DEFAULT_ALIGNMENT ) = 0;

	/** 
	 * Realloc
	 */
	virtual void* Realloc( void* Original, SIZE_T Count, uint32 Alignment=DEFAULT_ALIGNMENT ) = 0;

	/** 
	 * Free
	 */
	virtual void Free( void* Original ) = 0;
		
	/** 
	 * Handles any commands passed in on the command line
	 */
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{ 
		return false; 
	}

	/** Called once per frame, gathers and sets all memory allocator statistics into the corresponding stats. MUST BE THREAD SAFE. */
	virtual void UpdateStats();

	/** Writes allocator stats from the last update into the specified destination. */
	virtual void GetAllocatorStats( FGenericMemoryStats& out_Stats );

	/** Dumps current allocator stats to the log. */
	virtual void DumpAllocatorStats( class FOutputDevice& Ar )
	{
		Ar.Logf( TEXT("Allocator Stats for %s: (not implemented)" ), GetDescriptiveName() );
	}

	/**
	 * Returns if the allocator is guaranteed to be thread-safe and therefore
	 * doesn't need a unnecessary thread-safety wrapper around it.
	 */
	virtual bool IsInternallyThreadSafe() const 
	{ 
		return false; 
	}

	/**
	 * Validates the allocator's heap
	 */
	virtual bool ValidateHeap()
	{
		return( true );
	}

	/**
	* If possible determine the size of the memory allocated at the given address
	*
	* @param Original - Pointer to memory we are checking the size of
	* @param SizeOut - If possible, this value is set to the size of the passed in pointer
	* @return true if succeeded
	*/
	virtual bool GetAllocationSize(void *Original, SIZE_T &SizeOut)
	{
		return false; // Default implementation has no way of determining this
	}

	/**
	 * Gets descriptive name for logging purposes.
	 *
	 * @return pointer to human-readable malloc name
	 */
	virtual const TCHAR* GetDescriptiveName()
	{
		return TEXT("Unspecified allocator");
	}

protected:
	friend struct FCurrentFrameCalls;

	/** Atomically increment total malloc calls. */
	FORCEINLINE void IncrementTotalMallocCalls()
	{
		FPlatformAtomics::InterlockedIncrement( (volatile int32*)&FMalloc::TotalMallocCalls );
	}

	/** Atomically increment total free calls. */
	FORCEINLINE void IncrementTotalFreeCalls()
	{
		FPlatformAtomics::InterlockedIncrement( (volatile int32*)&FMalloc::TotalFreeCalls );
	}

	/** Atomically increment total realloc calls. */
	FORCEINLINE void IncrementTotalReallocCalls()
	{
		FPlatformAtomics::InterlockedIncrement( (volatile int32*)&FMalloc::TotalReallocCalls );
	}

	/** Total number of calls Malloc, if implemented by derived class. */
	static uint32 TotalMallocCalls;
	/** Total number of calls Malloc, if implemented by derived class. */
	static uint32 TotalFreeCalls;
	/** Total number of calls Malloc, if implemented by derived class. */
	static uint32 TotalReallocCalls;
};


#endif
