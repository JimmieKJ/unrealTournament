// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DSP/ParametricEqualizer.h"

namespace Audio
{
	FParametricEqualizer::FParametricEqualizer()
	{
	}

	FParametricEqualizer::~FParametricEqualizer()
	{
	}

	void FParametricEqualizer::SetParameters(const TArray<FEqualizerBandParams>& InParams)
	{
		Filters.Reset();
		for (int32 i = 0; i < InParams.Num(); ++i)
		{
			const float Q = GetQFromBandWidth(InParams[i].Bandwidth, InParams[i].CutoffFrequency);
			Filters.Add(FBiquadFilter(EBiquadFilter::PeakEQ, InParams[i].CutoffFrequency, Q, InParams[i].Gain));
		}
	}

	bool FParametricEqualizer::SetBand(const int32 InBand, const FEqualizerBandParams& InParams)
	{
		if (InBand >= 0 && InBand < Filters.Num())
		{
			const float Q = GetQFromBandWidth(InParams.Bandwidth, InParams.CutoffFrequency);
			Filters[InBand].SetParams(EBiquadFilter::PeakEQ, InParams.CutoffFrequency, Q, InParams.Gain);
			return true;
		}
		return false;
	}
}
