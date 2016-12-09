// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Audio
{
	/**************************************************************************************
	Digital biquad filter derived from analog prototypes using bi-linear transform.

	Parameters:
	- SampleRate :			Sample rate (e.g. 44.1 kHz) of the filter runs at.
	- CutoffFrequency :		Normalized frequency at which the filter (whichever type) does its thing.
	- GainDB :				Amplitude gain in dB (used only in peak and shelve filters)
	- Q :					Filter quality, amount of sharpness in the frequency response
	- Bandwidth :			Bandwidth of filter (used alternatively to Q)


	Note: Normalized frequency, Fn, is:

	Fn = 2.0f * Fhz / SampleRate

	where Fn is in range of (0.0f, 1.0f]

	See:
	https://en.wikipedia.org/wiki/Digital_biquad_filter
	***************************************************************************************/

	namespace EBiquadFilter
	{
		enum Type
		{
			None,

			LowPass,
			HighPass,
			BandPass,
			Notch,
			AllPass,
			PeakEQ,
			LowShelf,
			HighShelf,
		};
	};

	/** Helper function to get Q from a given bandwidth */
	FORCEINLINE float GetQFromBandWidth(const float InBandwidth, const float CutoffFrequency)
	{
		static const float NaturalLog2 = 0.69314718056f;
		const float Omega = PI * CutoffFrequency;
		float InQ = 1.0f / (2.0f * sinh(0.5f * NaturalLog2 * InBandwidth * Omega / FMath::Sin(Omega)));
		return InQ;
	}

	class FBiquadFilter
	{
	public:
		/** Default constructor */
		FBiquadFilter();

		/** Constructor to initialize type and parameters */
		FBiquadFilter(const EBiquadFilter::Type InFilterType, const float InCutoffFrequency, const float InQ, const float InGainDB = 0.0f);

		/** Virtual destructor */
		virtual ~FBiquadFilter();

		/** Process input sample to generate next sample. */
		FORCEINLINE float operator()(const float InSample)
		{
			float Output = B0 * InSample + B1 * X1 + B2 * X2 - A1 * Y1 - A2 * Y2;

			// Only accept full-precision floats, clamp to 0.0 for subnormal precision
			if ((Output > 0.0f && Output < FLT_MIN) || (Output < 0.0f && Output > -FLT_MIN))
			{
				Output = 0.0f;
			}

			X2 = X1;
			X1 = InSample;
			Y2 = Y1;
			Y1 = Output;

			return Output;
		}

		/** Parameter setting functions. */
		void SetParams(const EBiquadFilter::Type InFilterType, const float InGainDB, const float InCutoffFrequency, const float IQ);

		/** Set the biquad filter type. */
		void SetType(const EBiquadFilter::Type InFilterType);

		/** Set the gain. */
		void SetGain(const float InGainDB);

		/** Sets the frequency of the filter */
		void SetCutoffFrequency(const float InCutoffFrequency);

		/** Sets the Q of the filter */
		void SetQ(const float InQ);

		/** Sets the bandwidth of the filter. (Alternative to setting Q) */
		void SetBandwidth(const float InBandwidth);

		/** Static functions to create filters of different types */
		static FBiquadFilter CreateLowPassFilter(const float InCutoffFrequency, const float InQ);
		static FBiquadFilter CreateHighPassFilter(const float InCutoffFrequency, const float InQ);
		static FBiquadFilter CreateBandPassFilter(const float InCutoffFrequency, const float InQ);

	protected:

		/** The current filter type. */
		EBiquadFilter::Type FilterType;

		/** Generic filter parameters. */
		float CutoffFrequency;
		float Q;
		float GainDB;
		float Bandwidth;

		/** Feedforward and feedback Sample values */
		float X1; // X(n) * Z^-1
		float X2; // X(n) * Z^-2
		float Y1; // Y(n) * Z^-1
		float Y2; // Y(n) * Z^-2

		/** Filter coefficients */
		float B0;
		float B1;
		float B2;
		float A1;
		float A2;
	};
}
