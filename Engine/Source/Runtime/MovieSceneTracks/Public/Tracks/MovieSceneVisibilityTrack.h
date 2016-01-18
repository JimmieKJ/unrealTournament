// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieScenePropertyTrack.h"
#include "MovieSceneBoolTrack.h"
#include "MovieSceneVisibilityTrack.generated.h"

/**
 * Handles manipulation of visibility properties in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieSceneVisibilityTrack : public UMovieSceneBoolTrack
{
	GENERATED_UCLASS_BODY()

public:
	/** UMovieSceneTrack interface */
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
};
