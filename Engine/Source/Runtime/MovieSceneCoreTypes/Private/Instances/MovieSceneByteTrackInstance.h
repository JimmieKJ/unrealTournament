// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Instance of a UMovieSceneByteTrack
 */
class FMovieSceneByteTrackInstance : public IMovieSceneTrackInstance
{
public:
	FMovieSceneByteTrackInstance( UMovieSceneByteTrack& InByteTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) override;
	virtual void RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) override;
private:
	/** Byte track that is being instanced */
	UMovieSceneByteTrack* ByteTrack;
	/** Runtime property bindings */
	TSharedPtr<class FTrackInstancePropertyBindings> PropertyBindings;
};
