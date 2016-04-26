// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneCaptureSettings.h"
#include "MovieSceneCaptureProtocolSettings.generated.h"

UCLASS()
class MOVIESCENECAPTURE_API UMovieSceneCaptureProtocolSettings : public UObject
{
public:

	GENERATED_BODY()

	/**
	 * Called when this protocol has been released
	 */
	virtual void OnReleaseConfig(FMovieSceneCaptureSettings& InSettings) {}

	/**
	 * Called when this protocol has been loaded
	 */
	virtual void OnLoadConfig(FMovieSceneCaptureSettings& InSettings) {}
};

