// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintProfilerSettings.h"

//////////////////////////////////////////////////////////////////////////
// EScriptPerfDataType

enum EScriptPerfDataType
{
	Event = 0,
	Node
};

//////////////////////////////////////////////////////////////////////////
// FScriptHeatLevelMetrics

struct KISMET_API FScriptHeatLevelMetrics
{
	/** Event performance threshold */
	float EventPerformanceThreshold;
	/** Average performance threshold */
	float AveragePerformanceThreshold;
	/** Inclusive performance threshold */
	float InclusivePerformanceThreshold;
	/** Node max performance threshold */
	float MaxPerformanceThreshold;
	/** Total time performance threshold */
	float TotalTimePerformanceThreshold;
	/** Whether or not to use total time watermark when calculating total time heat level */
	bool bUseTotalTimeWaterMark;
	/** Total time watermark (note: this is intentionally global) */
	static float NodeTotalTimeWaterMark;

	/** Default constructor */
	FScriptHeatLevelMetrics()
		: EventPerformanceThreshold(0.0f)
		, AveragePerformanceThreshold(0.0f)
		, InclusivePerformanceThreshold(0.0f)
		, MaxPerformanceThreshold(0.0f)
		, TotalTimePerformanceThreshold(0.0f)
		, bUseTotalTimeWaterMark(false)
	{

	}
};

//////////////////////////////////////////////////////////////////////////
// FScriptPerfData

class KISMET_API FScriptPerfData : public TSharedFromThis<FScriptPerfData>
{
public:

	FScriptPerfData(const EScriptPerfDataType StatTypeIn, const int32 SampleFrequencyIn = 1)
		: StatType(StatTypeIn)
		, SampleFrequency(SampleFrequencyIn)
		, AverageTiming(0.0)
		, InclusiveTiming(0.0)
		, MaxTiming(-MAX_dbl)
		, MinTiming(MAX_dbl)
		, AverageTimingHeatLevel(0.f)
		, InclusiveTimingHeatLevel(0.f)
		, MaxTimingHeatLevel(0.f)
		, TotalTimingHeatLevel(0.f)
		, HottestPathHeatValue(0.f)
		, RawSamples(0)
	{
	}

	/** Add a single event node timing into the dataset */
	void AddEventTiming(const double AverageTimingIn);

	/** Add a inclusive timing into the dataset */
	void AddInclusiveTiming(const double InclusiveNodeTimingIn, const bool bIncrementSamples);

	/** Add data */
	void AddData(const FScriptPerfData& DataIn);

	/** Initialise from data set */
	void InitialiseFromDataSet(const TArray<TSharedPtr<FScriptPerfData>>& DataSet);

	/** Initialise as accumulated from data set */
	void AccumulateDataSet(const TArray<TSharedPtr<FScriptPerfData>>& DataSet);

	/** Increments sample count without affecting data */
	void TickSamples() { RawSamples++; }

	/** Reset the current data buffer and all derived data stats */
	void Reset();

	// Returns if this data structure has any valid data
	bool IsDataValid() const { return RawSamples > 0; }

	// Updates the various thresholds that control stats calcs and display.
	static void SetNumberFormattingForStats(const FNumberFormattingOptions& StatFormatIn, const FNumberFormattingOptions& TimeFormatIn);
	static void EnableBlueprintStatAverage(const bool bEnable);

	// Returns the various stats this container holds
	double GetAverageTiming() const;
	double GetInclusiveTiming() const;
	double GetMaxTiming() const { return MaxTiming; }
	double GetMinTiming() const { return MinTiming; }
	double GetTotalTiming() const { return AverageTiming; }
	float GetSampleCount() const { return RawSamples / SampleFrequency; }
	void OverrideSampleCount(const int32 NewSampleCount) { RawSamples = NewSampleCount; }

	// Returns performance heat levels for visual display
	float GetAverageHeatLevel() const { return AverageTimingHeatLevel; }
	float GetInclusiveHeatLevel() const { return InclusiveTimingHeatLevel; }
	float GetMaxTimeHeatLevel() const { return MaxTimingHeatLevel; }
	float GetTotalHeatLevel() const { return TotalTimingHeatLevel; }

	// Calculate performance heat levels for visual display
	void SetHeatLevels(TSharedPtr<FScriptHeatLevelMetrics> HeatLevelMetrics);

	// Hottest path interface
	float GetHottestPathHeatLevel() const { return HottestPathHeatValue; }
	void SetHottestPathHeatLevel(const float HottestPathValue) { HottestPathHeatValue = HottestPathValue; }

	// Returns the various stats this container holds in FText format
	FText GetTotalTimingText() const;
	FText GetInclusiveTimingText() const;
	FText GetAverageTimingText() const;
	FText GetMaxTimingText() const;
	FText GetMinTimingText() const;
	FText GetSamplesText() const;

private:

	/** The stat type for performance threshold comparisons */
	const EScriptPerfDataType StatType;
	/** The sample frequency */
	float SampleFrequency;

	/** Average timing */
	double AverageTiming;
	/** Inclusive timing */
	double InclusiveTiming;
	/** Max average timing */
	double MaxTiming;
	/** Min average timing */
	double MinTiming;
	/** Average timing heat level */
	float AverageTimingHeatLevel;
	/** Inclusive timing heat level */
	float InclusiveTimingHeatLevel;
	/** Max timing heat level */
	float MaxTimingHeatLevel;
	/** Total timing heat level */
	float TotalTimingHeatLevel;
	/** Hottest path value */
	float HottestPathHeatValue;
	/** The current raw average sample count */
	int32 RawSamples;

	/** Stat number formatting options */
	static FNumberFormattingOptions StatNumberFormat;
	/** Time number formatting options */
	static FNumberFormattingOptions TimeNumberFormat;

	/** Sets blueprint level stats as averaged */
	static bool bAverageBlueprintStats;
};
