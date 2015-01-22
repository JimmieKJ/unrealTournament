// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCorePrivatePCH.h"
#include "ModuleManager.h"

#include "MovieScene.h"
#include "MovieSceneBindings.h"
#include "RuntimeMovieScenePlayer.h"

DEFINE_LOG_CATEGORY(LogSequencerRuntime);


/**
 * MovieSceneCore module implementation (private)
 */
class FMovieSceneCoreModule : public IMovieSceneCore
{

public:

	/** IModuleInterface */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};


IMPLEMENT_MODULE( FMovieSceneCoreModule, MovieSceneCore );



void FMovieSceneCoreModule::StartupModule()
{
}


void FMovieSceneCoreModule::ShutdownModule()
{
}