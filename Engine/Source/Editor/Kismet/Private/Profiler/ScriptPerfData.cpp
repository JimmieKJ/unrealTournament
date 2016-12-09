// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Profiler/ScriptPerfData.h"

//////////////////////////////////////////////////////////////////////////
// ScriptPerfDataUIText

namespace ScriptPerfDataUIText
{
	const FText StatValue_NoData(NSLOCTEXT("ScriptProfilerDataUI", "StatValue_NoData", ""));
	const FText StatValue_NotApplicable(NSLOCTEXT("ScriptProfilerDataUI", "StatValue_NotApplicable", "--"));
};

//////////////////////////////////////////////////////////////////////////
// FScriptHeatLevelMetrics

float FScriptHeatLevelMetrics::NodeTotalTimeWaterMark = 0.f;

//////////////////////////////////////////////////////////////////////////
// FScriptPerfData

FNumberFormattingOptions FScriptPerfData::StatNumberFormat;
FNumberFormattingOptions FScriptPerfData::TimeNumberFormat;

void FScriptPerfData::AddEventTiming(const double AverageTimingIn)
{
	const double TimingInMs = AverageTimingIn * 1000.0;
	AverageTiming += TimingInMs;
	RawSamples++;
	const int32 SampleCount = GetSampleCount();
	if (SampleCount > 0)
	{
		const double NormalisedAverageTiming = GetAverageTiming();
		MaxTiming = FMath::Max<double>(MaxTiming, NormalisedAverageTiming);
		MinTiming = FMath::Min<double>(MinTiming, NormalisedAverageTiming);
	}
}

void FScriptPerfData::AddInclusiveTiming(const double InclusiveNodeTimingIn, const bool bIncrementSamples)
{
	// Discrete timings should negate the discrete nodetimings so they sum correctly later.
	InclusiveTiming += InclusiveNodeTimingIn * 1000.0;
	if (bIncrementSamples)
	{
		RawSamples++;
	}
}

void FScriptPerfData::AddData(const FScriptPerfData& DataIn)
{
	if (DataIn.IsDataValid())
	{
		// Accumulate data, find min, max values
		InclusiveTiming += DataIn.InclusiveTiming;
		AverageTiming += DataIn.AverageTiming;
		RawSamples += DataIn.RawSamples;
		MaxTiming = FMath::Max<double>(MaxTiming, DataIn.MaxTiming);
		MinTiming = FMath::Min<double>(MinTiming, DataIn.MinTiming);
	}
}

void FScriptPerfData::CreateBlueprintStats(const TArray<TSharedPtr<FScriptPerfData>>& BlueprintDataSet)
{
	Reset();
	SampleFrequency = 0.f;
	double NewMaxTiming = 0.0;
	double NewMinTiming = 0.0;
	for (auto DataPoint : BlueprintDataSet)
	{
		if (DataPoint.IsValid())
		{
			const float SampleCount = DataPoint->GetSampleCount();
			const double AverageTimeTemp = DataPoint->GetAverageTiming();
			AverageTiming += AverageTimeTemp * SampleCount;
			InclusiveTiming += (DataPoint->GetInclusiveTiming() - AverageTimeTemp) * SampleCount;
			NewMaxTiming += DataPoint->GetMaxTiming();
			NewMinTiming += DataPoint->GetMinTiming();
			// Either sum the samples to get the average or use the highest sample rate encountered in the dataset.
			RawSamples += SampleCount;
			SampleFrequency += 1.f;
		}
	}
	// Update min/max timings if the data is valid.
	if (NewMaxTiming > 0.0)
	{
		MaxTiming = NewMaxTiming;
		MinTiming = NewMinTiming;
	}
	SampleFrequency = FMath::Max(1.f, SampleFrequency);
}

void FScriptPerfData::AccumulateDataSet(const TArray<TSharedPtr<FScriptPerfData>>& DataSet, bool bApplyAsInclusive)
{
	Reset();
	SampleFrequency = 0.f;
	double NewMaxTiming = 0.0;
	double NewMinTiming = 0.0;
	for (auto DataPoint : DataSet)
	{
		if (DataPoint.IsValid())
		{
			const float SampleCount = DataPoint->GetSampleCount();
			if (bApplyAsInclusive)
			{
				InclusiveTiming += DataPoint->GetInclusiveTiming() * SampleCount;
			}
			else
			{
				const double AverageTimeTemp = DataPoint->GetAverageTiming();
				AverageTiming += AverageTimeTemp * SampleCount;
				InclusiveTiming += (DataPoint->GetInclusiveTiming() - AverageTimeTemp) * SampleCount;
			}
			NewMaxTiming += DataPoint->GetMaxTiming();
			NewMinTiming += DataPoint->GetMinTiming();
			// Either sum the samples to get the average or use the highest sample rate encountered in the dataset.
			RawSamples += SampleCount;
			SampleFrequency += 1.f;
		}
	}
	// Update min/max timings if the data is valid.
	if (NewMaxTiming > 0.0)
	{
		MaxTiming = NewMaxTiming;
		MinTiming = NewMinTiming;
	}
	SampleFrequency = FMath::Max(1.f, SampleFrequency);
}

void FScriptPerfData::Reset()
{
	AverageTiming = 0.0;
	InclusiveTiming = 0.0;
	AverageTimingHeatLevel = 0.f;
	InclusiveTimingHeatLevel = 0.f;
	MaxTimingHeatLevel = 0.f;
	TotalTimingHeatLevel = 0.f;
	HottestPathHeatValue = 0.f;
	RawSamples = 0;
	MaxTiming = -MAX_dbl;
	MinTiming = MAX_dbl;
}

