// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneVisibilityTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneVisibilitySection.h"
#include "MovieSceneVisibilityTrackInstance.h"


UMovieSceneVisibilityTrack::UMovieSceneVisibilityTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


UMovieSceneSection* UMovieSceneVisibilityTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneVisibilitySection::StaticClass(), NAME_None, RF_Transactional);
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneVisibilityTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneVisibilityTrackInstance( *this ) );
}
