// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "ScriptPerfData.h"

//////////////////////////////////////////////////////////////////////////
// FScriptPerfData

FNumberFormattingOptions FScriptPerfData::StatNumberFormat;
float FScriptPerfData::RecentSampleBias = 0.2f;
float FScriptPerfData::HistoricalSampleBias = 0.8f;
float FScriptPerfData::NodePerformanceThreshold = 1.f;
float FScriptPerfData::PureNodePerformanceThreshold = 1.f;
float FScriptPerfData::InclusivePerformanceThreshold = 1.5f;
float FScriptPerfData::MaxPerformanceThreshold = 2.f;

void FScriptPerfData::AddEventTiming(const double PureNodeTimingIn, const double NodeTimingIn)
{
	const double TimingInMs = NodeTimingIn * 1000;
	const double PureNodeTimingInMs = PureNodeTimingIn * 1000;
	NodeTiming = (TimingInMs * RecentSampleBias) + (NodeTiming * HistoricalSampleBias);
	PureNodeTiming = (PureNodeTimingInMs * RecentSampleBias) + (PureNodeTiming * HistoricalSampleBias);
	InclusiveTiming = NodeTiming + PureNodeTiming;
	TotalTiming += NodeTimingIn + PureNodeTimingIn;
	NumSamples++;
	MaxTiming = FMath::Max<double>(MaxTiming, InclusiveTiming);
	MinTiming = FMath::Min<double>(MinTiming, InclusiveTiming);
}

void FScriptPerfData::AddData(const FScriptPerfData& DataIn)
{
	if (DataIn.IsDataValid())
	{
		// Accumulate data, find min, max values
		NodeTiming += DataIn.NodeTiming;
		PureNodeTiming += DataIn.PureNodeTiming;
		InclusiveTiming = NodeTiming + PureNodeTiming;
		TotalTiming += DataIn.TotalTiming;
		NumSamples += DataIn.NumSamples;
		MaxTiming = FMath::Max<double>(MaxTiming, DataIn.MaxTiming);
		MinTiming = FMath::Min<double>(MinTiming, DataIn.MinTiming);
	}
}

void FScriptPerfData::AddBranchData(const FScriptPerfData& DataIn)
{
	if (DataIn.IsDataValid())
	{
		NumSamples += DataIn.NumSamples;
	}
}

void FScriptPerfData::Reset()
{
	NodeTiming = 0.0;
	PureNodeTiming = 0.0;
	InclusiveTiming = 0.0;
	TotalTiming = 0.0;
	NumSamples = 0;
	MaxTiming = -MAX_dbl;
	MinTiming = MAX_dbl;
}

void FScriptPerfData::SetRecentSampleBias(const float RecentSampleBiasIn)
{
	RecentSampleBias = RecentSampleBiasIn;
	HistoricalSampleBias = 1.f - RecentSampleBias;
}

void FScriptPerfData::SetNodePerformanceThreshold(const float NodePerformanceThresholdIn)
{
	NodePerformanceThreshold = NodePerformanceThresholdIn;
}

FSlateColor FScriptPerfData::GetNodeHeatColor() const
{
	const float Value = 1.f - FMath::Min<float>(NodeTiming / NodePerformanceThreshold, 1.f);
	return FLinearColor(1.f, Value, Value);
}

void FScriptPerfData::SetPureNodePerformanceThreshold(const float PureNodePerformanceThresholdIn)
{
	PureNodePerformanceThreshold = PureNodePerformanceThresholdIn;
}

FSlateColor FScriptPerfData::GetPureNodeHeatColor() const
{
	const float Value = 1.f - FMath::Min<float>(PureNodeTiming / PureNodePerformanceThreshold, 1.f);
	return FLinearColor(1.f, Value, Value);
}

void FScriptPerfData::SetInclusivePerformanceThreshold(const float InclusivePerformanceThresholdIn)
{
	InclusivePerformanceThreshold = InclusivePerformanceThresholdIn;
}

FSlateColor FScriptPerfData::GetInclusiveHeatColor() const
{
	const float Value = 1.f - FMath::Min<float>(InclusiveTiming / InclusivePerformanceThreshold, 1.f);
	return FLinearColor(1.f, Value, Value);
}

void FScriptPerfData::SetMaxPerformanceThreshold(const float MaxPerformanceThresholdIn)
{
	MaxPerformanceThreshold = MaxPerformanceThresholdIn;
}

FSlateColor FScriptPerfData::GetMaxTimeHeatColor() const
{
	const float Value = 1.f - FMath::Min<float>(MaxTiming / MaxPerformanceThreshold, 1.f);
	return FLinearColor(1.f, Value, Value);
}

FText FScriptPerfData::GetNodeTimingText() const
{
	return FText::AsNumber(GetNodeTiming(), &StatNumberFormat);
}

FText FScriptPerfData::GetPureNodeTimingText() const
{
	const double CurrentPureTiming = GetPureTiming();
	if (CurrentPureTiming == 0.0)
	{
		return FText::GetEmpty();
	}
	else
	{
		return FText::AsNumber(CurrentPureTiming, &StatNumberFormat);
	}
}

FText FScriptPerfData::GetInclusiveTimingText() const
{
	const double CurrentPureTiming = GetPureTiming();
	if (CurrentPureTiming == 0.0)
	{
		return FText::GetEmpty();
	}
	else
	{
		return FText::AsNumber(GetInclusiveTiming(), &StatNumberFormat);
	}
}

FText FScriptPerfData::GetMaxTimingText() const
{
	const double MaximumTime = GetMaxTiming();
	const bool bMaxTimeInitialised = MaximumTime != -MAX_dbl;
	return FText::AsNumber(bMaxTimeInitialised ? MaximumTime : 0.0, &StatNumberFormat);
}

FText FScriptPerfData::GetMinTimingText() const
{
	const double MinimumTime = GetMinTiming();
	const bool bMinTimeInitialised = MinimumTime != MAX_dbl;
	return FText::AsNumber(bMinTimeInitialised ? MinimumTime : 0.0, &StatNumberFormat);
}

FText FScriptPerfData::GetTotalTimingText() const
{
	// Covert back up to seconds for total time.
	return FText::AsNumber(GetTotalTiming(), &StatNumberFormat);
}

FText FScriptPerfData::GetSamplesText() const
{
	return FText::AsNumber(GetSampleCount());
}