// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"


class UMovieSceneStringTrack;


/**
 * Instance of a UMovieSceneStringTrack
 */
class FMovieSceneStringTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InStringTrack The string track to create an instance for.
	 */
	FMovieSceneStringTrackInstance(UMovieSceneStringTrack& InStringTrack );

	/** Virtual destructor. */
	virtual ~FMovieSceneStringTrackInstance() { }

public:

	//~ IMovieSceneTrackInstance interface

	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { }
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;

private:

	/** Map from object to initial state. */
	TMap<TWeakObjectPtr<UObject>, FString> InitStringMap;

	/** Runtime property bindings */
	TSharedPtr<FTrackInstancePropertyBindings> PropertyBindings;

	/** Track that is being instanced */
	UMovieSceneStringTrack* StringTrack;
};
