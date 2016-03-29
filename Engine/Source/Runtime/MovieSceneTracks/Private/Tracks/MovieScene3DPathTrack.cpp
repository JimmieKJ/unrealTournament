// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieScene3DPathSection.h"
#include "MovieScene3DPathTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieScene3DPathTrackInstance.h"


#define LOCTEXT_NAMESPACE "MovieScene3DPathTrack"


UMovieScene3DPathTrack::UMovieScene3DPathTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


TSharedPtr<IMovieSceneTrackInstance> UMovieScene3DPathTrack::CreateInstance()
{
	return MakeShareable( new FMovieScene3DPathTrackInstance( *this ) ); 
}


void UMovieScene3DPathTrack::AddConstraint(float KeyTime, float ConstraintEndTime, const FName SocketName, const FName ComponentName, const FGuid& ConstraintId)
{
	UMovieScene3DPathSection* NewSection = NewObject<UMovieScene3DPathSection>(this);
	{
		NewSection->AddPath(KeyTime, ConstraintEndTime, ConstraintId);
		NewSection->InitialPlacement( ConstraintSections, KeyTime, ConstraintEndTime, SupportsMultipleRows() );
	}

	ConstraintSections.Add(NewSection);
}


#if WITH_EDITORONLY_DATA
FText UMovieScene3DPathTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Path");
}
#endif


#undef LOCTEXT_NAMESPACE
