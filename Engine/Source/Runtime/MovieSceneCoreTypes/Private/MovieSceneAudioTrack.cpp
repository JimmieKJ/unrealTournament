// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneAudioSection.h"
#include "MovieSceneAudioTrack.h"
#include "IMovieScenePlayer.h"
#include "SoundDefinitions.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Runtime/Engine/Public/AudioDecompress.h"
#include "MovieSceneAudioTrackInstance.h"

#define LOCTEXT_NAMESPACE "MovieSceneDirectorTrack"

UMovieSceneAudioTrack::UMovieSceneAudioTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

FName UMovieSceneAudioTrack::GetTrackName() const
{
	return AudioTrackConstants::UniqueTrackName;
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneAudioTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneAudioTrackInstance( *this ) ); 
}

TArray<UMovieSceneSection*> UMovieSceneAudioTrack::GetAllSections() const
{
	TArray<UMovieSceneSection*> OutSections;
	for (int32 SectionIndex = 0; SectionIndex < AudioSections.Num(); ++SectionIndex)
	{
		OutSections.Add( AudioSections[SectionIndex] );
	}
	return OutSections;
}

void UMovieSceneAudioTrack::RemoveAllAnimationData()
{
}

bool UMovieSceneAudioTrack::HasSection( UMovieSceneSection* Section ) const
{
	return AudioSections.Find( Section ) != INDEX_NONE;
}

void UMovieSceneAudioTrack::RemoveSection( UMovieSceneSection* Section )
{
	AudioSections.Remove( Section );
}

bool UMovieSceneAudioTrack::IsEmpty() const
{
	return AudioSections.Num() == 0;
}

TRange<float> UMovieSceneAudioTrack::GetSectionBoundaries() const
{
	TArray< TRange<float> > Bounds;
	for (int32 i = 0; i < AudioSections.Num(); ++i)
	{
		Bounds.Add(AudioSections[i]->GetRange());
	}
	return TRange<float>::Hull(Bounds);
}

void UMovieSceneAudioTrack::AddNewSound(USoundBase* Sound, float Time)
{
	check(Sound);
	
	// determine initial duration
	// @todo Once we have infinite sections, we can remove this
	float DurationToUse = 1.f; // if all else fails, use 1 second duration

	float SoundDuration = Sound->GetDuration();
	if (SoundDuration != INDEFINITELY_LOOPING_DURATION)
	{
		DurationToUse = SoundDuration;
	}

	// add the section
	UMovieSceneAudioSection* NewSection = NewObject<UMovieSceneAudioSection>(this);
	NewSection->InitialPlacement( AudioSections, Time, Time + DurationToUse, SupportsMultipleRows() );
	NewSection->SetAudioStartTime(Time);
	NewSection->SetSound(Sound);

	AudioSections.Add(NewSection);
}

bool UMovieSceneAudioTrack::IsAMasterTrack() const
{
	return Cast<UMovieScene>(GetOuter())->IsAMasterTrack(this);
}



#undef LOCTEXT_NAMESPACE
