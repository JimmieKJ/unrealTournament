// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HapticFeedbackEffect.generated.h"

USTRUCT()
struct FHapticFeedbackDetails
{
	GENERATED_USTRUCT_BODY()

	/** The frequency to vibrate the haptic device at.  Frequency ranges vary by device! */
	UPROPERTY(EditAnywhere, Category = "HapticDetails")
	FRuntimeFloatCurve	Frequency;

	/** The amplitude to vibrate the haptic device at.  Amplitudes are normalized over the range [0.0, 1.0], with 1.0 being the max setting of the device */
	UPROPERTY(EditAnywhere, Category = "HapticDetails")
	FRuntimeFloatCurve	Amplitude;

	FHapticFeedbackDetails()
	{
	}
};

USTRUCT()
struct FActiveHapticFeedbackEffect
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	class UHapticFeedbackEffect* HapticEffect;

	FActiveHapticFeedbackEffect()
		: PlayTime(0.f)
		, Scale(1.f)
	{
	}

	FActiveHapticFeedbackEffect(UHapticFeedbackEffect* InEffect, float InScale)
		: HapticEffect(InEffect)
		, PlayTime(0.f)
	{
		Scale = FMath::Clamp(InScale, 0.f, 1.f);
	}

	bool Update(const float DeltaTime, struct FHapticFeedbackValues& Values);

private:
	float PlayTime;
	float Scale;

};

UCLASS(MinimalAPI, BlueprintType)
class UHapticFeedbackEffect : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "HapticFeedbackEffect")
	FHapticFeedbackDetails HapticDetails;

	void GetValues(const float EvalTime, FHapticFeedbackValues& Values) const;

	float GetDuration() const;
};
