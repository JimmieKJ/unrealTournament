// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/** 
 * ChartCreation
 *
 */
#include "EnginePrivate.h"
#include "ChartCreation.h"
#include "Database.h"
#include "RenderCore.h"
#include "Scalability.h"
#include "AnalyticsEventAttribute.h"
#include "GameFramework/GameUserSettings.h"
#include "Performance/EnginePerformanceTargets.h"

DEFINE_LOG_CATEGORY_STATIC(LogChartCreation, Log, All);

/** The total GPU time taken to render all frames. In seconds. */
double GTotalGPUTime = 0; 

#if DO_CHARTING

// Should we round raw FPS values before thresholding them into bins?
static TAutoConsoleVariable<int32> GRoundChartingFPSBeforeBinning(
	TEXT("t.FPSChart.RoundFPSBeforeBinning"),
	0,
	TEXT("Should we round raw FPS values before thresholding them into bins when doing a FPS chart?\n")
	TEXT(" default: 0"));

float GMaximumFrameTimeToConsiderForHitchesAndBinning = 1.0f;

static FAutoConsoleVariableRef GMaximumFrameTimeToConsiderForHitchesAndBinningCVar(
	TEXT("t.FPSChart.MaxFrameDeltaSecsBeforeDiscarding"),
	GMaximumFrameTimeToConsiderForHitchesAndBinning,
	TEXT("The maximum length a frame can be (in seconds) to be considered for FPS chart binning (default 1.0s; no maximum length if <= 0.0)")
	);

// NOTE:  if you add any new stats make certain to update the StartFPSChart()

/** Enables capturing the fps chart data */
bool GEnableDataCapture = false;

/** Start date/time of capture */
FString GCaptureStartTime;

/** User label set when starting the chart. */
FString GFPSChartLabel;

/** Start time of current FPS chart. */
double GFPSChartStartTime = 0;

/** Stop time of current FPS chart (so we don't depend on time when Dump.. is called) */
double GFPSChartStopTime = 0;

/** FPS chart information. Time spent for each bucket and count. */
FFPSChartEntry	GFPSChart[STAT_FPSChartLastBucketStat - STAT_FPSChartFirstStat];

/** Hitch histogram.  How many times the game experienced hitches of various lengths. */
FHitchChartEntry GHitchChart[ STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat ];

/** Time thresholds for the various hitch buckets in milliseconds */
const int32 GHitchThresholds[ STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat ] =
	{ 5000, 2500, 2000, 1500, 1000, 750, 500, 300, 200, 150, 100, 60, 30 };

/** Number of frames for each time of <boundtype> **/
uint32 GNumFramesBound_GameThread = 0;
uint32 GNumFramesBound_RenderThread = 0;
uint32 GNumFramesBound_GPU = 0;

double GTotalFramesBoundTime_GameThread = 0.0;
double GTotalFramesBoundTime_RenderThread = 0.0;
double GTotalFramesBoundTime_GPU = 0.0;

double GTotalFrameTime_GameThread = 0.0;
double GTotalFrameTime_RenderThread = 0.0;

/** Should we record per-frame times */
bool GFPSChartRecordPerFrameTimes = false;

/** Arrays of render/game/GPU and total frame times. Captured and written out if FPS charting is enabled and GFPSChartRecordPerFrameTimes is true */
TArray<float> GRenderThreadFrameTimes;
TArray<float> GGameThreadFrameTimes;
TArray<float> GGPUFrameTimes;
TArray<float> GFrameTimes;

/** Array of interesting summary thresholds (e.g., 30 Hz, 60 Hz, 120 Hz) */
TArray<int32> GTargetFrameRatesForSummary;


/** We can't trust delta seconds if frame time clamping is enabled or if we're benchmarking so we simply calculate it ourselves. */
double GLastTimeChartCreationTicked = 0;

/** Keep track of our previous frame's statistics */
float GLastDeltaSeconds = 0.0f;

/** Keep track of our previous frame's statistics */
double GLastHitchTime = 0;


//////////////////////////////////////////////////////////////////////

// Prints the FPS chart summary to an endpoint
struct FDumpFPSChartToEndpoint
{
	/**
	 * Dumps a chart, allowing subclasses to format the data in their own way via various protected virtuals to be overridden
	 */
	void DumpChart(float InTotalTime, float InDeltaTime, int32 InNumFrames, const FString& InMapName);

	/**
	 * Calculates the range of FPS values for the given bucket index
	 */
	static void CalcQuantisedFPSRange(int32 BucketIndex, int32& StartFPS, int32& EndFPS);

protected:
	float TotalTime;
	float DeltaTime;
	int32 NumFrames;
	FString MapName;

	float AvgFPS;
	float TimeDisregarded;
	float AvgGPUFrameTime;

	float AvgHitchesPerMinute;

	float BoundGameThreadPct;
	float BoundRenderThreadPct;
	float BoundGPUPct;

	Scalability::FQualityLevels ScalabilityQuality;
	FString OSMajor;
	FString OSMinor;

	FString CPUVendor;
	FString CPUBrand;

	FString PrimaryGPUBrand;

protected:
	virtual void PrintToEndpoint(const FString& Text) = 0;

	virtual void HandleFPSBucket(float BucketTimePercentage, float BucketFramePercentage, int32 StartFPS, int32 EndFPS)
	{
		// Log bucket index, time and frame Percentage.
		PrintToEndpoint(FString::Printf(TEXT("Bucket: %2i - %2i  Time: %5.2f  Frame: %5.2f"), StartFPS, EndFPS, BucketTimePercentage, BucketFramePercentage));
	}

