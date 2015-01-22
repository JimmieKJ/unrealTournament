// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Instance of a UMovieSceneTransformTrack
 */
class FMovieScene3DTransformTrackInstance : public IMovieSceneTrackInstance
{
public:
	FMovieScene3DTransformTrackInstance( UMovieScene3DTransformTrack& InTransformTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) override;
	virtual void RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) override {}
private:
	/** Track that is being instanced */
	UMovieScene3DTransformTrack* TransformTrack;
};