void FScriptPerfData::SetNumberFormattingForStats(const FNumberFormattingOptions& StatFormatIn, const FNumberFormattingOptions& TimeFormatIn)
{
	StatNumberFormat = StatFormatIn;
	TimeNumberFormat = TimeFormatIn;
}

double FScriptPerfData::GetAverageTiming() const
{
	// We need to use the GetSampleCount() call so we can take account of both the 
	// SampleBase (re-entrant handling) and the RawSamples.
	const float SampleCount = GetSampleCount();
	return SampleCount == 0.0f ? 0.0 : (AverageTiming / GetSampleCount());
}

double FScriptPerfData::GetInclusiveTiming() const
{
	// For inclusive timings we just use RawSamples as there is no re-entrant handling.
	const double InclusiveTime = RawSamples == 0 ? 0.0 : (InclusiveTiming / RawSamples);
	return GetAverageTiming() + InclusiveTime;
}

FText FScriptPerfData::GetAverageTimingText() const
{
	if (AverageTiming != 0.f)
	{
		const double Value = GetAverageTiming();
		return FText::AsNumber(Value, &StatNumberFormat);
	}
	else
	{
		return GetSampleCount() > 0.f ? ScriptPerfDataUIText::StatValue_NotApplicable : ScriptPerfDataUIText::StatValue_NoData;
	}
}

FText FScriptPerfData::GetInclusiveTimingText() const
{
	const double Value = GetInclusiveTiming();
	if (Value != 0.f)
	{
		return FText::AsNumber(Value, &StatNumberFormat);
	}
	else
	{
		return GetSampleCount() > 0.f ? ScriptPerfDataUIText::StatValue_NotApplicable : ScriptPerfDataUIText::StatValue_NoData;
	}
}

FText FScriptPerfData::GetMaxTimingText() const
{
	if (MaxTiming != -MAX_dbl)
	{
		return FText::AsNumber(MaxTiming, &StatNumberFormat);
	}
	else
	{
		return GetSampleCount() > 0.f ? ScriptPerfDataUIText::StatValue_NotApplicable : ScriptPerfDataUIText::StatValue_NoData;
	}
}

FText FScriptPerfData::GetMinTimingText() const
{
	if (MinTiming != MAX_dbl)
	{
		return FText::AsNumber(MinTiming, &StatNumberFormat);
	}
	else
	{
		return GetSampleCount() > 0.f ? ScriptPerfDataUIText::StatValue_NotApplicable : ScriptPerfDataUIText::StatValue_NoData;
	}
}

FText FScriptPerfData::GetTotalTimingText() const
{
	if (GetSampleCount() > 0.f)
	{
		return FText::AsNumber(AverageTiming, &TimeNumberFormat);
	}
	return ScriptPerfDataUIText::StatValue_NoData;
}

FText FScriptPerfData::GetSamplesText() const
{
	const int32 SampleCount = GetSampleCount();
	if (SampleCount > 0)
	{
		return FText::AsNumber(SampleCount);
	}
	return ScriptPerfDataUIText::StatValue_NoData;
}

FText FScriptPerfData::GetHottestPathText() const
{
	return FText::AsNumber(HottestPathHeatValue);
}

FText FScriptPerfData::GetHeatLevelText() const
{
	return FText::AsNumber(AverageTimingHeatLevel);
}

void FScriptPerfData::SetHeatLevels(TSharedPtr<const FScriptHeatLevelMetrics> HeatLevelMetrics)
{
	if (ensure(HeatLevelMetrics.IsValid()))
	{
		// Note: We're intentionally not accessing AverageTiming/InclusiveTiming members directly here - the accessors make some internal adjustments.
		const float AverageThreshold = (StatType == EScriptPerfDataType::Event) ? HeatLevelMetrics->EventPerformanceThreshold : HeatLevelMetrics->AveragePerformanceThreshold;
		AverageTimingHeatLevel = FMath::Min<float>(GetAverageTiming() * AverageThreshold, 1.f);
		InclusiveTimingHeatLevel = FMath::Min<float>(GetInclusiveTiming() * HeatLevelMetrics->InclusivePerformanceThreshold, 1.f);
		MaxTimingHeatLevel = MaxTiming == -MAX_dbl ? 0.f : FMath::Min<float>(MaxTiming * HeatLevelMetrics->MaxPerformanceThreshold, 1.f);

		if (HeatLevelMetrics->bUseTotalTimeWaterMark)
		{
			HeatLevelMetrics->NodeTotalTimeWaterMark = FMath::Max<float>(AverageTiming, HeatLevelMetrics->NodeTotalTimeWaterMark);
			TotalTimingHeatLevel = FMath::Min<float>(AverageTiming / HeatLevelMetrics->NodeTotalTimeWaterMark, 1.f);
		}
		else
		{
			TotalTimingHeatLevel = FMath::Min<float>(AverageTiming * HeatLevelMetrics->TotalTimePerformanceThreshold, 1.f);
		}
	}
	else
	{
		AverageTimingHeatLevel = 0.0f;
		InclusiveTimingHeatLevel = 0.0f;
		MaxTimingHeatLevel = 0.0f;
		TotalTimingHeatLevel = 0.0f;
	}
}
