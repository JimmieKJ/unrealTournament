// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"

/**
 * Instance of a UMovieScene2DTransformTrack
 */
class FMovieScene2DTransformTrackInstance : public IMovieSceneTrackInstance
{
public:
	FMovieScene2DTransformTrackInstance( class UMovieScene2DTransformTrack& InTransformTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) override;
	virtual void RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) override;
private:
	/** Track that is being instanced */
	UMovieScene2DTransformTrack* TransformTrack;
	/** Runtime property bindings */
	TSharedPtr<class FTrackInstancePropertyBindings> PropertyBindings;
};
