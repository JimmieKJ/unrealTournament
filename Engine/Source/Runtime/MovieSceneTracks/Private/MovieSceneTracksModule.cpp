// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "ModuleManager.h"


/**
 * Implements the MovieSceneTracks module.
 */
class FMovieSceneTracksModule
	: public IMovieSceneTracksModule
{ };


IMPLEMENT_MODULE(FMovieSceneTracksModule, MovieSceneTracks);
