// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneCapture.generated.h"

class FSceneViewport;
struct FMovieSceneCaptureSettings;
struct FMovieSceneCaptureHandle;

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UMovieSceneCaptureInterface : public UInterface
{
public:
	GENERATED_BODY()
};

/** Interface for a movie capture object */
class IMovieSceneCaptureInterface
{
public:
	GENERATED_BODY()

	/** Initialize this capture object by binding it to the specified viewport */
	virtual void Initialize(TWeakPtr<FSceneViewport> Viewport) = 0;

	/** Called after initialization to allow the movie scene capture object to override the range of frames to 
	    capture.  It's called after initialization so you can be sure that the FrameRate and other variables are
		fully initialized first. */
	virtual void SetupFrameRange() = 0;

	/** Shut down this movie capture */
	virtual void Close() = 0;

	/** Get a unique handle to this object */
	virtual FMovieSceneCaptureHandle GetHandle() const = 0;

	/** Access specific movie scene capture settings */
	virtual const FMovieSceneCaptureSettings& GetSettings() const = 0;
};