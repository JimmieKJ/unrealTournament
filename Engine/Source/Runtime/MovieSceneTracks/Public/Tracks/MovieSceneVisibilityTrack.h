// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneBoolTrack.h"
#include "MovieSceneVisibilityTrack.generated.h"


class IMovieSceneTrackInstance;
class UMovieSceneSection;


/**
 * Handles manipulation of visibility properties in a movie scene
 */
UCLASS(MinimalAPI)
class UMovieSceneVisibilityTrack
	: public UMovieSceneBoolTrack
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieSceneTrack interface
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const override;
#endif
};
