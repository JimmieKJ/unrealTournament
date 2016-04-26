// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"
#include "MovieSceneLevelVisibilitySection.h"
#include "ObjectKey.h"

class UMovieSceneLevelVisibilityTrack;

/**
 * Controls the runtime behavior of the movie scene level visibility track.
 */
class FMovieSceneLevelVisibilityTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	FMovieSceneLevelVisibilityTrackInstance( UMovieSceneLevelVisibilityTrack& InLevelVisibilityTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState (const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState (const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override;
	virtual void ClearInstance( IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}

private:

	void SaveLevelVisibilityState( ULevelStreaming* StreamingLevel );
	void SetEditorLevelVisibilityState( ULevelStreaming* StreamingLevel, bool bShouldBeVisible );

private:

	/** Track that is being instanced */
	UMovieSceneLevelVisibilityTrack* LevelVisibilityTrack;

	TMap<FObjectKey, bool> SavedLevelVisibility;

	TMap<FName, TWeakObjectPtr<ULevelStreaming>> NameToLevelMap;

};
