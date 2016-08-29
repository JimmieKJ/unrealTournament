// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneVisibilityTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneVisibilitySection.h"
#include "MovieSceneVisibilityTrackInstance.h"


#define LOCTEXT_NAMESPACE "MovieSceneVisibilityTrack"


UMovieSceneVisibilityTrack::UMovieSceneVisibilityTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}


UMovieSceneSection* UMovieSceneVisibilityTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneVisibilitySection::StaticClass(), NAME_None, RF_Transactional);
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneVisibilityTrack::CreateInstance()
{
	return MakeShareable(new FMovieSceneVisibilityTrackInstance(*this));
}


#if WITH_EDITORONLY_DATA

FText UMovieSceneVisibilityTrack::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "Visibility");
}

#endif


#undef LOCTEXT_NAMESPACE
