// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "MovieSceneSectionParameters.generated.h"

USTRUCT()
struct FMovieSceneSectionParameters
{
	GENERATED_BODY()

	/** Default constructor */
	FMovieSceneSectionParameters()
		: StartOffset(0.0f)
		, TimeScale(1.0f)
		, PrerollTime(0.0f)
		, PostrollTime(0.0f)
	{}

	/** Number of seconds to skip at the beginning of the sub-sequence. */
	UPROPERTY(EditAnywhere, Category="Clipping")
	float StartOffset;

	/** Playback time scaling factor. */
	UPROPERTY(EditAnywhere, Category="Timing")
	float TimeScale;

	/** Amount of time to evaluate the section before its actual physical start time. */
	UPROPERTY(EditAnywhere, Category="Timing")
	float PrerollTime;

	/** Amount of time to evaluate the section after its actual physical end time. */
	UPROPERTY(EditAnywhere, Category="Timing")
	float PostrollTime;
};
