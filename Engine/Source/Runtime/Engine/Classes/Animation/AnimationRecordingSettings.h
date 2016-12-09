// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimationRecordingSettings.generated.h"

/** Settings describing how to record an animation */
USTRUCT()
struct ENGINE_API FAnimationRecordingSettings
{
	GENERATED_BODY()

	/** 30Hz default sample rate */
	static const float DefaultSampleRate;

	/** 1 minute default length */
	static const float DefaultMaximumLength;

	/** Length used to specify unbounded */
	static const float UnboundedMaximumLength;

	FAnimationRecordingSettings()
		: bRecordInWorldSpace(true)
		, bRemoveRootAnimation(true)
		, bAutoSaveAsset(false)
		, SampleRate((float)DefaultSampleRate)
		, Length((float)DefaultMaximumLength)
	{}

	/** Whether to record animation in world space, defaults to true */
	UPROPERTY(EditAnywhere, Category = "Settings")
	bool bRecordInWorldSpace;

	/** Whether to remove the root bone transform from the animation */
	UPROPERTY(EditAnywhere, Category = "Settings")
	bool bRemoveRootAnimation;

	/** Whether to auto-save asset when recording is completed. Defaults to false */
	UPROPERTY(EditAnywhere, Category = "Settings")
	bool bAutoSaveAsset;

	/** Sample rate of the recorded animation (in Hz) */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float SampleRate;

	/** Maximum length of the animation recorded (in seconds). If zero the animation will keep on recording until stopped. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float Length;
};