	virtual void HandleHitchBucket(int32 BucketIndex)
	{
		const float HitchThresholdInSeconds = (float)GHitchThresholds[BucketIndex] * 0.001f;

		FString RangeName;
		if (BucketIndex == 0)
		{
			// First bucket's end threshold is infinitely large
			RangeName = FString::Printf(TEXT("%0.2fs - inf"), HitchThresholdInSeconds);
		}
		else
		{
			const float PrevHitchThresholdInSeconds = (float)GHitchThresholds[BucketIndex - 1] * 0.001f;

			// Set range from current bucket threshold to the last bucket's threshold
			RangeName = FString::Printf(TEXT("%0.2fs - %0.2fs"), HitchThresholdInSeconds, PrevHitchThresholdInSeconds);
		}

		PrintToEndpoint(FString::Printf(TEXT("Bucket: %s  Count: %i "), *RangeName, GHitchChart[BucketIndex].HitchCount));
	}

	virtual void HandleHitchSummary(int32 TotalHitchCount, int32 TotalGameThreadBoundHitches, int32 TotalRenderThreadBoundHitches, int32 TotalGPUBoundHitches)
	{
		const int32 HitchBucketCount = STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat;
		PrintToEndpoint(FString::Printf(TEXT("Total hitch count (at least %ims):  %i"), GHitchThresholds[HitchBucketCount - 1], TotalHitchCount));
		PrintToEndpoint(FString::Printf(TEXT("Hitch frames bound by game thread:  %i  (%0.1f percent)"), TotalGameThreadBoundHitches, (TotalHitchCount > 0) ? ((float)TotalGameThreadBoundHitches / (float)TotalHitchCount * 100.0f) : 0.0f));
		PrintToEndpoint(FString::Printf(TEXT("Hitch frames bound by render thread:  %i  (%0.1f percent)"), TotalRenderThreadBoundHitches, TotalHitchCount > 0 ? ((float)TotalRenderThreadBoundHitches / (float)TotalHitchCount * 0.0f) : 0.0f));
		PrintToEndpoint(FString::Printf(TEXT("Hitch frames bound by GPU:  %i  (%0.1f percent)"), TotalGPUBoundHitches, TotalHitchCount > 0 ? ((float)TotalGPUBoundHitches / (float)TotalHitchCount * 100.0f) : 0.0f));
		PrintToEndpoint(FString::Printf(TEXT("Hitches / min:  %.2f"), AvgHitchesPerMinute));
	}

	virtual void HandleFPSThreshold(int32 TargetFPS, int32 NumFramesBelow, float PctTimeAbove, float PctMissedFrames)
	{
		const float PercentFramesAbove = float(NumFrames - NumFramesBelow) / float(NumFrames)*100.0f;

		PrintToEndpoint(FString::Printf(TEXT("  Target %d FPS: %.2f %% syncs missed, %4.2f %% of time spent > %d FPS (%.2f %% of frames)"), TargetFPS, PctMissedFrames, PctTimeAbove, TargetFPS, PercentFramesAbove));
	}

	virtual void HandleBasicStats()
	{
		PrintToEndpoint(FString::Printf(TEXT("--- Begin : FPS chart dump for level '%s'"), *MapName));

		PrintToEndpoint(FString::Printf(TEXT("Dumping FPS chart at %s using build %s in config %s built from changelist %i"), *FDateTime::Now().ToString(), *FEngineVersion::Current().ToString(), EBuildConfigurations::ToString(FApp::GetBuildConfiguration()), GetChangeListNumberForPerfTesting()));

		PrintToEndpoint(TEXT("Machine info:"));
		PrintToEndpoint(FString::Printf(TEXT("\tOS: %s %s"), *OSMajor, *OSMinor));
		PrintToEndpoint(FString::Printf(TEXT("\tCPU: %s %s"), *CPUVendor, *CPUBrand));
		PrintToEndpoint(FString::Printf(TEXT("\tGPU: %s"), *PrimaryGPUBrand));
		PrintToEndpoint(FString::Printf(TEXT("\tResolution Quality: %.2f"), ScalabilityQuality.ResolutionQuality));
		PrintToEndpoint(FString::Printf(TEXT("\tView Distance Quality: %d"), ScalabilityQuality.ViewDistanceQuality));
		PrintToEndpoint(FString::Printf(TEXT("\tAnti-Aliasing Quality: %d"), ScalabilityQuality.AntiAliasingQuality));
		PrintToEndpoint(FString::Printf(TEXT("\tShadow Quality: %d"), ScalabilityQuality.ShadowQuality));
		PrintToEndpoint(FString::Printf(TEXT("\tPost-Process Quality: %d"), ScalabilityQuality.PostProcessQuality));
		PrintToEndpoint(FString::Printf(TEXT("\tTexture Quality: %d"), ScalabilityQuality.TextureQuality));
		PrintToEndpoint(FString::Printf(TEXT("\tEffects Quality: %d"), ScalabilityQuality.EffectsQuality));

		PrintToEndpoint(FString::Printf(TEXT("%i frames collected over %4.2f seconds, disregarding %4.2f seconds for a %4.2f FPS average"),
			NumFrames,
			DeltaTime,
			TimeDisregarded,
			AvgFPS));
		PrintToEndpoint(FString::Printf(TEXT("Average GPU frametime: %4.2f ms"), AvgGPUFrameTime));
		PrintToEndpoint(FString::Printf(TEXT("BoundGameThreadPct: %4.2f"), BoundGameThreadPct));
		PrintToEndpoint(FString::Printf(TEXT("BoundRenderThreadPct: %4.2f"), BoundRenderThreadPct));
		PrintToEndpoint(FString::Printf(TEXT("BoundGPUPct: %4.2f"), BoundGPUPct));
	}
};

//////////////////////////////////////////////////////////////////////

void FDumpFPSChartToEndpoint::CalcQuantisedFPSRange(int32 BucketIndex, int32& StartFPS, int32& EndFPS)
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

