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
private:
	struct FActiveSectionData
	{
		TWeakObjectPtr<UMovieSceneLevelVisibilitySection> Section;
		TMap<FName, bool> PreviousLevelVisibility;
	};

public:

	FMovieSceneLevelVisibilityTrackInstance( UMovieSceneLevelVisibilityTrack& InLevelVisibilityTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState (const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState (const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual bool RequiresUpdateForSubSceneDeactivate() { return true; }
	virtual void RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override;
	virtual void ClearInstance( IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}

private:

	void SaveLevelVisibilityState( ULevelStreaming* StreamingLevel );

private:

	/** Track that is being instanced */
	UMovieSceneLevelVisibilityTrack* LevelVisibilityTrack;

	TMap<FObjectKey, bool> SavedLevelVisibility;

	TArray<FActiveSectionData> ActiveSectionDatas;

	TMap<FName, TWeakObjectPtr<ULevelStreaming>> NameToLevelMap;

};
