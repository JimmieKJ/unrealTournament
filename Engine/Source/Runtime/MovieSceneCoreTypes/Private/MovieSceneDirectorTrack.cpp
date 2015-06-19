// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneShotSection.h"
#include "MovieSceneDirectorTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneDirectorTrackInstance.h"

#define LOCTEXT_NAMESPACE "MovieSceneDirectorTrack"

UMovieSceneDirectorTrack::UMovieSceneDirectorTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}


FName UMovieSceneDirectorTrack::GetTrackName() const
{
	static const FName UniqueName("DirectorTrack");

	return UniqueName;
}

TSharedPtr<IMovieSceneTrackInstance> UMovieSceneDirectorTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneDirectorTrackInstance( *this ) ); 
}

TArray<UMovieSceneSection*> UMovieSceneDirectorTrack::GetAllSections() const
{
	TArray<UMovieSceneSection*> OutSectionPackets;
	for ( int32 SectionIndex = 0; SectionIndex < ShotSections.Num(); ++SectionIndex )
	{
		OutSectionPackets.Add( ShotSections[SectionIndex] );
	}
	return OutSectionPackets;
}

void UMovieSceneDirectorTrack::RemoveAllAnimationData()
{
	ShotSections.Empty();
}

bool UMovieSceneDirectorTrack::HasSection( UMovieSceneSection* Section ) const
{
	return ShotSections.Find(Section) != INDEX_NONE;
}

void UMovieSceneDirectorTrack::RemoveSection( UMovieSceneSection* Section )
{
	ShotSections.Remove( Section );
}

bool UMovieSceneDirectorTrack::IsEmpty() const
{
	return ShotSections.Num() == 0;
}

void UMovieSceneDirectorTrack::AddNewShot(FGuid CameraHandle, float Time)
{
	UMovieSceneShotSection* NewSection = NewObject<UMovieSceneShotSection>(this);
	NewSection->InitialPlacement(ShotSections, Time, Time + 1.f, SupportsMultipleRows()); //@todo find a better default end time
	NewSection->SetCameraGuid(CameraHandle);
	NewSection->SetTitle(LOCTEXT("AddNewShot", "New Shot"));

	ShotSections.Add(NewSection);
}

TRange<float> UMovieSceneDirectorTrack::GetSectionBoundaries() const
{
	TArray< TRange<float> > Bounds;
	for (int32 i = 0; i < ShotSections.Num(); ++i)
	{
		Bounds.Add(ShotSections[i]->GetRange());
	}
	return TRange<float>::Hull(Bounds);
}

#undef LOCTEXT_NAMESPACE