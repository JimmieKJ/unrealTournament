// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DSP/BiQuadFilter.h"

namespace Audio
{
	FBiquadFilter::FBiquadFilter()
	{
		FMemory::Memzero(this, sizeof(FBiquadFilter));
	}

	FBiquadFilter::FBiquadFilter(const EBiquadFilter::Type InFilterType, const float InCutoffFrequency, const float InQ, const float InGainDB)
		: X1(0.0f)
		, X2(0.0f)
		, Y1(0.0f)
		, Y2(0.0f)
	{
		SetParams(InFilterType, InGainDB, InCutoffFrequency, InQ);
	}

	FBiquadFilter::~FBiquadFilter()
	{
	}

	void FBiquadFilter::SetParams(const EBiquadFilter::Type InFilterType, const float InGainDB, const float InCutoffFrequency, const float InQ)
	{
		FilterType = InFilterType;
		GainDB = InGainDB;
		CutoffFrequency = InCutoffFrequency;
		Q = InQ;

		float Omega = PI * CutoffFrequency;
		float SinOmega = FMath::Sin(Omega);
		float CosOmega = FMath::Cos(Omega);

		static const float NaturalLog2 = 0.69314718056f;
		float Alpha = 0.5f * SinOmega / Q;

		// Temporary coefficients
		float a0, a1, a2, b0, b1, b2;

		switch (FilterType)
		{
		default:
		case EBiquadFilter::None:
			UE_LOG(LogTemp, Error, TEXT("Invalid Filter Parameter for FBiquadFilter"));
			return;
			break;

		case EBiquadFilter::LowPass:
		{
			// H(s) = 1 / (s^2 + s/Q + 1)
			b0 = 0.5f * (1.0f - CosOmega);
			b1 = 1.0f - CosOmega;
			b2 = b0;
			a0 = 1.0f + Alpha;
			a1 = -2.0f * CosOmega;
			a2 = 1.0f - Alpha;
		}
		break;

		case EBiquadFilter::HighPass:
		{
			// H(s) = s^2 / (s^2 + s/Q + 1)
			b0 = 0.5f * (1.0f + CosOmega);
			b1 = -1.0f - CosOmega;
			b2 = b0;
			a0 = 1.0f + Alpha;
			a1 = -2.0f * CosOmega;
			a2 = 1.0f - Alpha;
		}
		break;

		case EBiquadFilter::BandPass:
		{
			// H(s) = (s / Q) / (s ^ 2 + s / Q + 1)      (constant 0 dB peak gain)
			b0 = Alpha;
			b1 = 0.0f;
			b2 = -Alpha;
			a0 = 1.0f + Alpha;
			a1 = -2.0f * CosOmega;
			a2 = 1.0f - Alpha;
		}
		break;

		case EBiquadFilter::Notch:
		{
			// H(s) = (s^2 + 1) / (s^2 + s/Q + 1)
			b0 = 1.0f;
			b1 = -2.0f * CosOmega;
			b2 = 1.0f;
			a0 = 1.0f + Alpha;
			a1 = -2.0f * CosOmega;
			a2 = 1.0f - Alpha;
		}
		break;

		case EBiquadFilter::AllPass:
		{
			// H(s) = (s^2 - s/Q + 1) / (s^2 + s/Q + 1)
			b0 = 1.0f - Alpha;
			b1 = -2.0f * CosOmega;
			b2 = 1.0f + Alpha;
			a0 = 1.0f + Alpha;
			a1 = -2.0f * CosOmega;
			a2 = 1.0f - Alpha;
		}
		break;

		case EBiquadFilter::PeakEQ:
		{
			// H(s) = (s^2 + s*(A/Q) + 1) / (s^2 + s/(A*Q) + 1)
			// Divide by 40 for peaking and shelving EQ filters
			float A = FMath::Pow(10.0f, GainDB / 40.0f);
			b0 = 1.0f + Alpha * A;
			b1 = -2.0f * CosOmega;
			b2 = 1.0f - Alpha * A;
			a0 = 1.0f + Alpha / A;
			a1 = -2.0f * CosOmega;
			a2 = 1.0f - Alpha / A;
		}
		break;

		case EBiquadFilter::LowShelf:
		{
			// H(s) = A * (s^2 + (sqrt(A)/Q)*s + A)/(A*s^2 + (sqrt(A)/Q)*s + 1)
			// Divide by 40 for peaking and shelving EQ filters
			float A = FMath::Pow(10.0f, GainDB / 40.0f);
			float SqrtA = FMath::Sqrt(A);

			b0 = A * ((A + 1.0f) - (A - 1.0f) * CosOmega + 2.0f * SqrtA * Alpha);
			b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * CosOmega);
			b2 = A * ((A + 1.0f) - (A - 1.0f) * CosOmega - 2.0f * SqrtA * Alpha);
			a0 = (A + 1.0f) + (A - 1.0f) * CosOmega + 2.0f * SqrtA * Alpha;
			a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * CosOmega);
			a2 = (A + 1.0f) + (A - 1.0f) * CosOmega - 2.0f * SqrtA * Alpha;
		}
		break;

