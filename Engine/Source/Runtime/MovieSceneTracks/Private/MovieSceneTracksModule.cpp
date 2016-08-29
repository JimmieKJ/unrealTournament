// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "IMovieSceneTracksModule.h"


/**
 * Implements the MovieSceneTracks module.
 */
class FMovieSceneTracksModule
	: public IMovieSceneTracksModule
{ };


IMPLEMENT_MODULE(FMovieSceneTracksModule, MovieSceneTracks);
