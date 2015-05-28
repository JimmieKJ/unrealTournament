// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/** 
 * ChartCreation
 *
 */
#include "EnginePrivate.h"
#include "ChartCreation.h"
#include "Database.h"
#include "RenderCore.h"
#include "Scalability.h"
#include "GameFramework/GameUserSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogChartCreation, Log, All);

/** The total GPU time taken to render all frames. In seconds. */
double			GTotalGPUTime = 0; 

#if DO_CHARTING

// NOTE:  if you add any new stats make certain to update the StartFPSChart()

/** Enables capturing the fps chart data */
bool GEnableDataCapture = false;

/** Start date/time of capture */
FString GCaptureStartTime;

/** Start time of current FPS chart.										*/
double			GFPSChartStartTime;

/** FPS chart information. Time spent for each bucket and count.			*/
FFPSChartEntry	GFPSChart[STAT_FPSChartLastBucketStat - STAT_FPSChartFirstStat];

/** Hitch histogram.  How many times the game experienced hitches of various lengths. */
FHitchChartEntry GHitchChart[ STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat ];

/** Time thresholds for the various hitch buckets in milliseconds */
const int32 GHitchThresholds[ STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat ] =
	{ 5000, 2500, 2000, 1500, 1000, 750, 500, 300, 200, 150, 100, 60 };

/** Number of frames for each time of <boundtype> **/
uint32 GNumFramesBound_GameThread = 0;
uint32 GNumFramesBound_RenderThread = 0;
uint32 GNumFramesBound_GPU = 0;

double GTotalFramesBoundTime_GameThread = 0;
double GTotalFramesBoundTime_RenderThread = 0;
double GTotalFramesBoundTime_GPU = 0;

/** Arrays of render/game/GPU and total frame times. Captured and written out if FPS charting is enabled */
TArray<float> GRenderThreadFrameTimes;
TArray<float> GGameThreadFrameTimes;
TArray<float> GGPUFrameTimes;
TArray<float> GFrameTimes;

/** 
 * This will look at where the map is doing a bot test or a flythrough test.
 *
 * @ return the name of the chart to use
 **/
static FString GetFPSChartType()
{
	FString Retval;

	// causeevent=FlyThrough is what the FlyThrough event is
	if( FString(FCommandLine::Get()).Contains( TEXT( "causeevent=FlyThrough" )) )
	{
		Retval = TEXT ( "FlyThrough" );
	}
	else
	{
		Retval = TEXT ( "FPS" );
	}

	return Retval;
}


/**
 * This will create the file name for the file we are saving out.
 *
 * @param ChartType	the type of chart we are making 
 * @param InMapName	the name of the map
 * @param FileExtension	the filename extension to append
 **/
static FString CreateFileNameForChart( const FString& ChartType, const FString& InMapName, const FString& FileExtension )
{
	FString Retval;
	// Create FPS chart filename.
	FString Platform;
	// determine which platform we are
	Platform = FPlatformProperties::PlatformName();

	Retval = InMapName + TEXT("-") + ChartType + TEXT("-") + Platform + FileExtension;

	return Retval;
}

/** This will create the folder name for the output directory for FPS charts. */
static FString CreateOutputDirectory()
{
	// Create folder for FPS chart data.
	const FString OutputDir = FPaths::ProfilingDir() / TEXT( "FPSChartStats" ) / GCaptureStartTime;
	IFileManager::Get().MakeDirectory( *OutputDir, true );
	return OutputDir;
}

