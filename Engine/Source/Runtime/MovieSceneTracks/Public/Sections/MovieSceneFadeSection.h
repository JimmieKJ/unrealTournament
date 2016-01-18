// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneFloatSection.h"
#include "MovieSceneFadeSection.generated.h"


/**
 * A single floating point section
 */
UCLASS(MinimalAPI)
class UMovieSceneFadeSection
	: public UMovieSceneFloatSection
{
	GENERATED_BODY()

	/** Default constructor. */
	UMovieSceneFadeSection();

public:

	/** Fade color */
	UPROPERTY(EditAnywhere, Category="Fade")
	FLinearColor FadeColor;

	/** Fade audio */
	UPROPERTY(EditAnywhere, Category="Fade")
	uint32 bFadeAudio:1;
};
