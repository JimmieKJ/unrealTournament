// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if TRACK_ARRAY_SLACK 

#include "StackTracker.h"

FStackTracker* GSlackTracker = NULL;


/** Updates an existing call stack trace with new data for this particular call*/
static void SlackTrackerUpdateFn(const FStackTracker::FCallStack& CallStack, void* UserData)
{
	if( UserData != NULL )
	{
		//Callstack has been called more than once, aggregate the data
		FSlackTrackData* NewLCData = static_cast<FSlackTrackData*>(UserData);
		FSlackTrackData* CurrLCData = static_cast<FSlackTrackData*>(CallStack.UserData);

		CurrLCData->NumElements += NewLCData->NumElements;
		CurrLCData->NumSlackElements += NewLCData->NumSlackElements;
		//CurrLCData->Foo += 1;
	}
}


static void SlackTrackerReportFn(const FStackTracker::FCallStack& CallStack, uint64 TotalStackCount, FOutputDevice& Ar)
{
	//Output to a csv file any relevant data
	FSlackTrackData* const LCData = static_cast<FSlackTrackData*>(CallStack.UserData);
	if( LCData != NULL )
	{
		FString UserOutput = LINE_TERMINATOR TEXT(",,,");

		UserOutput += FString::Printf(TEXT("NumElements: %f, NumSlackElements: %f, Curr: %f TotalStackCount: %d "), (float)LCData->NumElements/TotalStackCount, (float)LCData->NumSlackElements/TotalStackCount, (float)LCData->CurrentSlackNum/TotalStackCount, TotalStackCount );

		Ar.Log(*UserOutput);
	}
}

static class FSlackTrackExec: private FSelfRegisteringExec
{
	virtual bool Exec( const TCHAR* Cmd, FOutputDevice& Ar )
	{
		if( FParse::Command( &Cmd, TEXT("DUMPSLACKTRACES") ) )
		{
			if( GSlackTracker )
			{
				GSlackTracker->DumpStackTraces( 100, Ar );
			}
			return true;
		}
		else if( FParse::Command( &Cmd, TEXT("RESETSLACKTRACKING") ) )
		{
			if( GSlackTracker )
			{
				GSlackTracker->ResetTracking();
			}
			return true;
		}
		else if( FParse::Command( &Cmd, TEXT("TOGGLESLACKTRACKING") ) )
		{
			if( GSlackTracker )
			{
				GSlackTracker->ToggleTracking();
			}
			return true;
		}
		return false;
	}
} SlackTrackExec;

void TrackSlack(int32 NumElements, int32 NumAllocatedElements, SIZE_T BytesPerElement, int32 Retval)
{
	if( !GSlackTracker )
	{
		GSlackTracker = new FStackTracker( SlackTrackerUpdateFn, SlackTrackerReportFn );
	}
	#define SLACK_TRACE_TO_SKIP 4
	FSlackTrackData* const LCData = static_cast<FSlackTrackData*>(FMemory::Malloc(sizeof(FSlackTrackData)));
	FMemory::Memset(LCData, 0, sizeof(FSlackTrackData));

	LCData->NumElements = NumElements;
	LCData->NumSlackElements = Retval;
	LCData->CurrentSlackNum = NumAllocatedElements-NumElements;

	GSlackTracker->CaptureStackTrace(SLACK_TRACE_TO_SKIP, static_cast<void*>(LCData));
}
#endif

