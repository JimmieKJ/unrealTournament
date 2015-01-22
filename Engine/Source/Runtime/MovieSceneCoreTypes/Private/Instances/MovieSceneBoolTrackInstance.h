// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Instance of a UMovieSceneBoolTrack
 */
class FMovieSceneBoolTrackInstance : public IMovieSceneTrackInstance
{
public:
	FMovieSceneBoolTrackInstance( UMovieSceneBoolTrack& InBoolTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) override;
	virtual void RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) override;
private:
	/** Bool track that is being instanced */
	UMovieSceneBoolTrack* BoolTrack;
	/** Runtime property bindings */
	TSharedPtr<class FTrackInstancePropertyBindings> PropertyBindings;
};
