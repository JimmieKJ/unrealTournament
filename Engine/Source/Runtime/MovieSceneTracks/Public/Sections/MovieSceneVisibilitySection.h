// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneBoolSection.h"
#include "MovieSceneVisibilitySection.generated.h"


/**
 * A single visibility section.
 *
 * The property that's being tracked by this section is bHiddenInGame. 
 * This custom bool track stores the inverse keys to display visibility (A green section bar means visible).
 *
 */
UCLASS( MinimalAPI )
class UMovieSceneVisibilitySection
	: public UMovieSceneBoolSection
{
	GENERATED_UCLASS_BODY()
};
