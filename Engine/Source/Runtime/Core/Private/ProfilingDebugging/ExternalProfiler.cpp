// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "ExternalProfiler.h"
#include "Runtime/Core/Public/Features/IModularFeature.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"


DEFINE_LOG_CATEGORY_STATIC( LogExternalProfiler, Log, All );



FExternalProfiler::FExternalProfiler()
	:	TimerCount( 0 ),
		// No way to tell whether we're paused or not so we assume paused as it makes the most sense
		bIsPaused( true )
{
}


void FExternalProfiler::PauseProfiler()
{
	ProfilerPauseFunction();
	bIsPaused = true;
}


void FExternalProfiler::ResumeProfiler()
{
	ProfilerResumeFunction();
	bIsPaused = false;
}


FName FExternalProfiler::GetFeatureName()
{
	static FName ProfilerFeatureName( "ExternalProfiler" );
	return ProfilerFeatureName;
}


bool FScopedExternalProfilerBase::bDidInitialize = false;

FExternalProfiler* FScopedExternalProfilerBase::ActiveProfiler = NULL;



void FScopedExternalProfilerBase::StartScopedTimer( const bool bWantPause )
{
	// Create profiler on demand.
	if( ActiveProfiler == NULL && !bDidInitialize )
	{
		const int32 AvailableProfilerCount = IModularFeatures::Get().GetModularFeatureImplementationCount( FExternalProfiler::GetFeatureName() );
		for( int32 CurProfilerIndex = 0; CurProfilerIndex < AvailableProfilerCount; ++CurProfilerIndex )
		{
			FExternalProfiler& CurProfiler = IModularFeatures::Get().GetModularFeature<FExternalProfiler&>( FExternalProfiler::GetFeatureName() );

			UE_LOG( LogExternalProfiler, Log, TEXT( "Found external profiler: %s" ), CurProfiler.GetProfilerName() );

			// Default to the first profiler we have if none were specified on the command-line
			if( ActiveProfiler == NULL )
			{
				ActiveProfiler = &CurProfiler;
			}

			// Check to see if the profiler was specified on the command-line (e.g., "-VTune")
			if( FParse::Param( FCommandLine::Get(), CurProfiler.GetProfilerName() ) )
			{
				ActiveProfiler = &CurProfiler;
			}
		}

		if( ActiveProfiler != NULL )
		{
			UE_LOG( LogExternalProfiler, Log, TEXT( "Using external profiler: %s" ), ActiveProfiler->GetProfilerName() );
		}
		else
		{
			UE_LOG( LogExternalProfiler, Warning, TEXT( "No registered external profiler was matched with a command-line switch (or the DLL could not be loaded).  External profiling features will not be available." ) );
		}

		// Don't try to initialize again this session
		bDidInitialize = true;
	}


	if( ActiveProfiler != NULL )
	{
		// Store the current state of profiler
		bWasPaused = ActiveProfiler->bIsPaused;

		// If the current profiler state isn't set to what we need, or if the global profiler sampler isn't currently
		// running, then start it now
		if( ActiveProfiler->TimerCount == 0 || bWantPause != ActiveProfiler->bIsPaused )
		{
			if( bWantPause )
			{
				ActiveProfiler->PauseProfiler();
			}
			else
			{
				ActiveProfiler->ResumeProfiler();
			}
		}

		// Increment number of overlapping timers
		ActiveProfiler->TimerCount++;
	}
}


void FScopedExternalProfilerBase::StopScopedTimer()
{
	if( ActiveProfiler != NULL )
	{
		// Make sure a timer was already started
		check( ActiveProfiler->TimerCount > 0 );

		// Decrement timer count
		ActiveProfiler->TimerCount--;

		// Restore the previous state of VTune
		if( bWasPaused != ActiveProfiler->bIsPaused )
		{
			if( bWasPaused )
			{
				ActiveProfiler->PauseProfiler();
			}
			else
			{
				ActiveProfiler->ResumeProfiler();
			}
		}
	}
}


