// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneAudioSection.h"
#include "MovieSceneAudioTrack.h"
#include "IMovieScenePlayer.h"
#include "SoundDefinitions.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundCue.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Runtime/Engine/Public/AudioDecompress.h"
#include "MovieSceneAudioTrackInstance.h"


#define LOCTEXT_NAMESPACE "MovieSceneAudioTrack"


UMovieSceneAudioTrack::UMovieSceneAudioTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(93, 95, 136);
#endif
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneAudioTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneAudioTrackInstance( *this ) ); 
}


const TArray<UMovieSceneSection*>& UMovieSceneAudioTrack::GetAllSections() const
{
	return AudioSections;
}


void UMovieSceneAudioTrack::RemoveAllAnimationData()
{
	// do nothing
}


bool UMovieSceneAudioTrack::HasSection(const UMovieSceneSection& Section) const
{
	return AudioSections.Contains(&Section);
}


void UMovieSceneAudioTrack::AddSection(UMovieSceneSection& Section)
{
	AudioSections.Add(&Section);
}


void UMovieSceneAudioTrack::RemoveSection(UMovieSceneSection& Section)
{
	AudioSections.Remove(&Section);
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

float GetSoundDuration(USoundBase* Sound)
{
	USoundWave* SoundWave = nullptr;

	if (Sound->IsA<USoundWave>())
	{
		SoundWave = Cast<USoundWave>(Sound);
	}
	else if (Sound->IsA<USoundCue>())
	{
#if WITH_EDITORONLY_DATA
		USoundCue* SoundCue = Cast<USoundCue>(Sound);

		// @todo Sequencer - Right now for sound cues, we just use the first sound wave in the cue
		// In the future, it would be better to properly generate the sound cue's data after forcing determinism
		const TArray<USoundNode*>& AllNodes = SoundCue->AllNodes;
		for (int32 i = 0; i < AllNodes.Num() && SoundWave == nullptr; ++i)
		{
			if (AllNodes[i]->IsA<USoundNodeWavePlayer>())
			{
				SoundWave = Cast<USoundNodeWavePlayer>(AllNodes[i])->GetSoundWave();
			}
		}
#endif
	}

	const float Duration = (SoundWave ? SoundWave->GetDuration() : 0.f);
	return Duration == INDEFINITELY_LOOPING_DURATION ? SoundWave->Duration : Duration;
}

void UMovieSceneAudioTrack::AddNewSound(USoundBase* Sound, float Time)
{
	check(Sound);
	
	// determine initial duration
	// @todo Once we have infinite sections, we can remove this
	float DurationToUse = 1.f; // if all else fails, use 1 second duration

	float SoundDuration = GetSoundDuration(Sound);
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
	return Cast<UMovieScene>(GetOuter())->IsAMasterTrack(*this);
}


bool UMovieSceneAudioTrack::SupportsMultipleRows() const
{
	return true;
}


#undef LOCTEXT_NAMESPACE
