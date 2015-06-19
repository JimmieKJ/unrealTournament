// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneAudioTrackInstance.h"
#include "IMovieScenePlayer.h"
#include "SoundDefinitions.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Runtime/Engine/Public/AudioDecompress.h"
#include "MovieSceneAudioTrack.h"

FMovieSceneAudioTrackInstance::FMovieSceneAudioTrackInstance( UMovieSceneAudioTrack& InAudioTrack )
{
	AudioTrack = &InAudioTrack;
}

void FMovieSceneAudioTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) 
{
	const TArray<UMovieSceneSection*>& AudioSections = AudioTrack->GetAudioSections();

	TArray<AActor*> Actors = GetRuntimeActors(RuntimeObjects);

	if (Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Playing)
	{
		if (Position > LastPosition)
		{
			TMap<int32, TArray<UMovieSceneAudioSection*> > AudioSectionsBySectionIndex;
			for (int32 i = 0; i < AudioSections.Num(); ++i)
			{
				UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(AudioSections[i]);
				int32 SectionIndex = AudioSection->GetRowIndex();
				AudioSectionsBySectionIndex.FindOrAdd(SectionIndex).Add(AudioSection);
			}

			for (TMap<int32, TArray<UMovieSceneAudioSection*> >::TIterator It(AudioSectionsBySectionIndex); It; ++It)
			{
				int32 RowIndex = It.Key();
				TArray<UMovieSceneAudioSection*>& MovieSceneAudioSections = It.Value();

				for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex)
				{
					UAudioComponent* Component = GetAudioComponent(Actors[ActorIndex], RowIndex);

					bool bComponentIsPlaying = false;
					for (int32 i = 0; i < MovieSceneAudioSections.Num(); ++i)
					{
						UMovieSceneAudioSection* AudioSection = MovieSceneAudioSections[i];
						if (AudioSection->IsTimeWithinAudioRange(Position))
						{
							if (!AudioSection->IsTimeWithinAudioRange(LastPosition) || !Component->IsPlaying())
							{
								PlaySound(AudioSection, Component, Position);
							}
							bComponentIsPlaying = true;
						}
					}

					if (!bComponentIsPlaying)
					{
						StopSound(RowIndex);
					}
				}
			}
		}
		else
		{
			StopAllSounds();
		}
	}
	else if (Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Scrubbing)
	{
		// handle scrubbing
		if (!FMath::IsNearlyEqual(Position, LastPosition))
		{
			for (int32 i = 0; i < AudioSections.Num(); ++i)
			{
				auto AudioSection = Cast<UMovieSceneAudioSection>(AudioSections[i]);
				int32 RowIndex = AudioSection->GetRowIndex();
				
				for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex)
				{
					UAudioComponent* Component = GetAudioComponent(Actors[ActorIndex], RowIndex);

					if (AudioSection->IsTimeWithinAudioRange(Position) && !Component->IsPlaying())
					{
						PlaySound(AudioSection, Component, Position);
						// Fade out the sound at the same volume in order to simply
						// set a short duration on the sound, far from ideal soln
						Component->FadeOut(AudioTrackConstants::ScrubDuration, 1.f);
					}
				}
			}
		}
	}
	else
	{
		// beginning scrubbing, stopped, recording
		StopAllSounds();
	}

	// handle locality of non-master audio
	if (!AudioTrack->IsAMasterTrack())
	{
		for (int32 RowIndex = 0; RowIndex < PlaybackAudioComponents.Num(); ++RowIndex)
		{
			for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex)
			{
				UAudioComponent* Component = GetAudioComponent(Actors[ActorIndex], RowIndex);
				if (Component->IsPlaying())
				{
					FAudioDevice* AudioDevice = Component->GetAudioDevice();
					FActiveSound* ActiveSound = AudioDevice->FindActiveSound(Component);
					ActiveSound->bLocationDefined = true;
					ActiveSound->Transform = Actors[ActorIndex]->GetTransform();
				}
			}
		}
	}
}

void FMovieSceneAudioTrackInstance::PlaySound(UMovieSceneAudioSection* AudioSection, UAudioComponent* Component, float Time)
{
	float PitchMultiplier = 1.f / AudioSection->GetAudioDilationFactor();
	Component->bAllowSpatialization = !AudioTrack->IsAMasterTrack();
	Component->Stop();
	Component->SetSound(AudioSection->GetSound());
	Component->SetVolumeMultiplier(1.f);
	Component->SetPitchMultiplier(PitchMultiplier);
	Component->Play(Time - AudioSection->GetAudioStartTime());
}

void FMovieSceneAudioTrackInstance::StopSound(int32 RowIndex)
{
	if (RowIndex >= PlaybackAudioComponents.Num()) {return;}

	TMap<AActor*, UAudioComponent*>& AudioComponents = PlaybackAudioComponents[RowIndex];
	for (TMap<AActor*, UAudioComponent*>::TIterator It(AudioComponents); It; ++It)
	{
		It.Value()->Stop();
	}
}

void FMovieSceneAudioTrackInstance::StopAllSounds()
{
	for (int32 i = 0; i < PlaybackAudioComponents.Num(); ++i)
	{
		TMap<AActor*, UAudioComponent*>& AudioComponents = PlaybackAudioComponents[i];
		for (TMap<AActor*, UAudioComponent*>::TIterator It(AudioComponents); It; ++It)
		{
			It.Value()->Stop();
		}
	}
}

UAudioComponent* FMovieSceneAudioTrackInstance::GetAudioComponent(AActor* Actor, int32 RowIndex)
{
	if (RowIndex + 1 > PlaybackAudioComponents.Num())
	{
		while (PlaybackAudioComponents.Num() < RowIndex + 1)
		{
			TMap<AActor*, UAudioComponent*> AudioComponentMap;
			PlaybackAudioComponents.Add(AudioComponentMap);
		}
	}

	UAudioComponent*& AudioComponent = PlaybackAudioComponents[RowIndex].FindOrAdd(Actor);
	if (AudioComponent == NULL)
	{
		USoundCue* TempPlaybackAudioCue = NewObject<USoundCue>();
		
		AudioComponent = FAudioDevice::CreateComponent(TempPlaybackAudioCue, NULL, Actor, false, false);
	}
	return AudioComponent;
}

TArray<AActor*> FMovieSceneAudioTrackInstance::GetRuntimeActors(const TArray<UObject*>& RuntimeObjects) const
{
	TArray<AActor*> Actors;
	for (int32 ObjectIndex = 0; ObjectIndex < RuntimeObjects.Num(); ++ObjectIndex)
	{
		if (RuntimeObjects[ObjectIndex]->IsA<AActor>())
		{
			Actors.Add(Cast<AActor>(RuntimeObjects[ObjectIndex]));
		}
	}
	if (AudioTrack->IsAMasterTrack())
	{
		check(Actors.Num() == 0);
		Actors.Add(NULL);
	}
	return Actors;
}
