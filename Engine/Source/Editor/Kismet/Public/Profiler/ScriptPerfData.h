// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// FScriptPerfData

class KISMET_API FScriptPerfData : public TSharedFromThis<FScriptPerfData>
{
public:

	FScriptPerfData()
		: InclusiveTiming(0.0)
		, NodeTiming(0.0)
		, PureNodeTiming(0.0)
		, MaxTiming(-MAX_dbl)
		, MinTiming(MAX_dbl)
		, TotalTiming(0.0)
		, NumSamples(0)
	{
	}

	/** Add a single event node timing into the dataset */
	void AddEventTiming(const double PureNodeTimingIn, const double NodeTimingIn);

	/** Add data */
	void AddData(const FScriptPerfData& DataIn);

	/** Add branch data */
	void AddBranchData(const FScriptPerfData& DataIn);

	/** Reset the current data buffer and all derived data stats */
	void Reset();

	// Returns if this data structure has any valid data
	bool IsDataValid() const { return NumSamples > 0; }

	// Updates the various thresholds that control stats calcs and display.
	static void SetNumberFormattingForStats(const FNumberFormattingOptions& FormatIn) { StatNumberFormat = FormatIn; }
	static void SetRecentSampleBias(const float RecentSampleBiasIn);
	static void SetNodePerformanceThreshold(const float NodePerformanceThresholdIn);
	static void SetPureNodePerformanceThreshold(const float PureNodePerformanceThresholdIn);
	static void SetInclusivePerformanceThreshold(const float InclusivePerformanceThresholdIn);
	static void SetMaxPerformanceThreshold(const float MaxPerformanceThresholdIn);

	// Returns the various stats this container holds
	double GetNodeTiming() const { return NodeTiming; }
	double GetPureTiming() const { return PureNodeTiming; }
	float GetInclusiveTiming() const { return InclusiveTiming; }
	double GetMaxTiming() const { return MaxTiming; }
	double GetMinTiming() const { return MinTiming; }
	double GetTotalTiming() const { return TotalTiming; }
	int32 GetSampleCount() const { return NumSamples; }

	// Returns various performance colors for visual display
	FSlateColor GetNodeHeatColor() const;
	FSlateColor GetPureNodeHeatColor() const;
	FSlateColor GetInclusiveHeatColor() const;
	FSlateColor GetMaxTimeHeatColor() const;

	// Returns the various stats this container holds in FText format
	FText GetTotalTimingText() const;
	FText GetInclusiveTimingText() const;
	FText GetNodeTimingText() const;
	FText GetPureNodeTimingText() const;
	FText GetMaxTimingText() const;
	FText GetMinTimingText() const;
	FText GetSamplesText() const;

private:

	/** Inclusive timing, pure and node timings */
	double InclusiveTiming;
	/** Node timing */
	double NodeTiming;
	/** Pure node timing */
	double PureNodeTiming;
	/** Max exclusive timing */
	double MaxTiming;
	/** Min exclusive timing */
	double MinTiming;
	/** Total time accrued */
	double TotalTiming;
	/** The current inclusive sample count */
	int32 NumSamples;

	/** Number formatting options */
	static FNumberFormattingOptions StatNumberFormat;

	/** Controls the bias between new and older samples */
	static float RecentSampleBias;
	/** Cached Historical Sample Bias */
	static float HistoricalSampleBias;
	/** Cached node performance threshold */
	static float NodePerformanceThreshold;
	/** Cached pure node performance threshold */
	static float PureNodePerformanceThreshold;
	/** Cached inclusive performance threshold */
	static float InclusivePerformanceThreshold;
	/** Cached max performance threshold */
	static float MaxPerformanceThreshold;

};
