// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "StackTracker.h"


DEFINE_LOG_CATEGORY_STATIC(LogStackTracker, Log, All);


/**
 * Captures the current stack and updates stack tracking information.
 * optionally stores a user data pointer that the tracker will take ownership of and delete upon reset
 * you must allocate the memory with FMemory::Malloc()
 */
void FStackTracker::CaptureStackTrace(int32 EntriesToIgnore /*=2*/, void* UserData /*=NULL*/)
{
	// Avoid re-rentrancy as the code uses TArray/TMap.
	if( !bAvoidCapturing && bIsEnabled )
	{
		// Scoped true/ false.
		bAvoidCapturing = true;

		// Capture callstack and create CRC.
		uint64* FullBackTrace = NULL;
		FullBackTrace = static_cast<uint64*>(FMemory_Alloca((MAX_BACKTRACE_DEPTH + EntriesToIgnore) * sizeof(uint64)));

		FPlatformStackWalk::CaptureStackBackTrace( FullBackTrace, MAX_BACKTRACE_DEPTH + EntriesToIgnore );
		CA_ASSUME(FullBackTrace);

		// Skip first NUM_ENTRIES_TO_SKIP entries as they are inside this code
		uint64* BackTrace = &FullBackTrace[EntriesToIgnore];
		uint32 CRC = FCrc::MemCrc_DEPRECATED( BackTrace, MAX_BACKTRACE_DEPTH * sizeof(uint64) );
        
		// Use index if found
		int32* IndexPtr = CRCToCallStackIndexMap.Find( CRC );
        
		if( IndexPtr )
		{
			// Increase stack count for existing callstack.
			CallStacks[*IndexPtr].StackCount++;
			if (UpdateFn)
			{
				UpdateFn(CallStacks[*IndexPtr], UserData);
			}

			//We can delete this since the user gives ownership at the beginning of this call
			//and had a chance to update their data inside the above callback
			if (UserData)
			{
				FMemory::Free(UserData);
			}
		}
		// Encountered new call stack, add to array and set index mapping.
		else
		{
			// Add to array and set mapping for future use.
			int32 Index = CallStacks.AddUninitialized();
			CRCToCallStackIndexMap.Add( CRC, Index );

			// Fill in callstack and count.
			FCallStack& CallStack = CallStacks[Index];
			FMemory::Memcpy( CallStack.Addresses, BackTrace, sizeof(uint64) * MAX_BACKTRACE_DEPTH );
			CallStack.StackCount = 1;
			CallStack.UserData = UserData;
		}

		// We're done capturing.
		bAvoidCapturing = false;
	}
}

/**
 * Dumps capture stack trace summary to the passed in log.
 */
void FStackTracker::DumpStackTraces( int32 StackThreshold, FOutputDevice& Ar )
{
	// Avoid distorting results while we log them.
	check( !bAvoidCapturing );
	bAvoidCapturing = true;

	// Make a copy as sorting causes index mismatch with TMap otherwise.
	TArray<FCallStack> SortedCallStacks = CallStacks;
	// Compare function, sorting callstack by stack count in descending order.
	struct FCompareStackCount
	{
		FORCEINLINE bool operator()( const FCallStack& A, const FCallStack& B ) const { return B.StackCount < A.StackCount; }
	};
	// Sort callstacks in descending order by stack count.
	SortedCallStacks.Sort( FCompareStackCount() );

	// Iterate over each callstack to get total stack count.
	uint64 TotalStackCount = 0;
	for( int32 CallStackIndex=0; CallStackIndex<SortedCallStacks.Num(); CallStackIndex++ )
	{
		const FCallStack& CallStack = SortedCallStacks[CallStackIndex];
		TotalStackCount += CallStack.StackCount;
	}

	// Calculate the number of frames we captured.
	int32 FramesCaptured = 0;
	if( bIsEnabled )
	{
		FramesCaptured = GFrameCounter - StartFrameCounter;
	}
	else
	{
		FramesCaptured = StopFrameCounter - StartFrameCounter;
	}

	// Log quick summary as we don't log each individual so totals in CSV won't represent real totals.
	Ar.Logf(TEXT("Captured %i unique callstacks totalling %i function calls over %i frames, averaging %5.2f calls/frame, Avg Per Frame"), SortedCallStacks.Num(), (int32)TotalStackCount, FramesCaptured, (float) TotalStackCount / FramesCaptured);

	// Iterate over each callstack and write out info in human readable form in CSV format
	for( int32 CallStackIndex=0; CallStackIndex<SortedCallStacks.Num(); CallStackIndex++ )
	{
		const FCallStack& CallStack = SortedCallStacks[CallStackIndex];

		// Avoid log spam by only logging above threshold.
		if( CallStack.StackCount > StackThreshold )
		{
			// First row is stack count.
			FString CallStackString = FString::FromInt((int32)CallStack.StackCount);
			CallStackString += FString::Printf( TEXT(",%5.2f"), static_cast<float>(CallStack.StackCount)/static_cast<float>(FramesCaptured) );
			

			// Iterate over all addresses in the callstack to look up symbol name.
			for( int32 AddressIndex=0; AddressIndex<ARRAY_COUNT(CallStack.Addresses) && CallStack.Addresses[AddressIndex]; AddressIndex++ )
			{
				ANSICHAR AddressInformation[512];
				AddressInformation[0] = 0;
				FPlatformStackWalk::ProgramCounterToHumanReadableString( AddressIndex, CallStack.Addresses[AddressIndex], AddressInformation, ARRAY_COUNT(AddressInformation)-1 );
				CallStackString = CallStackString + LINE_TERMINATOR TEXT(",,,") + FString(AddressInformation);
			}

			// Finally log with ',' prefix so "Log:" can easily be discarded as row in Excel.
			Ar.Logf(TEXT(",%s"),*CallStackString);
            
			//Append any user information before moving on to the next callstack
			if (ReportFn)
			{
				ReportFn(CallStack, CallStack.StackCount, Ar);
			}
		}
	}

	// Done logging.
	bAvoidCapturing = false;
}

/** Resets stack tracking. Deletes all user pointers passed in via CaptureStackTrace() */
void FStackTracker::ResetTracking()
{
	check(!bAvoidCapturing);
	CRCToCallStackIndexMap.Empty();

	//Clean up any user data 
	for(int32 i=0; i<CallStacks.Num(); i++)
	{
		if (CallStacks[i].UserData)
		{
			FMemory::Free(CallStacks[i].UserData);
		}
	}

	CallStacks.Empty();

	// Reset the markers
	StartFrameCounter = GFrameCounter;
	StopFrameCounter = GFrameCounter;
}

/** Toggles tracking. */
void FStackTracker::ToggleTracking()
{
	bIsEnabled = !bIsEnabled;
	// Enabled
	if( bIsEnabled )
	{
		UE_LOG(LogStackTracker, Log, TEXT("Stack tracking is now enabled."));
		StartFrameCounter = GFrameCounter;
	}
	// Disabled.
	else
	{
		StopFrameCounter = GFrameCounter;
		UE_LOG(LogStackTracker, Log, TEXT("Stack tracking is now disabled."));
	}
}
