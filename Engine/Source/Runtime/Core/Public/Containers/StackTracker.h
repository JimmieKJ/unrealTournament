// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UMemoryDefines.h"


/** Whether array slack is being tracked. */
#define TRACK_ARRAY_SLACK 0


struct FSlackTrackData
{
	//// Because this code is used for a number of metric gathering tasks that are all not just
	//// count the total number / avg per frame   We will just add in the specific data that we want to 
	//// use elsewhere

	uint64 NumElements;

	/** NumSlackElements in DefaultCalculateSlack call */
	uint64 NumSlackElements;

	//uint64 Foo;

	uint64 CurrentSlackNum;

	// maybe store off policy also

};


/**
 * Stack tracker. Used to identify callstacks at any point in the codebase.
 */
struct FStackTracker
{
public:
	/** Maximum number of backtrace depth. */
	static const int32 MAX_BACKTRACE_DEPTH = 50;
	/** Helper structure to capture callstack addresses and stack count. */
	struct FCallStack
	{
		/** Stack count, aka the number of calls to CalculateStack */
		int64 StackCount;
		/** Program counter addresses for callstack. */
		uint64 Addresses[MAX_BACKTRACE_DEPTH];
		/** User data to store with the stack trace for later use */
		void* UserData;
	};

	/** Used to optionally update the information currently stored with the callstack */
	typedef void (*StackTrackerUpdateFn)( const FCallStack& CallStack, void* UserData);
	/** Used to optionally report information based on the current stack */
	typedef void (*StackTrackerReportFn)( const FCallStack& CallStack, uint64 TotalStackCount, FOutputDevice& Ar );

	/** Constructor, initializing all member variables */
	FStackTracker(StackTrackerUpdateFn InUpdateFn = NULL, StackTrackerReportFn InReportFn = NULL, bool bInIsEnabled = false)
		:	bAvoidCapturing(false)
		,	bIsEnabled(bInIsEnabled)
		,	StartFrameCounter(0)
		,	StopFrameCounter(0)
		,   UpdateFn(InUpdateFn)
		,   ReportFn(InReportFn)
	{ }

	/**
	 * Captures the current stack and updates stack tracking information.
	 * optionally stores a user data pointer that the tracker will take ownership of and delete upon reset
	 * you must allocate the memory with FMemory::Malloc()
	 */
	CORE_API void CaptureStackTrace( int32 EntriesToIgnore = 2, void* UserData = NULL );

	/**
	 * Dumps capture stack trace summary to the passed in log.
	 */
	CORE_API void DumpStackTraces( int32 StackThreshold, FOutputDevice& Ar );

	/** Resets stack tracking. Deletes all user pointers passed in via CaptureStackTrace() */
	CORE_API void ResetTracking();

	/** Toggles tracking. */
	CORE_API void ToggleTracking();

private:

	/** Array of unique callstacks. */
	TArray<FCallStack> CallStacks;
	/** Mapping from callstack CRC to index in callstack array. */
	TMap<uint32,int32> CRCToCallStackIndexMap;
	/** Whether we are currently capturing or not, used to avoid re-entrancy. */
	bool bAvoidCapturing;
	/** Whether stack tracking is enabled. */
	bool bIsEnabled;
	/** Frame counter at the time tracking was enabled. */
	uint64 StartFrameCounter;
	/** Frame counter at the time tracking was disabled. */
	uint64 StopFrameCounter;

	/** Used to optionally update the information currently stored with the callstack */
	StackTrackerUpdateFn UpdateFn;
	/** Used to optionally report information based on the current stack */
	StackTrackerReportFn ReportFn;
};


