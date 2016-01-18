// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieScenePrivatePCH.h"


DEFINE_LOG_CATEGORY(LogMovieScene);


/**
 * MovieScene module implementation.
 */
class FMovieSceneModule
	: public IMovieSceneModule
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FMovieSceneModule, MovieScene);
