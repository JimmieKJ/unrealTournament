// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "IMovieSceneCapture.h"
#include "MovieSceneCaptureHandle.h"

class FMovieSceneCaptureProtocolRegistry;

class IMovieSceneCaptureModule : public IModuleInterface
{
public:
	static IMovieSceneCaptureModule& Get()
	{
		static const FName ModuleName(TEXT("MovieSceneCapture"));
		return FModuleManager::LoadModuleChecked< IMovieSceneCaptureModule >(ModuleName);
	}

	virtual IMovieSceneCaptureInterface* InitializeFromCommandLine() = 0;
	virtual IMovieSceneCaptureInterface* CreateMovieSceneCapture(TSharedPtr<FSceneViewport> Viewport) = 0;
	virtual IMovieSceneCaptureInterface* GetFirstActiveMovieSceneCapture() = 0;
	
	virtual IMovieSceneCaptureInterface* RetrieveMovieSceneInterface(FMovieSceneCaptureHandle Handle) = 0;
	virtual void DestroyMovieSceneCapture(FMovieSceneCaptureHandle Handle) = 0;

	virtual void DestroyAllActiveCaptures() = 0;
	virtual FMovieSceneCaptureProtocolRegistry& GetProtocolRegistry() = 0;
};

