// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Instance of a UMovieSceneVectorTrack
 */
class FMovieSceneVectorTrackInstance : public IMovieSceneTrackInstance
{
public:
	FMovieSceneVectorTrackInstance( UMovieSceneVectorTrack& InVectorTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) override;
	virtual void RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) override;
private:
	/** Track that is being instanced */
	UMovieSceneVectorTrack* VectorTrack;
	/** Runtime property bindings */
	TSharedPtr<class FTrackInstancePropertyBindings> PropertyBindings;
};
