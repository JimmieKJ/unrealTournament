// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IMovieSceneTracksModule.h"


/**
 * Implements the MovieSceneTracks module.
 */
class FMovieSceneTracksModule
	: public IMovieSceneTracksModule
{ };


IMPLEMENT_MODULE(FMovieSceneTracksModule, MovieSceneTracks);
