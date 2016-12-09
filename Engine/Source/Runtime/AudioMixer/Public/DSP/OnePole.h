// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"

namespace Audio
{
	/** One Pole Feedback Filter */
	class FOnePole
	{
	public:
		/** Constructor */
		FOnePole()
			: A0(1.0f)
			, B1(0.0f)
			, Z1(0.0f)
		{
		}

		/** Destructor */
		virtual ~FOnePole()
		{
		}

		/** Initialize the LPF */
		void Init()
		{
			A0 = 1.0f;
			B1 = 0.0f;
			Z1 = 0.0f;
		}

		/** Directly set the filter coefficients */
		void SetCoefficients(const float InA0, const float InB1)
		{
			A0 = InA0;
			B1 = InB1;
		}

		/** Filter the input sample. */
		float operator()(const float InSample)
		{
			const float Yn = A0 * InSample + B1 * Z1;

			// Keep float values in full precision
			return Z1 = UnderflowClamp(Yn);
		}

	protected:
		// Coefficients
		float A0;
		float B1;

		/** One sample delay. */
		float Z1;
	};

	/** One Pole Low Pass Filter */
	class FOnePoleLPF : public FOnePole
	{
	public:
		FOnePoleLPF()
			: CutoffFrequency(0.0f)
		{
			SetFrequency(1.0f);
		}

		/** Directly sets the G-coefficient. */
		FORCEINLINE void SetG(const float InG)
		{
			B1 = InG;
			A0 = 1.0f - B1;
		}

		/** Sets the filter frequency using normalized frequency */
		FORCEINLINE void SetFrequency(const float InFrequency)
		{
			if (CutoffFrequency != InFrequency)
			{
				CutoffFrequency = InFrequency;
				B1 = FMath::Exp(-PI * CutoffFrequency);
				A0 = 1.0f - B1;
			}
		}

		float CutoffFrequency;
	};

	/** One Pole Highpass Pass Filter */
	class FOnePoleHPF : public FOnePole
	{
	public:
		FOnePoleHPF()
			: CutoffFrequency(1.0f)
		{
			SetFrequency(0.0f);
		}

		FORCEINLINE void SetFrequency(const float InFrequency)
		{
			if (CutoffFrequency != InFrequency)
			{
				CutoffFrequency = InFrequency;
				B1 = -FMath::Exp(PI * (CutoffFrequency - 0.5f));
				A0 = 1.0f + B1;
			}
		}

		float CutoffFrequency;
	};

}