		case EBiquadFilter::HighShelf:
		{
			float A = FMath::Pow(10.0f, GainDB / 40.0f);
			float SqrtA = FMath::Sqrt(A);

			b0 = A * ((A + 1.0f) + (A - 1.0f) * CosOmega + 2.0f * SqrtA * Alpha);
			b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * CosOmega);
			b2 = A * ((A + 1.0f) + (A - 1.0f) * CosOmega - 2.0f * SqrtA * Alpha);
			a0 = (A + 1.0f) - (A - 1.0f) * CosOmega + 2.0f * SqrtA * Alpha;
			a1 = -2.0f * ((A - 1.0f) - (A + 1.0f) * CosOmega);
			a2 = (A + 1.0f) - (A - 1.0f) * CosOmega - 2.0f * SqrtA * Alpha;
		}
		break;
		}

		// Set the filter coefficients normalizing by a0
		B0 = b0 / a0;
		B1 = b1 / a0;
		B2 = b2 / a0;
		A1 = a1 / a0;
		A2 = a2 / a0;
	}

	void FBiquadFilter::SetType(const EBiquadFilter::Type InFilterType)
	{
		if (FilterType != InFilterType)
		{
			SetParams(InFilterType, GainDB, CutoffFrequency, Q);
		}
	}

	void FBiquadFilter::SetGain(const float InGainDB)
	{
		if (GainDB != InGainDB)
		{
			SetParams(FilterType, InGainDB, CutoffFrequency, Q);
		}
	}

	void FBiquadFilter::SetCutoffFrequency(const float InCutoffFrequency)
	{
		if (CutoffFrequency != InCutoffFrequency)
		{
			SetParams(FilterType, GainDB, InCutoffFrequency, Q);
		}
	}

	void FBiquadFilter::SetQ(const float InQ)
	{
		if (Q != InQ)
		{
			SetParams(FilterType, GainDB, CutoffFrequency, InQ);
		}
	}

	void FBiquadFilter::SetBandwidth(const float InBandwidth)
	{
		if (Bandwidth != InBandwidth)
		{
			Bandwidth = InBandwidth;
			const float InQ = GetQFromBandWidth(InBandwidth, CutoffFrequency);
			SetQ(InQ);
		}
	}

	FBiquadFilter FBiquadFilter::CreateLowPassFilter(const float InCutoffFrequency, const float InQ)
	{
		return FBiquadFilter(EBiquadFilter::LowPass, InCutoffFrequency, InQ);
	}

	FBiquadFilter FBiquadFilter::CreateHighPassFilter(const float InCutoffFrequency, const float InQ)
	{
		return FBiquadFilter(EBiquadFilter::HighPass, InCutoffFrequency, InQ);
	}

	FBiquadFilter FBiquadFilter::CreateBandPassFilter(const float InCutoffFrequency, const float InQ)
	{
		return FBiquadFilter(EBiquadFilter::BandPass, InCutoffFrequency, InQ);
	}

}