void FDumpFPSChartToEndpoint::DumpChart(float InTotalTime, float InDeltaTime, int32 InNumFrames, const FString& InMapName)
{
	TotalTime = InTotalTime;
	DeltaTime = InDeltaTime;
	NumFrames = InNumFrames;
	MapName = InMapName;

	if (TotalTime > DeltaTime)
	{
		UE_LOG(LogChartCreation, Log, TEXT("Weirdness: wall clock time (%f) is smaller than total frame time (%f)"), DeltaTime, TotalTime);
	}

	AvgFPS = NumFrames / TotalTime;
	TimeDisregarded = FMath::Max<float>(0, DeltaTime - TotalTime);
	AvgGPUFrameTime = float((GTotalGPUTime / NumFrames)*1000.0);

	BoundGameThreadPct = (float(GNumFramesBound_GameThread) / float(NumFrames))*100.0f;
	BoundRenderThreadPct = (float(GNumFramesBound_RenderThread) / float(NumFrames))*100.0f;
	BoundGPUPct = (float(GNumFramesBound_GPU) / float(NumFrames))*100.0f;

	// Get OS info
	FPlatformMisc::GetOSVersions(/*out*/ OSMajor, /*out*/ OSMinor);
	OSMajor.Trim().TrimTrailing();
	OSMinor.Trim().TrimTrailing();

	// Get CPU/GPU info
	CPUVendor = FPlatformMisc::GetCPUVendor().Trim().TrimTrailing();
	CPUBrand = FPlatformMisc::GetCPUBrand().Trim().TrimTrailing();
	PrimaryGPUBrand = FPlatformMisc::GetPrimaryGPUBrand().Trim().TrimTrailing();

	// Get settings info
	UGameUserSettings* UserSettingsObj = GEngine->GetGameUserSettings();
	check(UserSettingsObj);
	ScalabilityQuality = UserSettingsObj->ScalabilityQuality;

	// Let the derived class process the members we've set up
	HandleBasicStats();

	// keep track of the number of frames below X FPS, and the percentage of time above X FPS
	TArray<float, TInlineAllocator<4>> TimesSpentAboveThreshold;
	TArray<int32, TInlineAllocator<4>> FramesSpentBelowThreshold;
	TimesSpentAboveThreshold.AddZeroed(GTargetFrameRatesForSummary.Num());
	FramesSpentBelowThreshold.AddZeroed(GTargetFrameRatesForSummary.Num());

	// Iterate over all buckets, dumping percentages.
	for (int32 BucketIndex = 0; BucketIndex < ARRAY_COUNT(GFPSChart); BucketIndex++)
	{
		const FFPSChartEntry& ChartEntry = GFPSChart[BucketIndex];

		// Figure out bucket time and frame percentage.
		const float BucketTimePercentage = 100.f * ChartEntry.CummulativeTime / TotalTime;
		const float BucketFramePercentage = 100.f * ChartEntry.Count / NumFrames;

		// Figure out bucket range. Buckets start at 5 frame intervals then change to 10.
		int32 StartFPS = 0;
		int32 EndFPS = 0;
		CalcQuantisedFPSRange(BucketIndex, /*out*/ StartFPS, /*out*/ EndFPS);

		for (int32 ThresholdIndex = 0; ThresholdIndex < GTargetFrameRatesForSummary.Num(); ++ThresholdIndex)
		{
			const int32 FrameRateThreshold = GTargetFrameRatesForSummary[ThresholdIndex];

			if (StartFPS >= FrameRateThreshold)
			{
				TimesSpentAboveThreshold[ThresholdIndex] += BucketTimePercentage;
			}
			else
			{
				FramesSpentBelowThreshold[ThresholdIndex] += ChartEntry.Count;
			}
		}

		HandleFPSBucket(BucketTimePercentage, BucketFramePercentage, StartFPS, EndFPS);
	}

	// Handle threhsolds
	for (int32 ThresholdIndex = 0; ThresholdIndex < GTargetFrameRatesForSummary.Num(); ++ThresholdIndex)
	{
		const int32 TargetFPS = GTargetFrameRatesForSummary[ThresholdIndex];

		const float PctTimeAbove = TimesSpentAboveThreshold[ThresholdIndex];
		const int32 NumFramesBelow = FramesSpentBelowThreshold[ThresholdIndex];

		const int32 TotalTargetFrames = TargetFPS * TotalTime;
		const int32 MissedFrames = FMath::Max(TotalTargetFrames - NumFrames, 0);
		const float PctMissedFrames = (float)((MissedFrames * 100.0) / (double)TotalTargetFrames);

		HandleFPSThreshold(TargetFPS, NumFramesBelow, PctTimeAbove, PctMissedFrames);
	}

	// Dump hitch data
	{
		PrintToEndpoint(FString::Printf(TEXT("--- Begin : Hitch chart dump for level '%s'"), *MapName));

		int32 TotalHitchCount = 0;
		int32 TotalGameThreadBoundHitches = 0;
		int32 TotalRenderThreadBoundHitches = 0;
		int32 TotalGPUBoundHitches = 0;
		for (int32 BucketIndex = 0; BucketIndex < ARRAY_COUNT(GHitchChart); ++BucketIndex)
		{
			HandleHitchBucket(BucketIndex);

			// Count up the total number of hitches
			TotalHitchCount += GHitchChart[BucketIndex].HitchCount;
			TotalGameThreadBoundHitches += GHitchChart[BucketIndex].GameThreadBoundHitchCount;
			TotalRenderThreadBoundHitches += GHitchChart[BucketIndex].RenderThreadBoundHitchCount;
			TotalGPUBoundHitches += GHitchChart[BucketIndex].GPUBoundHitchCount;
		}

		AvgHitchesPerMinute = TotalHitchCount / (TotalTime / 60.0f);
		HandleHitchSummary(TotalHitchCount, TotalGameThreadBoundHitches, TotalRenderThreadBoundHitches, TotalGPUBoundHitches);

		PrintToEndpoint(TEXT("--- End"));
	}
}

