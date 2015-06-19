// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneAnimationSection.h"
#include "MovieSceneAnimationTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneAnimationTrackInstance.h"

#define LOCTEXT_NAMESPACE "MovieSceneAnimationTrack"

UMovieSceneAnimationTrack::UMovieSceneAnimationTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

FName UMovieSceneAnimationTrack::GetTrackName() const
{
	return FName("Animation");
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneAnimationTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneAnimationTrackInstance( *this ) ); 
}

TArray<UMovieSceneSection*> UMovieSceneAnimationTrack::GetAllSections() const
{
	TArray<UMovieSceneSection*> OutSections;
	for (int32 SectionIndex = 0; SectionIndex < AnimationSections.Num(); ++SectionIndex)
	{
		OutSections.Add( AnimationSections[SectionIndex] );
	}
	return OutSections;
}

void UMovieSceneAnimationTrack::RemoveAllAnimationData()
{
}

bool UMovieSceneAnimationTrack::HasSection( UMovieSceneSection* Section ) const
{
	return AnimationSections.Find( Section ) != INDEX_NONE;
}

void UMovieSceneAnimationTrack::RemoveSection( UMovieSceneSection* Section )
{
	AnimationSections.Remove( Section );
}

bool UMovieSceneAnimationTrack::IsEmpty() const
{
	return AnimationSections.Num() == 0;
}

TRange<float> UMovieSceneAnimationTrack::GetSectionBoundaries() const
{
	TArray< TRange<float> > Bounds;
	for (int32 i = 0; i < AnimationSections.Num(); ++i)
	{
		Bounds.Add(AnimationSections[i]->GetRange());
	}
	return TRange<float>::Hull(Bounds);
}

void UMovieSceneAnimationTrack::AddNewAnimation(float KeyTime, UAnimSequence* AnimSequence)
{
	// add the section
	UMovieSceneAnimationSection* NewSection = NewObject<UMovieSceneAnimationSection>(this);
	NewSection->InitialPlacement( AnimationSections, KeyTime, KeyTime + AnimSequence->SequenceLength, SupportsMultipleRows() );
	NewSection->SetAnimationStartTime( KeyTime );
	NewSection->SetAnimSequence(AnimSequence);

	AnimationSections.Add(NewSection);
}

UMovieSceneSection* UMovieSceneAnimationTrack::GetAnimSectionAtTime(float Time)
{
	for (int32 i = 0; i < AnimationSections.Num(); ++i)
	{
		UMovieSceneSection* Section = AnimationSections[i];
		if( Section->IsTimeWithinSection( Time ) )
		{
			return Section;
		}
	}

	return NULL;
}


#undef LOCTEXT_NAMESPACE
