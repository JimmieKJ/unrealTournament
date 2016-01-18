// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneFloatTrack.h"
#include "MovieSceneFadeTrack.generated.h"


/**
 * Implements a movie scene track that controls a fade.
 */
UCLASS(MinimalAPI)
class UMovieSceneFadeTrack
	: public UMovieSceneFloatTrack
{
	GENERATED_BODY()

public:

	// UMovieSceneTrack interface

	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
	virtual UMovieSceneSection* CreateNewSection() override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const override;
#endif
};
