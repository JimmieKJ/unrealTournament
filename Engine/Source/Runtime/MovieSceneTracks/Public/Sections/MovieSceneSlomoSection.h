// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneFloatSection.h"
#include "MovieSceneSlomoSection.generated.h"


/**
 * A single floating point section.
 */
UCLASS(MinimalAPI)
class UMovieSceneSlomoSection
	: public UMovieSceneFloatSection
{
	GENERATED_BODY()

	/** Default constructor. */
	UMovieSceneSlomoSection();
};
