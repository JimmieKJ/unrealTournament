// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/BiQuadFilter.h"

namespace Audio
{
	/** Parameteric equalizer. A band of parallel biquad PeakEQ filters. */
	class FParametricEqualizer
	{
	public:
		/** Constructor */
		FParametricEqualizer();

		/** Destructor */
		~FParametricEqualizer();

		/** Struct defining each band's parameters. */
		struct FEqualizerBandParams
		{
			float CutoffFrequency;
			float Bandwidth;
			float Gain;
		};

		/** Sets (or overrides) the parameters of each of the bands. The number of filters used will be the size of the array. */
		void SetParameters(const TArray<FEqualizerBandParams>& InParams);

		/** Sets given band's parameters. Returns false if the band doesn't exist. **/
		bool SetBand(const int32 InBand, const FEqualizerBandParams& InParams);

		/** Process input sample to generate new output sample. */
		FORCEINLINE float operator()(const float Input)
		{
			float Output = 0.0f;
			for (int32 i = 0; i < Filters.Num(); ++i)
			{
				Output += Filters[i](Input);
			}
			return Output;
		}

	private:

		TArray<FBiquadFilter> Filters;
	};



}
