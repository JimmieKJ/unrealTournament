// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSkeletalAnimationSection.h"
#include "MovieSceneSkeletalAnimationTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSkeletalAnimationTrackInstance.h"


#define LOCTEXT_NAMESPACE "MovieSceneSkeletalAnimationTrack"


/* UMovieSceneSkeletalAnimationTrack structors
 *****************************************************************************/

UMovieSceneSkeletalAnimationTrack::UMovieSceneSkeletalAnimationTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


/* UMovieSceneSkeletalAnimationTrack interface
 *****************************************************************************/

void UMovieSceneSkeletalAnimationTrack::AddNewAnimation(float KeyTime, UAnimSequence* AnimSequence)
{
	UMovieSceneSkeletalAnimationSection* NewSection = NewObject<UMovieSceneSkeletalAnimationSection>(this);
	{
		NewSection->InitialPlacement(AnimationSections, KeyTime, KeyTime + AnimSequence->SequenceLength, SupportsMultipleRows());
		NewSection->SetAnimSequence(AnimSequence);
	}

	AddSection(*NewSection);
}


UMovieSceneSection* UMovieSceneSkeletalAnimationTrack::GetAnimSectionAtTime(float Time)
{
	for (auto Section : AnimationSections)
	{
		if (Section->IsTimeWithinSection(Time))
		{
			return Section;
		}
	}

	UMovieSceneSection* NearestSection = MovieSceneHelpers::FindNearestSectionAtTime( AnimationSections, Time );
	return NearestSection;
}


/* UMovieSceneTrack interface
 *****************************************************************************/

TSharedPtr<IMovieSceneTrackInstance> UMovieSceneSkeletalAnimationTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneSkeletalAnimationTrackInstance( *this ) ); 
}


const TArray<UMovieSceneSection*>& UMovieSceneSkeletalAnimationTrack::GetAllSections() const
{
	return AnimationSections;
}


void UMovieSceneSkeletalAnimationTrack::RemoveAllAnimationData()
{
	// do nothing
}


bool UMovieSceneSkeletalAnimationTrack::HasSection(const UMovieSceneSection& Section ) const
{
	return AnimationSections.Contains(&Section);
}


void UMovieSceneSkeletalAnimationTrack::AddSection(UMovieSceneSection& Section)
{
	AnimationSections.Add(&Section);
}


void UMovieSceneSkeletalAnimationTrack::RemoveSection(UMovieSceneSection& Section)
{
	AnimationSections.Remove(&Section);
}


bool UMovieSceneSkeletalAnimationTrack::IsEmpty() const
{
	return AnimationSections.Num() == 0;
}


TRange<float> UMovieSceneSkeletalAnimationTrack::GetSectionBoundaries() const
{
	TArray<TRange<float>> Bounds;

	for (auto Section : AnimationSections)
	{
		Bounds.Add(Section->GetRange());
	}

	return TRange<float>::Hull(Bounds);
}


#if WITH_EDITORONLY_DATA

FText UMovieSceneSkeletalAnimationTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Animation");
}

#endif


#undef LOCTEXT_NAMESPACE