//////////////////////////////////////////////////////////////////////

struct FDumpFPSChartToAnalyticsArray : public FDumpFPSChartToEndpoint
{
	FDumpFPSChartToAnalyticsArray(TArray<FAnalyticsEventAttribute>& InParamArray)
		: ParamArray(InParamArray)
	{
	}

protected:
	TArray<FAnalyticsEventAttribute>& ParamArray;
protected:
	virtual void PrintToEndpoint(const FString& Text) override
	{
	}

	virtual void HandleFPSBucket(float BucketTimePercentage, float BucketFramePercentage, int32 StartFPS, int32 EndFPS) override
	{
		ParamArray.Add(FAnalyticsEventAttribute(FString::Printf(TEXT("Bucket_%i_%i_TimePercentage"), StartFPS, EndFPS), BucketTimePercentage));
	}

	virtual void HandleHitchBucket(int32 BucketIndex) override
	{
		FString ParamName;
		if (BucketIndex == 0)
		{
			ParamName = FString::Printf(TEXT("Hitch_%i_Plus_HitchCount"), GHitchThresholds[BucketIndex]);
		}
		else
		{
			ParamName = FString::Printf(TEXT("Hitch_%i_%i_HitchCount"), GHitchThresholds[BucketIndex], GHitchThresholds[BucketIndex - 1]);
		}

		ParamArray.Add(FAnalyticsEventAttribute(ParamName, GHitchChart[BucketIndex].HitchCount));
	}

	virtual void HandleHitchSummary(int32 TotalHitchCount, int32 TotalGameThreadBoundHitches, int32 TotalRenderThreadBoundHitches, int32 TotalGPUBoundHitches) override
	{
		// Add hitch totals to the param array
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TotalHitches"), TotalHitchCount));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TotalGameBoundHitches"), TotalGameThreadBoundHitches));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TotalRenderBoundHitches"), TotalRenderThreadBoundHitches));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TotalGPUBoundHitches"), TotalGPUBoundHitches));
	}

	virtual void HandleFPSThreshold(int32 TargetFPS, int32 NumFramesBelow, float PctTimeAbove, float PctMissedFrames) override
	{
		const FString ParamName = FString::Printf(TEXT("PercentAbove%d"), TargetFPS);
		const FString ParamValue = FString::Printf(TEXT("%4.2f"), PctTimeAbove);
		ParamArray.Add(FAnalyticsEventAttribute(ParamName, ParamValue));
	}

	virtual void HandleBasicStats() override
	{
		// Add non-bucket params
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("ChangeList"), GetChangeListNumberForPerfTesting()));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("BuildType"), EBuildConfigurations::ToString(FApp::GetBuildConfiguration())));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("DateStamp"), FDateTime::Now().ToString()));

		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Platform"), FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()))));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("OS"), FString::Printf(TEXT("%s %s"), *OSMajor, *OSMinor)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("CPU"), FString::Printf(TEXT("%s %s"), *CPUVendor, *CPUBrand)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("GPU"), PrimaryGPUBrand));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("ResolutionQuality"), ScalabilityQuality.ResolutionQuality));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("ViewDistanceQuality"), ScalabilityQuality.ViewDistanceQuality));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("AntiAliasingQuality"), ScalabilityQuality.AntiAliasingQuality));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("ShadowQuality"), ScalabilityQuality.ShadowQuality));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("PostProcessQuality"), ScalabilityQuality.PostProcessQuality));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TextureQuality"), ScalabilityQuality.TextureQuality));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("FXQuality"), ScalabilityQuality.EffectsQuality));

		ParamArray.Add(FAnalyticsEventAttribute(TEXT("AvgFPS"), FString::Printf(TEXT("%4.2f"), AvgFPS)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TimeDisregarded"), FString::Printf(TEXT("%4.2f"), TimeDisregarded)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), FString::Printf(TEXT("%4.2f"), DeltaTime)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("FrameCount"), FString::Printf(TEXT("%i"), NumFrames)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("AvgGPUTime"), FString::Printf(TEXT("%4.2f"), AvgGPUFrameTime)));

		ParamArray.Add(FAnalyticsEventAttribute(TEXT("PercentGameThreadBound"), FString::Printf(TEXT("%4.2f"), BoundGameThreadPct)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("PercentRenderThreadBound"), FString::Printf(TEXT("%4.2f"), BoundRenderThreadPct)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("PercentGPUBound"), FString::Printf(TEXT("%4.2f"), BoundGPUPct)));
	}
};

//////////////////////////////////////////////////////////////////////

struct FDumpFPSChartToLogEndpoint : public FDumpFPSChartToEndpoint
{
	virtual void PrintToEndpoint(const FString& Text) override
	{
		UE_LOG(LogChartCreation, Log, TEXT("%s"), *Text);
	}
};

//////////////////////////////////////////////////////////////////////

#if ALLOW_DEBUG_FILES
struct FDumpFPSChartToFileEndpoint : public FDumpFPSChartToEndpoint
{
	FArchive* MyArchive;

	FDumpFPSChartToFileEndpoint(FArchive* InArchive)
		: MyArchive(InArchive)
	{
	}

	virtual void PrintToEndpoint(const FString& Text) override
	{
		MyArchive->Logf(TEXT("%s"), *Text);
	}
};
#endif

//////////////////////////////////////////////////////////////////////

#if ALLOW_DEBUG_FILES
struct FDumpFPSChartToHTMLEndpoint : public FDumpFPSChartToEndpoint
{
	FString& FPSChartRow;

	FDumpFPSChartToHTMLEndpoint(FString& InFPSChartRow)
		: FPSChartRow(InFPSChartRow)
	{
	}

protected:
	virtual void PrintToEndpoint(const FString& Text) override
	{
	}

