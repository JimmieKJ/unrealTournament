// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieScenePrivatePCH.h"
#include "MovieSceneBinding.h"
#include "MovieSceneTrack.h"


/* FMovieSceneBinding interface
 *****************************************************************************/

TRange<float> FMovieSceneBinding::GetTimeRange() const
{
	TArray<TRange<float>> Bounds;

	for (int32 TypeIndex = 0; TypeIndex < Tracks.Num(); ++TypeIndex)
	{
		Bounds.Add(Tracks[TypeIndex]->GetSectionBoundaries());
	}

	return TRange<float>::Hull(Bounds);
}


void FMovieSceneBinding::AddTrack(UMovieSceneTrack& NewTrack)
{
	Tracks.Add( &NewTrack );
}


bool FMovieSceneBinding::RemoveTrack(UMovieSceneTrack& Track)
{
	return (Tracks.RemoveSingle(&Track) != 0);
}
