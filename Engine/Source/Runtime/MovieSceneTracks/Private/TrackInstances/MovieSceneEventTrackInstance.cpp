// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneEventTrack.h"
#include "MovieSceneEventTrackInstance.h"


/* FMovieSceneEventTrackInstance structors
 *****************************************************************************/

FMovieSceneEventTrackInstance::FMovieSceneEventTrackInstance(UMovieSceneEventTrack& InEventTrack)
{
	EventTrack = &InEventTrack;
}


/* IMovieSceneTrackInstance interface
 *****************************************************************************/

void FMovieSceneEventTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	EventTrack->TriggerEvents(UpdateData.Position, UpdateData.LastPosition, Player);
}