	virtual void HandleFPSBucket(float BucketTimePercentage, float BucketFramePercentage, int32 StartFPS, int32 EndFPS) override
	{
		const FString SrcToken = FString::Printf(TEXT("TOKEN_%i_%i"), StartFPS, EndFPS);
		const FString DstToken = FString::Printf(TEXT("%5.2f"), BucketTimePercentage);

		// Replace token with actual values.
		FPSChartRow = FPSChartRow.Replace(*SrcToken, *DstToken, ESearchCase::CaseSensitive);
	}

	virtual void HandleHitchBucket(int32 BucketIndex) override
	{
		FString SrcToken;
		if (BucketIndex == 0)
		{
			SrcToken = FString::Printf(TEXT("TOKEN_HITCH_%i_PLUS"), GHitchThresholds[BucketIndex]);
		}
		else
		{
			SrcToken = FString::Printf(TEXT("TOKEN_HITCH_%i_%i"), GHitchThresholds[BucketIndex], GHitchThresholds[BucketIndex - 1]);
		}

		const FString DstToken = FString::Printf(TEXT("%i"), GHitchChart[BucketIndex].HitchCount);

		// Replace token with actual values.
		FPSChartRow = FPSChartRow.Replace(*SrcToken, *DstToken, ESearchCase::CaseSensitive);
	}

	virtual void HandleHitchSummary(int32 TotalHitchCount, int32 TotalGameThreadBoundHitches, int32 TotalRenderThreadBoundHitches, int32 TotalGPUBoundHitches) override
	{
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_HITCH_TOTAL"), *FString::Printf(TEXT("%i"), TotalHitchCount), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_HITCH_GAME_BOUND_COUNT"), *FString::Printf(TEXT("%i"), TotalGameThreadBoundHitches), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_HITCH_RENDER_BOUND_COUNT"), *FString::Printf(TEXT("%i"), TotalRenderThreadBoundHitches), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_HITCH_GPU_BOUND_COUNT"), *FString::Printf(TEXT("%i"), TotalGPUBoundHitches), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_HITCHES_PER_MIN"), *FString::Printf(TEXT("%.2f"), AvgHitchesPerMinute), ESearchCase::CaseSensitive);
	}

	virtual void HandleFPSThreshold(int32 TargetFPS, int32 NumFramesBelow, float PctTimeAbove, float PctMissedFrames) override
	{
		{
			const FString ParamName = FString::Printf(TEXT("TOKEN_PCT_ABOVE_%d"), TargetFPS);
			const FString ParamValue = FString::Printf(TEXT("%4.2f"), PctTimeAbove);
			FPSChartRow = FPSChartRow.Replace(*ParamName, *ParamValue, ESearchCase::CaseSensitive);
		}

		{
			const FString ParamName = FString::Printf(TEXT("TOKEN_MVP_%d"), TargetFPS);
			const FString ParamValue = FString::Printf(TEXT("%4.2f"), PctMissedFrames);
			FPSChartRow = FPSChartRow.Replace(*ParamName, *ParamValue, ESearchCase::CaseSensitive);
		}
	}

	virtual void HandleBasicStats() override
	{
		// Update non- bucket stats.
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_MAPNAME"), *FString::Printf(TEXT("%s"), *MapName), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_CHANGELIST"), *FString::Printf(TEXT("%i"), GetChangeListNumberForPerfTesting()), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_DATESTAMP"), *FString::Printf(TEXT("%s"), *FDateTime::Now().ToString()), ESearchCase::CaseSensitive);

		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_OS"), *FString::Printf(TEXT("%s %s"), *OSMajor, *OSMinor), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_CPU"), *FString::Printf(TEXT("%s %s"), *CPUVendor, *CPUBrand), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_GPU"), *FString::Printf(TEXT("%s"), *PrimaryGPUBrand), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_RES"), *FString::Printf(TEXT("%.2f"), ScalabilityQuality.ResolutionQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_VD"), *FString::Printf(TEXT("%d"), ScalabilityQuality.ViewDistanceQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_AA"), *FString::Printf(TEXT("%d"), ScalabilityQuality.AntiAliasingQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_SHADOW"), *FString::Printf(TEXT("%d"), ScalabilityQuality.ShadowQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_PP"), *FString::Printf(TEXT("%d"), ScalabilityQuality.PostProcessQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_TEX"), *FString::Printf(TEXT("%d"), ScalabilityQuality.TextureQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_FX"), *FString::Printf(TEXT("%d"), ScalabilityQuality.EffectsQuality), ESearchCase::CaseSensitive);

		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_AVG_FPS"), *FString::Printf(TEXT("%4.2f"), AvgFPS), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_TIME_DISREGARDED"), *FString::Printf(TEXT("%4.2f"), TimeDisregarded), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_TIME"), *FString::Printf(TEXT("%4.2f"), DeltaTime), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_FRAMECOUNT"), *FString::Printf(TEXT("%i"), NumFrames), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_AVG_GPUTIME"), *FString::Printf(TEXT("%4.2f ms"), AvgGPUFrameTime), ESearchCase::CaseSensitive);

		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_BOUND_GAME_THREAD_PERCENT"), *FString::Printf(TEXT("%4.2f"), BoundGameThreadPct), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_BOUND_RENDER_THREAD_PERCENT"), *FString::Printf(TEXT("%4.2f"), BoundRenderThreadPct), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_BOUND_GPU_PERCENT"), *FString::Printf(TEXT("%4.2f"), BoundGPUPct), ESearchCase::CaseSensitive);

		// Add non-bucket params
