// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

void FMovieSceneEventTrackInstance::Update(float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass)
{
	EventTrack->TriggerEvents(Position, LastPosition);
}
