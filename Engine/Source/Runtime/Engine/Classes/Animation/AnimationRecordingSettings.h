// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimationRecordingSettings.generated.h"

/** Settings describing how to record an animation */
USTRUCT()
struct FAnimationRecordingSettings
{
	GENERATED_BODY()

	/** 30Hz default sample rate */
	static const int32 DefaultSampleRate = DEFAULT_SAMPLERATE;

	/** 1 minute default length */
	static const int32 DefaultMaximumLength = 1.0f * 60.0f;

	/** Length used to specify unbounded */
	static const int32 UnboundedMaximumLength = 0.0f;

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