// 		ParamArray.Add(FAnalyticsEventAttribute(TEXT("BuildType"), EBuildConfigurations::ToString(FApp::GetBuildConfiguration())));
// 		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Platform"), FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()))));

		// Sum up FrameTimes and GameTimes
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_AVG_RENDTIME"), *FString::Printf(TEXT("%4.2f ms"), float((GTotalFrameTime_RenderThread / NumFrames)*1000.0)), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_AVG_GAMETIME"), *FString::Printf(TEXT("%4.2f ms"), float((GTotalFrameTime_GameThread / NumFrames)*1000.0)), ESearchCase::CaseSensitive);
	}
};
#endif

//////////////////////////////////////////////////////////////////////

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

void UEngine::TickFPSChart( float DeltaSeconds )
{
	const float MSToSeconds = 1.0f / 1000.0f;

	// early out if we aren't doing any of the captures
	if (!GEnableDataCapture)
	{
		return;
	}

	const double CurrentTime = FPlatformTime::Seconds();
	if( GLastTimeChartCreationTicked > 0 )
	{
		DeltaSeconds = CurrentTime - GLastTimeChartCreationTicked;
	}
	GLastTimeChartCreationTicked = CurrentTime;

	// now gather some stats on what this frame was bound by (game, render, gpu)

	// Copy these locally since the RT may update it between reads otherwise
	const uint32 LocalRenderThreadTime = GRenderThreadTime;
	const uint32 LocalGPUFrameTime = GGPUFrameTime;

	// determine which pipeline time is the greatest (between game thread, render thread, and GPU)
	const float EpsilonCycles = 0.250f;
	uint32 MaxThreadTimeValue = FMath::Max3<uint32>( LocalRenderThreadTime, GGameThreadTime, LocalGPUFrameTime );
	const float FrameTime = FPlatformTime::ToSeconds(MaxThreadTimeValue);

	const float EngineTargetMS = FEnginePerformanceTargets::GetTargetFrameTimeThresholdMS();

	// Try to estimate a GPU time even if the current platform does not support GPU timing
	uint32 PossibleGPUTime = LocalGPUFrameTime;
	if (PossibleGPUTime == 0)
	{
		// if we are over
		PossibleGPUTime = static_cast<uint32>(FMath::Max( FrameTime, DeltaSeconds ) / FPlatformTime::GetSecondsPerCycle() );
		MaxThreadTimeValue = FMath::Max3<uint32>( GGameThreadTime, LocalRenderThreadTime, PossibleGPUTime );
	}

	// Optionally disregard frames that took longer than one second when accumulating data.
	const bool bBinThisFrame = (DeltaSeconds < GMaximumFrameTimeToConsiderForHitchesAndBinning) || (GMaximumFrameTimeToConsiderForHitchesAndBinning <= 0.0f);
	if (bBinThisFrame)
	{
		const float CurrentFPS_Raw = 1.0f / DeltaSeconds;

		const bool bShouldRound = GRoundChartingFPSBeforeBinning.GetValueOnGameThread() != 0;
		const float CurrentFPS = bShouldRound ? FMath::RoundToFloat(CurrentFPS_Raw) : CurrentFPS_Raw;
		
		if (CurrentFPS < 5.0f)
		{
			GFPSChart[STAT_FPSChart_0_5 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_0_5 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 10.0f)
		{
			GFPSChart[STAT_FPSChart_5_10 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_5_10 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 15.0f)
		{
			GFPSChart[ STAT_FPSChart_10_15 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[ STAT_FPSChart_10_15 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 20.0f)
		{
			GFPSChart[STAT_FPSChart_15_20 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_15_20 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 25.0f)
		{
			GFPSChart[STAT_FPSChart_20_25 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_20_25 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 30.0f)
		{
			GFPSChart[STAT_FPSChart_25_30 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_25_30 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 40.0f)
		{
			GFPSChart[STAT_FPSChart_30_40 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_30_40 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 50.0f)
		{
			GFPSChart[STAT_FPSChart_40_50 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_40_50 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 60.0f)
		{
			GFPSChart[STAT_FPSChart_50_60 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_50_60 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 70.0f)
		{
			GFPSChart[STAT_FPSChart_60_70 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_60_70 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 80.0f)
		{
			GFPSChart[STAT_FPSChart_70_80 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_70_80 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 90.0f)
		{
			GFPSChart[STAT_FPSChart_80_90 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_80_90 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 100.0f)
		{
			GFPSChart[STAT_FPSChart_90_100 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_90_100 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 110.0f)
		{
			GFPSChart[STAT_FPSChart_100_110 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_100_110 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else if (CurrentFPS < 120.0f)
		{
			GFPSChart[STAT_FPSChart_110_120 - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_110_120 - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}
		else
		{
			GFPSChart[STAT_FPSChart_120_INF - STAT_FPSChartFirstStat].Count++;
			GFPSChart[STAT_FPSChart_120_INF - STAT_FPSChartFirstStat].CummulativeTime += DeltaSeconds;
		}

		GTotalGPUTime += FPlatformTime::ToSeconds(LocalGPUFrameTime);

		// if frame time is greater than our target then we are bounded by something
		const float TargetThreadTimeSeconds = EngineTargetMS * MSToSeconds;
		if (DeltaSeconds > TargetThreadTimeSeconds)
		{
			// If GPU time is inferred we can only determine GPU > threshold if we are GPU bound.
			bool bAreWeGPUBoundIfInferred = true;

			if (FPlatformTime::ToSeconds(GGameThreadTime) >= TargetThreadTimeSeconds)
			{
				GNumFramesBound_GameThread++;
				GTotalFramesBoundTime_GameThread += DeltaSeconds;
				bAreWeGPUBoundIfInferred = false;
			}

			if (FPlatformTime::ToSeconds(LocalRenderThreadTime) >= TargetThreadTimeSeconds)
			{
				GNumFramesBound_RenderThread++;
				GTotalFramesBoundTime_RenderThread += DeltaSeconds;
				bAreWeGPUBoundIfInferred = false;
			}			

			// Consider this frame GPU bound if we have an actual measurement which is over the limit,
			if (((LocalGPUFrameTime != 0) && (FPlatformTime::ToSeconds(LocalGPUFrameTime) >= TargetThreadTimeSeconds)) ||
				// Or if we don't have a measurement but neither of the other threads were the slowest
				((LocalGPUFrameTime == 0) && bAreWeGPUBoundIfInferred && (PossibleGPUTime == MaxThreadTimeValue)) )
			{
				GTotalFramesBoundTime_GPU += DeltaSeconds;
				GNumFramesBound_GPU++;
			}
		}
	}

	// Track per frame stats.

	const float RenderThreadTimeInSecs = FPlatformTime::ToSeconds(LocalRenderThreadTime);
	GTotalFrameTime_RenderThread += RenderThreadTimeInSecs;

	const float GameThreadTimeInSecs = FPlatformTime::ToSeconds(GGameThreadTime);
	GTotalFrameTime_GameThread += GameThreadTimeInSecs;

	// Capturing FPS chart info. We only use these when we intend to write out to a stats log
#if ALLOW_DEBUG_FILES
	if (GFPSChartRecordPerFrameTimes)
	{		
		GGameThreadFrameTimes.Add(GameThreadTimeInSecs);
		GRenderThreadFrameTimes.Add(RenderThreadTimeInSecs);
		GGPUFrameTimes.Add(FPlatformTime::ToSeconds(LocalGPUFrameTime));
		GFrameTimes.Add(DeltaSeconds);
	}
#endif

	// Check for hitches
	{
		// Minimum time quantum before we'll even consider this a hitch
		const float MinFrameTimeToConsiderAsHitch = FEnginePerformanceTargets::GetHitchFrameTimeThresholdMS() * MSToSeconds;

		// Ignore frames faster than our threshold
		if (DeltaSeconds >= MinFrameTimeToConsiderAsHitch)
		{
			// How long has it been since the last hitch we detected?
			const float TimeSinceLastHitch = ( float )( CurrentTime - GLastHitchTime );

			// Minimum time passed before we'll record a new hitch
			const float MinTimeBetweenHitches = FEnginePerformanceTargets::GetMinTimeBetweenHitchesMS() * MSToSeconds;

			// Make sure at least a little time has passed since the last hitch we reported
			if (TimeSinceLastHitch >= MinTimeBetweenHitches)
			{
				// For the current frame to be considered a hitch, it must have run at least this many times slower than
				// the previous frame
				const float HitchMultiplierAmount = FEnginePerformanceTargets::GetHitchToNonHitchRatio();

				// If our frame time is much larger than our last frame time, we'll count this as a hitch!
				if (DeltaSeconds > (GLastDeltaSeconds * HitchMultiplierAmount))
				{
					// We have a hitch!
					OnHitchDetectedDelegate.Broadcast(DeltaSeconds);
					
					// Track the hitch by bucketing it based on time severity
					const int32 HitchBucketCount = STAT_FPSChart_LastHitchBucketStat - STAT_FPSChart_FirstHitchStat;
					for (int32 CurBucketIndex = 0; CurBucketIndex < HitchBucketCount; ++CurBucketIndex)
					{
						const float HitchThresholdInSeconds = ( float )GHitchThresholds[ CurBucketIndex ] * MSToSeconds;
						if (DeltaSeconds >= HitchThresholdInSeconds)
						{
							// Increment the hitch count for this bucket
							++GHitchChart[ CurBucketIndex ].HitchCount;

						
							// Check to see what we were limited by this frame
							if (GGameThreadTime >= (MaxThreadTimeValue - EpsilonCycles))
							{
								// Bound by game thread
								++GHitchChart[ CurBucketIndex ].GameThreadBoundHitchCount;
							}
							else if (LocalRenderThreadTime >= (MaxThreadTimeValue - EpsilonCycles))
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

					GLastHitchTime = CurrentTime;
				}
			}
		}

		// Store stats for the next frame to look at
		GLastDeltaSeconds = DeltaSeconds;
	}
}

void UEngine::StartFPSChart(const FString& Label, bool bRecordPerFrameTimes)
{
	GFPSChartLabel = Label;
	GFPSChartRecordPerFrameTimes = bRecordPerFrameTimes;

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

#if ALLOW_DEBUG_FILES
	if (GFPSChartRecordPerFrameTimes)
	{
		// Pre-allocate 10 minutes worth of frames at 30 Hz. This is only needed if we intend to write out a stats log
		const int32 InitialNumFrames = 10 * 60 * 30;
		GRenderThreadFrameTimes.Reset(InitialNumFrames);
		GGPUFrameTimes.Reset(InitialNumFrames);
		GGameThreadFrameTimes.Reset(InitialNumFrames);
		GFrameTimes.Reset(InitialNumFrames);
	}
#endif

	GTotalFrameTime_RenderThread = 0.0;
	GTotalFrameTime_GameThread = 0.0;

	//@TODO: Drive this from a cvar
	GTargetFrameRatesForSummary.Reset();
	GTargetFrameRatesForSummary.Add(30);
	GTargetFrameRatesForSummary.Add(60);
	GTargetFrameRatesForSummary.Add(120);

	GTotalGPUTime = 0;
	GGPUFrameTime = 0;

	GNumFramesBound_GameThread = 0;
	GNumFramesBound_RenderThread = 0;
	GNumFramesBound_GPU = 0;

	GTotalFramesBoundTime_GameThread = 0;
	GTotalFramesBoundTime_RenderThread = 0;
	GTotalFramesBoundTime_GPU = 0;

	GEnableDataCapture = true;

	GLastTimeChartCreationTicked = 0;
	GLastHitchTime = 0;
	GLastDeltaSeconds = 0.0f;

	GCaptureStartTime = FDateTime::Now().ToString();

	UE_LOG(LogChartCreation, Log, TEXT("Started creating FPS charts at %f seconds"), GFPSChartStartTime);
}

void UEngine::StopFPSChart()
{
	GFPSChartStopTime = FPlatformTime::Seconds();
	GEnableDataCapture = false;

	UE_LOG(LogChartCreation, Log, TEXT("Stopped creating FPS charts at %f seconds"), GFPSChartStopTime);
}

void UEngine::DumpFPSChartToLog( float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName )
{
	FDumpFPSChartToLogEndpoint LogEndpoint;
	LogEndpoint.DumpChart(TotalTime, DeltaTime, NumFrames, InMapName);
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

void UEngine::DumpFrameTimesToStatsLog( float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName )
{
#if ALLOW_DEBUG_FILES
	// Create folder for FPS chart data.
	const FString OutputDir = CreateOutputDirectory();

	// Create archive for log data.
	const FString ChartType = GetFPSChartType();
	const FString ChartName = OutputDir / CreateFileNameForChart( ChartType, InMapName, TEXT( ".csv" ) );
	FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter( *ChartName );

	ensure(GFPSChartRecordPerFrameTimes);
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

void UEngine::DumpFPSChartToStatsLog( float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName )
{
#if ALLOW_DEBUG_FILES
	const FString OutputDir = CreateOutputDirectory();
	
	// Create archive for log data.
	const FString ChartType = GetFPSChartType();
	const FString ChartName = OutputDir / CreateFileNameForChart( ChartType, InMapName, TEXT( ".log" ) );

	if (FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter(*ChartName, FILEWRITE_Append))
	{
		FDumpFPSChartToFileEndpoint FileEndpoint(OutputFile);
		FileEndpoint.DumpChart(TotalTime, DeltaTime, NumFrames, InMapName);

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

void UEngine::DumpFPSChartToHTML( float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName )
{
#if ALLOW_DEBUG_FILES
	// Load the HTML building blocks from the Engine\Stats folder.
	FString FPSChartPreamble;
	FString FPSChartPostamble;
	FString FPSChartRow;
	bool bAreAllHTMLPartsLoaded = true;

	bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && FFileHelper::LoadFileToString( FPSChartPreamble,	*(FPaths::EngineContentDir() + TEXT("Stats/FPSChart_Preamble.html")	) );
	bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && FFileHelper::LoadFileToString( FPSChartPostamble,	*(FPaths::EngineContentDir() + TEXT("Stats/FPSChart_Postamble.html")	) );
	bAreAllHTMLPartsLoaded = bAreAllHTMLPartsLoaded && FFileHelper::LoadFileToString( FPSChartRow,		*(FPaths::EngineContentDir() + TEXT("Stats/FPSChart_Row.html")			) );

	// Successfully loaded all HTML templates.
	if( bAreAllHTMLPartsLoaded )
	{
		FDumpFPSChartToHTMLEndpoint HTMLEndpoint(FPSChartRow);
		HTMLEndpoint.DumpChart(TotalTime, DeltaTime, NumFrames, InMapName);

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

void UEngine::DumpFPSChartToAnalyticsParams(float TotalTime, float DeltaTime, int32 NumFrames, const FString& InMapName, TArray<FAnalyticsEventAttribute>& InParamArray)
{
	FDumpFPSChartToAnalyticsArray AnalyticsEndpoint(InParamArray);
	AnalyticsEndpoint.DumpChart(TotalTime, DeltaTime, NumFrames, InMapName);
}

void UEngine::DumpFPSChart( const FString& InMapName, bool bForceDump )
{
	// Iterate over all buckets, gathering total frame count and cumulative time.
	float TotalTime = 0;
	const float DeltaTime = GFPSChartStopTime - GFPSChartStartTime;
	int32 NumFrames = 0;
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
			DumpFPSChartToHTML( TotalTime, DeltaTime, NumFrames, GFPSChartLabel + "-" + GCaptureStartTime );
		}
	}
}

/**
* Dumps the FPS chart information to the passed in archive for analytics.
*
* @param	InMapName	Name of the map (Or Global)
*/
void UEngine::DumpFPSChartAnalytics(const FString& InMapName, TArray<FAnalyticsEventAttribute>& InParamArray)
{
	// Iterate over all buckets, gathering total frame count and cumulative time.
	float TotalTime = 0;
	const float DeltaTime = GFPSChartStopTime - GFPSChartStartTime;
	int32 NumFrames = 0;
	for (int32 BucketIndex = 0; BucketIndex < ARRAY_COUNT(GFPSChart); BucketIndex++)
	{
		NumFrames += GFPSChart[BucketIndex].Count;
		TotalTime += GFPSChart[BucketIndex].CummulativeTime;
	}

	bool bMemoryChartIsActive = false;
	if (FParse::Param(FCommandLine::Get(), TEXT("CaptureMemoryChartInfo")) || FParse::Param(FCommandLine::Get(), TEXT("gCMI")))
	{
		bMemoryChartIsActive = true;
	}

	if (TotalTime > 0.f
		&& NumFrames > 0
		&& bMemoryChartIsActive == false)
	{
		if (FApp::IsGame())
		{
			DumpFPSChartToAnalyticsParams(TotalTime, DeltaTime, NumFrames, InMapName, InParamArray);
		}
	}
}

void UEngine::GetFPSChartBoundByFrameCounts(uint32& OutGameThread, uint32& OutRenderThread, uint32& OutGPU) const
{
	OutGameThread = GNumFramesBound_GameThread;
	OutRenderThread = GNumFramesBound_RenderThread;
	OutGPU = GTotalFramesBoundTime_GPU;
}

#endif // DO_CHARTING