/**
* Ticks the FPS chart.
*
* @param DeltaSeconds	Time in seconds passed since last tick.
*/
void UEngine::TickFPSChart( float DeltaSeconds )
{
	// We can't trust delta seconds if frame time clamping is enabled or if we're benchmarking so we simply
	// calculate it ourselves.
	static double LastTime = 0;

	// Keep track of our previous frame's statistics
	static float LastDeltaSeconds = 0.0f;

	static double LastHitchTime = 0;

	// early out if we aren't doing any of the captures
	if (!GEnableDataCapture)
	{
		return;
	}

	const double CurrentTime = FPlatformTime::Seconds();
	if( LastTime > 0 )
	{
		DeltaSeconds = CurrentTime - LastTime;
	}
	LastTime = CurrentTime;

	// now gather some stats on what this frame was bound by (game, render, gpu)

	// Copy these locally since the RT may update it between reads otherwise
	const uint32 LocalRenderThreadTime = GRenderThreadTime;
	const uint32 LocalGPUFrameTime = GGPUFrameTime;

	// determine which Time is the greatest and less than 33.33 (as if we are above 30 fps we are happy and thus not "bounded" )
	const float Epsilon = 0.250f;
	uint32 MaxThreadTimeValue = FMath::Max3<uint32>( LocalRenderThreadTime, GGameThreadTime, LocalGPUFrameTime );
	const float FrameTime = FPlatformTime::ToSeconds(MaxThreadTimeValue);

	// so we want to try and guess when we are bound by the GPU even when on the xenon we do not get that info
	// If the frametime is bigger than 35 ms we can take DeltaSeconds into account as we're not VSYNCing in that case.
	uint32 PossibleGPUTime = LocalGPUFrameTime;
	if( PossibleGPUTime == 0 )
	{
		// if we are over
		PossibleGPUTime = static_cast<uint32>(FMath::Max( FrameTime, DeltaSeconds ) / FPlatformTime::GetSecondsPerCycle() );
		MaxThreadTimeValue = FMath::Max3<uint32>( GGameThreadTime, LocalRenderThreadTime, PossibleGPUTime );
	}

	// Disregard frames that took longer than one second when accumulating data.
	if( DeltaSeconds < 1.f )
	{
		const float CurrentFPS = 1 / DeltaSeconds;

		if( CurrentFPS < 5 )
		{
			GFPSChart[ STAT_FPSChart_0_5 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_0_5 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 10 )
		{
			GFPSChart[ STAT_FPSChart_5_10 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_5_10 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 15 )
		{
			GFPSChart[ STAT_FPSChart_10_15 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_10_15 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 20 )
		{
			GFPSChart[ STAT_FPSChart_15_20 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_15_20 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 25 )
		{
			GFPSChart[ STAT_FPSChart_20_25 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_20_25 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 30 )
		{
			GFPSChart[ STAT_FPSChart_25_30 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_25_30 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 40 )
		{
			GFPSChart[ STAT_FPSChart_30_40 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_30_40 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 50 )
		{
			GFPSChart[ STAT_FPSChart_40_50 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_40_50 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if( CurrentFPS < 60 )
		{
			GFPSChart[ STAT_FPSChart_50_60 - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_50_60 - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 70)
		{
			GFPSChart[STAT_FPSChart_60_70 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_60_70 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 80)
		{
			GFPSChart[STAT_FPSChart_70_80 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_70_80 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 90)
		{
			GFPSChart[STAT_FPSChart_80_90 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_80_90 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 100)
		{
			GFPSChart[STAT_FPSChart_90_100 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_90_100 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 110)
		{
			GFPSChart[STAT_FPSChart_100_110 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_100_110 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 120)
		{
			GFPSChart[STAT_FPSChart_110_120 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_110_120 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else
		{
			GFPSChart[ STAT_FPSChart_120_INF - STAT_FPSChartFirstStat ].Count++;
			GFPSChart[ STAT_FPSChart_120_INF - STAT_FPSChartFirstStat ].CummulativeTime += DeltaSeconds;
		}

		GTotalGPUTime += FPlatformTime::ToSeconds(LocalGPUFrameTime);

		// if Frametime is > 33 ms then we are bounded by something
		if( CurrentFPS < 30 )
		{
			// If GPU time is inferred we can only determine GPU > 33 ms if we are GPU bound.
			bool bAreWeGPUBoundIfInferred = true;
			// 33.333ms
			const float TargetThreadTimeSeconds = .0333333f;

			if( FPlatformTime::ToSeconds(GGameThreadTime) >= TargetThreadTimeSeconds )
			{
				GNumFramesBound_GameThread++;
				GTotalFramesBoundTime_GameThread += DeltaSeconds;
				bAreWeGPUBoundIfInferred = false;
			}

			if( FPlatformTime::ToSeconds(LocalRenderThreadTime) >= TargetThreadTimeSeconds )
			{
				GNumFramesBound_RenderThread++;
				GTotalFramesBoundTime_RenderThread += DeltaSeconds;
				bAreWeGPUBoundIfInferred = false;
			}			

			// Consider this frame GPU bound if we have an actual measurement which is over the limit,
			if( (LocalGPUFrameTime != 0 && FPlatformTime::ToSeconds(LocalGPUFrameTime) >= TargetThreadTimeSeconds) ||
				// Or if we don't have a measurement but neither of the other threads were the slowest
				(LocalGPUFrameTime == 0 && bAreWeGPUBoundIfInferred && PossibleGPUTime == MaxThreadTimeValue) )
			{
				GTotalFramesBoundTime_GPU += DeltaSeconds;
				GNumFramesBound_GPU++;
			}
		}
	}

	// Track per frame stats.

	// Capturing FPS chart info.
	{		
		GGameThreadFrameTimes.Add( FPlatformTime::ToSeconds(GGameThreadTime) );
		GRenderThreadFrameTimes.Add( FPlatformTime::ToSeconds(LocalRenderThreadTime) );
		GGPUFrameTimes.Add( FPlatformTime::ToSeconds(LocalGPUFrameTime) );
		GFrameTimes.Add(DeltaSeconds);
	}

	// Check for hitches
	{
		// Minimum time quantum before we'll even consider this a hitch
		const float MinFrameTimeToConsiderAsHitch = 0.06f;

		// Minimum time passed before we'll record a new hitch
		const float MinTimeBetweenHitches = 0.2f;

		// For the current frame to be considered a hitch, it must have run at least this many times slower than
		// the previous frame
		const float HitchMultiplierAmount = 1.5f;

		// Ignore frames faster than our threshold
		if( DeltaSeconds >= MinFrameTimeToConsiderAsHitch )
		{
			// How long has it been since the last hitch we detected?
			const float TimeSinceLastHitch = ( float )( CurrentTime - LastHitchTime );

			// Make sure at least a little time has passed since the last hitch we reported
			if( TimeSinceLastHitch >= MinTimeBetweenHitches )
			{
				// If our frame time is much larger than our last frame time, we'll count this as a hitch!
				if( DeltaSeconds > LastDeltaSeconds * HitchMultiplierAmount )
				{
					// We have a hitch!
					
					// Track the hitch by bucketing it based on time severity
					const int32 HitchBucketCount = STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat;
					for( int32 CurBucketIndex = 0; CurBucketIndex < HitchBucketCount; ++CurBucketIndex )
					{
						float HitchThresholdInSeconds = ( float )GHitchThresholds[ CurBucketIndex ] * 0.001f;
						if( DeltaSeconds >= HitchThresholdInSeconds )
						{
							// Increment the hitch count for this bucket
							++GHitchChart[ CurBucketIndex ].HitchCount;

						
							// Check to see what we were limited by this frame
							if( GGameThreadTime >= (MaxThreadTimeValue - Epsilon) )
							{
								// Bound by game thread
								++GHitchChart[ CurBucketIndex ].GameThreadBoundHitchCount;
							}
							else if( LocalRenderThreadTime >= (MaxThreadTimeValue - Epsilon) )
							{
								// Bound by render thread
								++GHitchChart[ CurBucketIndex ].RenderThreadBoundHitchCount;
							}
							else if( PossibleGPUTime == MaxThreadTimeValue )
							{
								// Bound by GPU
								++GHitchChart[ CurBucketIndex ].GPUBoundHitchCount;
							}


							// Found the bucket for this hitch!  We're done now.
							break;
						}
					}

					LastHitchTime = CurrentTime;
				}
			}
		}

		// Store stats for the next frame to look at
		LastDeltaSeconds = DeltaSeconds;
	}

}

/**
 * Starts the FPS chart data capture
 */
void UEngine::StartFPSChart()
{
	for( int32 BucketIndex=0; BucketIndex<ARRAY_COUNT(GFPSChart); BucketIndex++ )
	{
		GFPSChart[BucketIndex].Count = 0;
		GFPSChart[BucketIndex].CummulativeTime = 0;
	}
	GFPSChartStartTime = FPlatformTime::Seconds();

	for( int32 BucketIndex = 0; BucketIndex < ARRAY_COUNT( GHitchChart ); ++BucketIndex )
	{
		GHitchChart[ BucketIndex ].HitchCount = 0;
		GHitchChart[ BucketIndex ].GameThreadBoundHitchCount = 0;
		GHitchChart[ BucketIndex ].RenderThreadBoundHitchCount = 0;
		GHitchChart[ BucketIndex ].GPUBoundHitchCount = 0;
	}

	// Pre-allocate 10 minutes worth of frames at 30 Hz.
	int32 NumFrames = 10 * 60 * 30;
	GRenderThreadFrameTimes.Reset(NumFrames);
	GGPUFrameTimes.Reset(NumFrames);
	GGameThreadFrameTimes.Reset(NumFrames);
	GFrameTimes.Reset(NumFrames);

	GTotalGPUTime = 0;
	GGPUFrameTime = 0;

	GNumFramesBound_GameThread = 0;
	GNumFramesBound_RenderThread = 0;
	GNumFramesBound_GPU = 0;

	GTotalFramesBoundTime_GameThread = 0;
	GTotalFramesBoundTime_RenderThread = 0;
	GTotalFramesBoundTime_GPU = 0;

	GEnableDataCapture = true;

	GCaptureStartTime = FDateTime::Now().ToString();
}


/**
 * Stops the FPS chart data capture
 */
void UEngine::StopFPSChart()
{
	GEnableDataCapture = false;
}

/**
* Calculates the range of FPS values for the given bucket index
*/

void UEngine::CalcQuantisedFPSRange(int32 BucketIndex, int32& StartFPS, int32& EndFPS )
{
	// Figure out bucket range. Buckets start at 5 frame intervals then change to 10.
	StartFPS = 0;
	EndFPS = 0;
	
	if (BucketIndex < STAT_FPSChart_30_40)
	{
		// Still incrementing by 5
		StartFPS = BucketIndex * 5;
		EndFPS = StartFPS + 5;
	}
	else
	{
		// Now incrementing by 10
		StartFPS = STAT_FPSChart_30_40 * 5;
		StartFPS += (BucketIndex - STAT_FPSChart_30_40) * 10;
		EndFPS = StartFPS + 10;
	}

	if (BucketIndex + STAT_FPSChartFirstStat == STAT_FPSChart_120_INF)
	{
		EndFPS = 999;
	}
}

/**
 * Dumps the FPS chart information to the log.
 */
void UEngine::DumpFPSChartToLog( float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName )
{
	int32 NumFramesBelow30 = 0; // keep track of the number of frames below 30 FPS
	float PctTimeAbove30 = 0;	// Keep track of percentage of time at 30+ FPS.
	int32 NumFramesBelow60 = 0; // keep track of the number of frames below 60 FPS
	float PctTimeAbove60 = 0;	// Keep track of percentage of time at 60+ FPS.

	UE_LOG(LogChartCreation, Log, TEXT("--- Begin : FPS chart dump for level '%s'"), *InMapName );

	// Iterate over all buckets, dumping percentages.
	for( int32 BucketIndex=0; BucketIndex<ARRAY_COUNT(GFPSChart); BucketIndex++ )
	{
		// Figure out bucket time and frame percentage.
		const float BucketTimePercentage  = 100.f * GFPSChart[BucketIndex].CummulativeTime / TotalTime;
		const float BucketFramePercentage = 100.f * GFPSChart[BucketIndex].Count / NumFrames;

		// Figure out bucket range. Buckets start at 5 frame intervals then change to 10.
		int32 StartFPS = 0;
		int32 EndFPS = 0;
		CalcQuantisedFPSRange(BucketIndex, StartFPS, EndFPS);

		// Keep track of time spent at 30+ FPS.
		if( StartFPS >= 30 )
		{
			PctTimeAbove30 += BucketTimePercentage;
		}
		else
		{
			NumFramesBelow30 += GFPSChart[BucketIndex].Count;
		}

		// Keep track of time spent at 60+ FPS.
		if (StartFPS >= 60)
		{
			PctTimeAbove60 += BucketTimePercentage;
		}
		else
		{
			NumFramesBelow60 += GFPSChart[BucketIndex].Count;
		}

		// Log bucket index, time and frame Percentage.
		UE_LOG(LogChartCreation, Log, TEXT("Bucket: %2i - %2i  Time: %5.2f  Frame: %5.2f"), StartFPS, EndFPS, BucketTimePercentage, BucketFramePercentage );
	}

	UE_LOG(LogChartCreation, Log, TEXT("%i frames collected over %4.2f seconds, disregarding %4.2f seconds for a %4.2f FPS average, %4.2f percent of time spent > 30 FPS, %4.2f percent of time spent > 60 FPS"),
		NumFrames, 
		DeltaTime, 
		FMath::Max<float>( 0, DeltaTime - TotalTime ), 
		NumFrames / TotalTime,
		PctTimeAbove30,
		PctTimeAbove60);
	UE_LOG(LogChartCreation, Log, TEXT("Average GPU frametime: %4.2f ms"), float((GTotalGPUTime / NumFrames)*1000.0));
	UE_LOG(LogChartCreation, Log, TEXT("BoundGameThreadPct: %4.2f  BoundRenderThreadPct: %4.2f  BoundGPUPct: %4.2f PercentFrames30+: %f   PercentFrames60+: %f   BoundGameTime: %f  BoundRenderTime: %f  BoundGPUTime: %f  PctTimeAbove30: %f  PctTimeAbove60: %f ")
		, (float(GNumFramesBound_GameThread)/float(NumFrames))*100.0f
		, (float(GNumFramesBound_RenderThread)/float(NumFrames))*100.0f
		, (float(GNumFramesBound_GPU)/float(NumFrames))*100.0f
		, float(NumFrames - NumFramesBelow30) / float(NumFrames)*100.0f
		, float(NumFrames - NumFramesBelow60) / float(NumFrames)*100.0f
		, (GTotalFramesBoundTime_GameThread / DeltaTime)*100.0f
		, ((GTotalFramesBoundTime_RenderThread)/DeltaTime)*100.0f
		, ((GTotalFramesBoundTime_GPU)/DeltaTime)*100.0f
		, PctTimeAbove30
		, PctTimeAbove60
		);

	UE_LOG(LogChartCreation, Log, TEXT("--- End"));

	// Dump hitch data
	{
		UE_LOG(LogChartCreation, Log,  TEXT( "--- Begin : Hitch chart dump for level '%s'" ), *InMapName );

		int32 TotalHitchCount = 0;
		int32 TotalGameThreadBoundHitches = 0;
		int32 TotalRenderThreadBoundHitches = 0;
		int32 TotalGPUBoundHitches = 0;
		for( int32 BucketIndex = 0; BucketIndex < ARRAY_COUNT( GHitchChart ); ++BucketIndex )
		{
			const float HitchThresholdInSeconds = ( float )GHitchThresholds[ BucketIndex ] * 0.001f;

			FString RangeName;
			if( BucketIndex == 0 )
			{
				// First bucket's end threshold is infinitely large
				RangeName = FString::Printf( TEXT( "%0.2fs - inf" ), HitchThresholdInSeconds );
			}
			else
			{
				const float PrevHitchThresholdInSeconds = ( float )GHitchThresholds[ BucketIndex - 1 ] * 0.001f;

				// Set range from current bucket threshold to the last bucket's threshold
				RangeName = FString::Printf( TEXT( "%0.2fs - %0.2fs" ), HitchThresholdInSeconds, PrevHitchThresholdInSeconds );
			}

			UE_LOG(LogChartCreation, Log,  TEXT( "Bucket: %s  Count: %i " ), *RangeName, GHitchChart[ BucketIndex ].HitchCount );


			// Count up the total number of hitches
			TotalHitchCount += GHitchChart[ BucketIndex ].HitchCount;
			TotalGameThreadBoundHitches += GHitchChart[ BucketIndex ].GameThreadBoundHitchCount;
			TotalRenderThreadBoundHitches += GHitchChart[ BucketIndex ].RenderThreadBoundHitchCount;
			TotalGPUBoundHitches += GHitchChart[ BucketIndex ].GPUBoundHitchCount;
		}

		const int32 HitchBucketCount = STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat;
		UE_LOG(LogChartCreation, Log,  TEXT( "Total hitch count (at least %ims):  %i" ), GHitchThresholds[ HitchBucketCount - 1 ], TotalHitchCount );
		UE_LOG(LogChartCreation, Log, TEXT( "Hitch frames bound by game thread:  %i  (%0.1f percent)" ), TotalGameThreadBoundHitches, 
			(TotalHitchCount > 0) ? ( ( float )TotalGameThreadBoundHitches / ( float )TotalHitchCount * 100.0f ) : 0.0f );
		UE_LOG(LogChartCreation, Log,  TEXT( "Hitch frames bound by render thread:  %i  (%0.1f percent)" ), TotalRenderThreadBoundHitches, TotalHitchCount > 0 ? ( ( float )TotalRenderThreadBoundHitches / ( float )TotalHitchCount * 0.0f ) : 0.0f  );
		UE_LOG(LogChartCreation, Log,  TEXT( "Hitch frames bound by GPU:  %i  (%0.1f  percent)" ), TotalGPUBoundHitches, TotalHitchCount > 0 ? ( ( float )TotalGPUBoundHitches / ( float )TotalHitchCount * 100.0f ) : 0.0f );

		UE_LOG(LogChartCreation, Log,  TEXT( "--- End" ) );
	}
}

#if ALLOW_DEBUG_FILES
namespace
{
	/**
	 * Finds a percentile value in an array.
	 *
	 * @param Array array of values to look for (needs to be writable, will be modified)
	 * @param Percentile number between 0-99
	 * @return percentile value or -1 if no samples
	 */
	float GetPercentileValue(TArray<float> & Samples, int32 Percentile)
	{
		int32 Left = 0, Right = Samples.Num() - 1;

		if (Right < 0)
		{
			return -1.0f;
		}

		const int32 PercentileOrdinal = ( Percentile * Right ) / 100;

		// this is quickselect (see http://en.wikipedia.org/wiki/Quickselect for details).
		while( Right != Left )
		{
			// partition
			int32 MovingLeft = Left - 1;
			int32 MovingRight = Right;
			float Pivot = Samples[ MovingRight ];
			for ( ; ; )
			{
				while( Samples[ ++MovingLeft ] < Pivot );
				while( Samples[ --MovingRight ] > Pivot )
				{
					if ( MovingRight == Left )
						break;
				}

				if ( MovingLeft >= MovingRight )
					break;

				const float Temp = Samples[ MovingLeft ];
				Samples[ MovingLeft ] = Samples[ MovingRight ];
				Samples[ MovingRight ] = Temp;
			}

			const float Temp = Samples[ MovingLeft ];
			Samples[ MovingLeft ] = Samples[ Right ];
			Samples[ Right ] = Temp;

			// now we're pivoted around MovingLeft
			// decide what part K-th largest belong to
			if ( MovingLeft > PercentileOrdinal )
			{
				Right = MovingLeft - 1;
			}
			else if ( MovingLeft < PercentileOrdinal )
			{
				Left = MovingLeft + 1;
			}
			else
			{
				// we hit exactly the value we need, no need to sort further
				break;
			}
		}

		return Samples[ PercentileOrdinal ];
	}
}
#endif

/**
 * Dumps the frame times information to the special stats log file.
 */
void UEngine::DumpFrameTimesToStatsLog( float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName )
{
#if ALLOW_DEBUG_FILES
	// Create folder for FPS chart data.
	const FString OutputDir = CreateOutputDirectory();

	// Create archive for log data.
	const FString ChartType = GetFPSChartType();
	const FString ChartName = OutputDir / CreateFileNameForChart( ChartType, InMapName, TEXT( ".csv" ) );
	FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter( *ChartName );

	if( OutputFile )
	{
		OutputFile->Logf(TEXT("Percentile,Frame (ms), GT (ms), RT (ms), GPU (ms)"));
		TArray<float> FrameTimesCopy = GFrameTimes;
		TArray<float> GameThreadFrameTimesCopy = GGameThreadFrameTimes;
		TArray<float> RenderThreadFrameTimesCopy = GRenderThreadFrameTimes;
		TArray<float> GPUFrameTimesCopy = GGPUFrameTimes;
		// using selection a few times should still be faster than full sort once,
		// since it's linear vs non-linear (O(n) vs O(n log n) for quickselect vs quicksort)
		for (int32 Percentile = 25; Percentile <= 75; Percentile += 25)
		{
			OutputFile->Logf(TEXT("%d,%.2f,%.2f,%.2f,%.2f"), Percentile,
				GetPercentileValue(FrameTimesCopy, Percentile) * 1000,
				GetPercentileValue(GameThreadFrameTimesCopy, Percentile) * 1000,
				GetPercentileValue(RenderThreadFrameTimesCopy, Percentile) * 1000,
				GetPercentileValue(GPUFrameTimesCopy, Percentile) * 1000
				);
		}

		OutputFile->Logf(TEXT("Time (sec),Frame (ms), GT (ms), RT (ms), GPU (ms)"));
		float ElapsedTime = 0;
		for( int32 i=0; i<GFrameTimes.Num(); i++ )
		{
			OutputFile->Logf(TEXT("%.2f,%.2f,%.2f,%.2f,%.2f"),ElapsedTime,GFrameTimes[i]*1000,GGameThreadFrameTimes[i]*1000,GRenderThreadFrameTimes[i]*1000,GGPUFrameTimes[i]*1000);
			ElapsedTime += GFrameTimes[i];
		}
		delete OutputFile;
	}
#endif
}


/**
 * Dumps the FPS chart information to the special stats log file.
 */
void UEngine::DumpFPSChartToStatsLog( float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName )
{
#if ALLOW_DEBUG_FILES
	const FString OutputDir = CreateOutputDirectory();
	
	// Create archive for log data.
	const FString ChartType = GetFPSChartType();
	const FString ChartName = OutputDir / CreateFileNameForChart( ChartType, InMapName, TEXT( ".log" ) );
	FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter( *ChartName, FILEWRITE_Append );

	if( OutputFile )
	{
		OutputFile->Logf(TEXT("Dumping FPS chart at %s using build %s built from changelist %i"), *FDateTime::Now().ToString(), *GEngineVersion.ToString(), GetChangeListNumberForPerfTesting() );

		// Get OS info
		FString OSMajor;
		FString OSMinor;
		FPlatformMisc::GetOSVersions(OSMajor, OSMinor);

		// Get settings info
		const Scalability::FQualityLevels& Quality = GEngine->GetGameUserSettings()->ScalabilityQuality;

		OutputFile->Logf(TEXT("Machine info:"));
		OutputFile->Logf(TEXT("\tOS: %s %s"), *OSMajor, *OSMinor);
		OutputFile->Logf(TEXT("\tCPU: %s %s"), *FPlatformMisc::GetCPUVendor(), *FPlatformMisc::GetCPUBrand());
		OutputFile->Logf(TEXT("\tGPU: %s"), *FPlatformMisc::GetPrimaryGPUBrand());
		OutputFile->Logf(TEXT("\tResolution Quality: %d"), Quality.ResolutionQuality);
		OutputFile->Logf(TEXT("\tView Distance Quality: %d"), Quality.ViewDistanceQuality);
		OutputFile->Logf(TEXT("\tAnti-Aliasing Quality: %d"), Quality.AntiAliasingQuality);
		OutputFile->Logf(TEXT("\tShadow Quality: %d"), Quality.ShadowQuality);
		OutputFile->Logf(TEXT("\tPost-Process Quality: %d"), Quality.PostProcessQuality);
		OutputFile->Logf(TEXT("\tTexture Quality: %d"), Quality.TextureQuality);
		OutputFile->Logf(TEXT("\tEffects Quality: %d"), Quality.EffectsQuality);

		int32 NumFramesBelow30 = 0; // keep track of the number of frames below 30 FPS
		float PctTimeAbove30 = 0;	// Keep track of percentage of time at 30+ FPS.
		int32 NumFramesBelow60 = 0; // keep track of the number of frames below 60 FPS
		float PctTimeAbove60 = 0;	// Keep track of percentage of time at 60+ FPS.

		// Iterate over all buckets, dumping percentages.
		for( int32 BucketIndex=0; BucketIndex<ARRAY_COUNT(GFPSChart); BucketIndex++ )
		{
			// Figure out bucket time and frame percentage.
			const float BucketTimePercentage  = 100.f * GFPSChart[BucketIndex].CummulativeTime / TotalTime;
			const float BucketFramePercentage = 100.f * GFPSChart[BucketIndex].Count / NumFrames;

			int32 StartFPS = 0;
			int32 EndFPS = 0;
			CalcQuantisedFPSRange(BucketIndex, StartFPS, EndFPS);

			// Keep track of time spent at 30+ FPS.
			if (StartFPS >= 30)
			{
				PctTimeAbove30 += BucketTimePercentage;
			}
			else
			{
				NumFramesBelow30 += GFPSChart[BucketIndex].Count;
			}
			
			// Keep track of time spent at 60+ FPS.
			if (StartFPS >= 60)
			{
				PctTimeAbove60 += BucketTimePercentage;
			}
			else
			{
				NumFramesBelow60 += GFPSChart[BucketIndex].Count;
			}

			// Log bucket index, time and frame Percentage.
			OutputFile->Logf(TEXT("Bucket: %2i - %2i  Time: %5.2f  Frame: %5.2f"), StartFPS, EndFPS, BucketTimePercentage, BucketFramePercentage);
		}

		OutputFile->Logf(TEXT("%i frames collected over %4.2f seconds, disregarding %4.2f seconds for a %4.2f FPS average, %4.2f percent of time spent > 30 FPS, %4.2f percent of time spent > 60 FPS"), 
			NumFrames, 
			DeltaTime, 
			FMath::Max<float>( 0, DeltaTime - TotalTime ), 
			NumFrames / TotalTime,
			PctTimeAbove30, PctTimeAbove60 );
		OutputFile->Logf(TEXT("Average GPU frame time: %4.2f ms"), float((GTotalGPUTime / NumFrames)*1000.0));
		OutputFile->Logf(TEXT("BoundGameThreadPct: %4.2f  BoundRenderThreadPct: %4.2f  BoundGPUPct: %4.2f PercentFrames30+: %f   PercentFrames60+: %f   BoundGameTime: %f  BoundRenderTime: %f  BoundGPUTime: %f  PctTimeAbove30: %f  PctTimeAbove60: %f ")
			, (float(GNumFramesBound_GameThread)/float(NumFrames))*100.0f
			, (float(GNumFramesBound_RenderThread)/float(NumFrames))*100.0f
			, (float(GNumFramesBound_GPU)/float(NumFrames))*100.0f
			, float(NumFrames - NumFramesBelow30) / float(NumFrames)*100.0f
			, float(NumFrames - NumFramesBelow60) / float(NumFrames)*100.0f
			, (GTotalFramesBoundTime_GameThread / DeltaTime)*100.0f
			, ((GTotalFramesBoundTime_RenderThread)/DeltaTime)*100.0f
			, ((GTotalFramesBoundTime_GPU)/DeltaTime)*100.0f
			, PctTimeAbove30
			, PctTimeAbove60
			);

		// Dump hitch data
		{
			OutputFile->Logf( TEXT( "Hitch chart:" ) );

			int32 TotalHitchCount = 0;
			int32 TotalGameThreadBoundHitches = 0;
			int32 TotalRenderThreadBoundHitches = 0;
			int32 TotalGPUBoundHitches = 0;
			for( int32 BucketIndex = 0; BucketIndex < ARRAY_COUNT( GHitchChart ); ++BucketIndex )
			{
				const float HitchThresholdInSeconds = ( float )GHitchThresholds[ BucketIndex ] * 0.001f;

				FString RangeName;
				if( BucketIndex == 0 )
				{
					// First bucket's end threshold is infinitely large
					RangeName = FString::Printf( TEXT( "%0.2fs - inf" ), HitchThresholdInSeconds );
				}
				else
				{
					const float PrevHitchThresholdInSeconds = ( float )GHitchThresholds[ BucketIndex - 1 ] * 0.001f;

					// Set range from current bucket threshold to the last bucket's threshold
					RangeName = FString::Printf( TEXT( "%0.2fs - %0.2fs" ), HitchThresholdInSeconds, PrevHitchThresholdInSeconds );
				}

				OutputFile->Logf( TEXT( "Bucket: %s  Count: %i " ), *RangeName, GHitchChart[ BucketIndex ].HitchCount );


				// Count up the total number of hitches
				TotalHitchCount += GHitchChart[ BucketIndex ].HitchCount;
				TotalGameThreadBoundHitches += GHitchChart[ BucketIndex ].GameThreadBoundHitchCount;
				TotalRenderThreadBoundHitches += GHitchChart[ BucketIndex ].RenderThreadBoundHitchCount;
				TotalGPUBoundHitches += GHitchChart[ BucketIndex ].GPUBoundHitchCount;
			}

			const int32 HitchBucketCount = STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat;
			OutputFile->Logf( TEXT( "Total hitch count (at least %ims):  %i" ), GHitchThresholds[ HitchBucketCount - 1 ], TotalHitchCount );
			OutputFile->Logf( TEXT( "Hitch frames bound by game thread:  %i  (%0.1f%%)" ), TotalGameThreadBoundHitches, TotalHitchCount > 0 ? ( ( float )TotalGameThreadBoundHitches / ( float )TotalHitchCount * 100.0f ) : 0.0f );
			OutputFile->Logf( TEXT( "Hitch frames bound by render thread:  %i  (%0.1f%%)" ), TotalRenderThreadBoundHitches, TotalHitchCount > 0 ? ( ( float )TotalRenderThreadBoundHitches / ( float )TotalHitchCount * 0.0f ) : 0.0f  );
			OutputFile->Logf( TEXT( "Hitch frames bound by GPU:  %i  (%0.1f%%)" ), TotalGPUBoundHitches, TotalHitchCount > 0 ? ( ( float )TotalGPUBoundHitches / ( float )TotalHitchCount * 100.0f ) : 0.0f );
		}

		OutputFile->Logf( LINE_TERMINATOR LINE_TERMINATOR LINE_TERMINATOR );

		// Flush, close and delete.
		delete OutputFile;

		const FString AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead( *ChartName );

		UE_LOG( LogProfilingDebugging, Warning, TEXT( "FPS Chart (logfile) saved to %s" ), *AbsolutePath );

#if	PLATFORM_DESKTOP
		FPlatformProcess::ExploreFolder( *AbsolutePath );
#endif // PLATFORM_DESKTOP
	}
#endif // ALLOW_DEBUG_FILES
}

/**
* Dumps the FPS chart information to HTML.
*/
void UEngine::DumpFPSChartToHTML( float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName )
{
#if ALLOW_DEBUG_FILES
	// Load the HTML building blocks from the Engine\Stats folder.
	FString FPSChartPreamble;
	FString FPSChartPostamble;
	FString FPSChartRow;
	bool	bAreAllHTMLPartsLoaded = true;

	bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && FFileHelper::LoadFileToString( FPSChartPreamble,	*(FPaths::EngineContentDir() + TEXT("Stats/FPSChart_Preamble.html")	) );
	bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && FFileHelper::LoadFileToString( FPSChartPostamble,	*(FPaths::EngineContentDir() + TEXT("Stats/FPSChart_Postamble.html")	) );
	bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && FFileHelper::LoadFileToString( FPSChartRow,		*(FPaths::EngineContentDir() + TEXT("Stats/FPSChart_Row.html")			) );

	// Successfully loaded all HTML templates.
	if( bAreAllHTMLPartsLoaded )
	{
		// Keep track of percentage of time at 30+ FPS.
		float PctTimeAbove30 = 0;
		// Keep track of percentage of time at 60+ FPS.
		float PctTimeAbove60 = 0;

		// Iterate over all buckets, updating row 
		for( int32 BucketIndex=0; BucketIndex<ARRAY_COUNT(GFPSChart); BucketIndex++ )
		{
			// Figure out bucket time and frame percentage.
			const float BucketTimePercentage  = 100.f * GFPSChart[BucketIndex].CummulativeTime / TotalTime;
			const float BucketFramePercentage = 100.f * GFPSChart[BucketIndex].Count / NumFrames;

			// Figure out bucket range. Buckets start at 5 frame intervals then change to 10.
			int32 StartFPS = 0;
			int32 EndFPS = 0;
			CalcQuantisedFPSRange(BucketIndex, StartFPS, EndFPS);

			// Keep track of time spent at 30+ FPS.
			if( StartFPS >= 30 )
			{
				PctTimeAbove30 += BucketTimePercentage;
			}

			// Keep track of time spent at 60+ FPS.
			if (StartFPS >= 60)
			{
				PctTimeAbove60 += BucketTimePercentage;
			}

			const FString SrcToken = FString::Printf(TEXT("TOKEN_%i_%i"), StartFPS, EndFPS);
			const FString DstToken = FString::Printf( TEXT("%5.2f"), BucketTimePercentage );

			// Replace token with actual values.
			FPSChartRow	= FPSChartRow.Replace( *SrcToken, *DstToken );
		}


		// Update hitch data
		{
			int32 TotalHitchCount = 0;
			int32 TotalGameThreadBoundHitches = 0;
			int32 TotalRenderThreadBoundHitches = 0;
			int32 TotalGPUBoundHitches = 0;
			for( int32 BucketIndex = 0; BucketIndex < ARRAY_COUNT( GHitchChart ); ++BucketIndex )
			{
				FString SrcToken;
				if( BucketIndex == 0 )
				{
					SrcToken = FString::Printf( TEXT("TOKEN_HITCH_%i_PLUS"), GHitchThresholds[ BucketIndex ] );
				}
				else
				{
					SrcToken = FString::Printf( TEXT("TOKEN_HITCH_%i_%i"), GHitchThresholds[ BucketIndex ], GHitchThresholds[ BucketIndex - 1 ] );
				}

				const FString DstToken = FString::Printf( TEXT("%i"), GHitchChart[ BucketIndex ].HitchCount );

				// Replace token with actual values.
				FPSChartRow	= FPSChartRow.Replace( *SrcToken, *DstToken );

				// Count up the total number of hitches
				TotalHitchCount += GHitchChart[ BucketIndex ].HitchCount;
				TotalGameThreadBoundHitches += GHitchChart[ BucketIndex ].GameThreadBoundHitchCount;
				TotalRenderThreadBoundHitches += GHitchChart[ BucketIndex ].RenderThreadBoundHitchCount;
				TotalGPUBoundHitches += GHitchChart[ BucketIndex ].GPUBoundHitchCount;
			}

			// Total hitch count
			FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_HITCH_TOTAL"), *FString::Printf(TEXT("%i"), TotalHitchCount), ESearchCase::CaseSensitive );
			FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_HITCH_GAME_BOUND_COUNT"), *FString::Printf(TEXT("%i"), TotalGameThreadBoundHitches), ESearchCase::CaseSensitive );
			FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_HITCH_RENDER_BOUND_COUNT"), *FString::Printf(TEXT("%i"), TotalRenderThreadBoundHitches), ESearchCase::CaseSensitive );
			FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_HITCH_GPU_BOUND_COUNT"), *FString::Printf(TEXT("%i"), TotalGPUBoundHitches), ESearchCase::CaseSensitive );
		}

		// Get OS info
		FString OSMajor;
		FString OSMinor;
		FPlatformMisc::GetOSVersions(OSMajor, OSMinor);

		// Get settings info
		const Scalability::FQualityLevels& Quality = GEngine->GetGameUserSettings()->ScalabilityQuality;

		// Update non- bucket stats.
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_MAPNAME"),		    *FString::Printf(TEXT("%s"), *InMapName ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_CHANGELIST"),		*FString::Printf(TEXT("%i"), GetChangeListNumberForPerfTesting() ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_DATESTAMP"),         *FString::Printf(TEXT("%s"), *FDateTime::Now().ToString() ), ESearchCase::CaseSensitive );

		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_OS"),			    *FString::Printf(TEXT("%s %s"), *OSMajor, *OSMinor ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_CPU"),			    *FString::Printf(TEXT("%s %s"), *FPlatformMisc::GetCPUVendor(), *FPlatformMisc::GetCPUBrand() ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_GPU"),			    *FString::Printf(TEXT("%s"), *FPlatformMisc::GetPrimaryGPUBrand() ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_SETTINGS_RES"),	    *FString::Printf(TEXT("%d"), Quality.ResolutionQuality ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_SETTINGS_VD"),	    *FString::Printf(TEXT("%d"), Quality.ViewDistanceQuality ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_SETTINGS_AA"),	    *FString::Printf(TEXT("%d"), Quality.AntiAliasingQuality ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_SETTINGS_SHADOW"),   *FString::Printf(TEXT("%d"), Quality.ShadowQuality ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_SETTINGS_PP"),	    *FString::Printf(TEXT("%d"), Quality.PostProcessQuality ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_SETTINGS_TEX"),	    *FString::Printf(TEXT("%d"), Quality.TextureQuality ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_SETTINGS_FX"),	    *FString::Printf(TEXT("%d"), Quality.EffectsQuality ), ESearchCase::CaseSensitive );

		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_AVG_FPS"),			*FString::Printf(TEXT("%4.2f"), NumFrames / TotalTime), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_PCT_ABOVE_30"), *FString::Printf(TEXT("%4.2f"), PctTimeAbove30), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_PCT_ABOVE_60"), *FString::Printf(TEXT("%4.2f"), PctTimeAbove60), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_TIME_DISREGARDED"), *FString::Printf(TEXT("%4.2f"), FMath::Max<float>(0, DeltaTime - TotalTime)), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_TIME"),				*FString::Printf(TEXT("%4.2f"), DeltaTime), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_FRAMECOUNT"),		*FString::Printf(TEXT("%i"), NumFrames), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_AVG_GPUTIME"),		*FString::Printf(TEXT("%4.2f ms"), float((GTotalGPUTime / NumFrames)*1000.0) ), ESearchCase::CaseSensitive );

		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_BOUND_GAME_THREAD_PERCENT"),		*FString::Printf(TEXT("%4.2f"), (float(GNumFramesBound_GameThread)/float(NumFrames))*100.0f ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_BOUND_RENDER_THREAD_PERCENT"),		*FString::Printf(TEXT("%4.2f"), (float(GNumFramesBound_RenderThread)/float(NumFrames))*100.0f ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_BOUND_GPU_PERCENT"),		*FString::Printf(TEXT("%4.2f"), (float(GNumFramesBound_GPU)/float(NumFrames))*100.0f ), ESearchCase::CaseSensitive );

		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_BOUND_GAME_THREAD_TIME"),		*FString::Printf(TEXT("%4.2f"), (GTotalFramesBoundTime_GameThread/DeltaTime)*100.0f ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_BOUND_RENDER_THREAD_TIME"),		*FString::Printf(TEXT("%4.2f"), ((GTotalFramesBoundTime_RenderThread)/DeltaTime)*100.0f ), ESearchCase::CaseSensitive );
		FPSChartRow = FPSChartRow.Replace( TEXT("TOKEN_BOUND_GPU_TIME"),		*FString::Printf(TEXT("%4.2f"), ((GTotalFramesBoundTime_GPU)/DeltaTime)*100.0f ), ESearchCase::CaseSensitive );


		const FString OutputDir = CreateOutputDirectory();

		// Create FPS chart filename.
		const FString ChartType = GetFPSChartType();

		const FString& FPSChartFilename = OutputDir / CreateFileNameForChart( ChartType, *InMapName, TEXT( ".html" ) );
		FString FPSChart;

		// See whether file already exists and load it into string if it does.
		if( FFileHelper::LoadFileToString( FPSChart, *FPSChartFilename ) )
		{
			// Split string where we want to insert current row.
			const FString HeaderSeparator = TEXT("<UE4></UE4>");
			FString FPSChartBeforeCurrentRow, FPSChartAfterCurrentRow;
			FPSChart.Split( *HeaderSeparator, &FPSChartBeforeCurrentRow, &FPSChartAfterCurrentRow );

			// Assemble FPS chart by inserting current row at the top.
			FPSChart = FPSChartPreamble + FPSChartRow + FPSChartAfterCurrentRow;
		}
		// Assemble from scratch.
		else
		{
			FPSChart = FPSChartPreamble + FPSChartRow + FPSChartPostamble;
		}

		// Save the resulting file back to disk.
		FFileHelper::SaveStringToFile( FPSChart, *FPSChartFilename );

		UE_LOG( LogProfilingDebugging, Warning, TEXT( "FPS Chart (HTML) saved to %s" ), *IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead( *FPSChartFilename ) );
	}
	else
	{
		UE_LOG(LogChartCreation, Log, TEXT("Missing FPS chart template files."));
	}

#endif // ALLOW_DEBUG_FILES
}

/**
 * Dumps the FPS chart information to the passed in archive.
 *
 * @param	InMapName	Name of the map
 * @param	bForceDump	Whether to dump even if FPS chart info is not enabled.
 */
void UEngine::DumpFPSChart( const FString& InMapName, bool bForceDump )
{
	// Iterate over all buckets, gathering total frame count and cumulative time.
	float TotalTime = 0;
	const float DeltaTime = FPlatformTime::Seconds() - GFPSChartStartTime;
	int32	  NumFrames = 0;
	for( int32 BucketIndex=0; BucketIndex<ARRAY_COUNT(GFPSChart); BucketIndex++ )
	{
		NumFrames += GFPSChart[BucketIndex].Count;
		TotalTime += GFPSChart[BucketIndex].CummulativeTime;
	}

	bool bFPSChartIsActive = bForceDump;

	bool bMemoryChartIsActive = false;
	if( FParse::Param( FCommandLine::Get(), TEXT("CaptureMemoryChartInfo") ) || FParse::Param( FCommandLine::Get(), TEXT("gCMCI") ) )
	{
		bMemoryChartIsActive = true;
	}

	if( ( bFPSChartIsActive == true )
		&& ( TotalTime > 0.f ) 
		&& ( NumFrames > 0 )
		&& ( bMemoryChartIsActive == false ) // we do not want to dump out FPS stats if we have been gathering mem stats as that will probably throw off the stats
		)
	{
		// Log chart info to the log.
		DumpFPSChartToLog( TotalTime, DeltaTime, NumFrames, InMapName );

		// Only log FPS chart data to file in the game and not PIE.
		if( FApp::IsGame() )
		{
			DumpFPSChartToStatsLog( TotalTime, DeltaTime, NumFrames, InMapName );
			DumpFrameTimesToStatsLog( TotalTime, DeltaTime, NumFrames, InMapName );

			DumpFPSChartToHTML( TotalTime, DeltaTime, NumFrames, InMapName  );
			DumpFPSChartToHTML( TotalTime, DeltaTime, NumFrames, "Global" );
		}


	}
}

#endif // DO_CHARTING




