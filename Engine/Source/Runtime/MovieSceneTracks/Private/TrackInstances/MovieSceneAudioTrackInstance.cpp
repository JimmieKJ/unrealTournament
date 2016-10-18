// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneAudioTrackInstance.h"
#include "IMovieScenePlayer.h"
#include "SoundDefinitions.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Runtime/Engine/Public/AudioDecompress.h"
#include "MovieSceneAudioTrack.h"
#include "MovieSceneAudioSection.h"
#include "AudioThread.h"


FMovieSceneAudioTrackInstance::FMovieSceneAudioTrackInstance( UMovieSceneAudioTrack& InAudioTrack )
{
	AudioTrack = &InAudioTrack;
}

void FMovieSceneAudioTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
}

void FMovieSceneAudioTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	StopAllSounds();
}

void FMovieSceneAudioTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
	// If the playback status is jumping, ie. one such occurrence is setting the time for thumbnail generation, disable audio updates
	if (Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Jumping)
	{
		return;
	}

	const TArray<UMovieSceneSection*>& AudioSections = AudioTrack->GetAudioSections();

	TArray<AActor*> Actors = GetRuntimeActors(RuntimeObjects);

	if (Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Playing)
	{
		if (UpdateData.Position >= UpdateData.LastPosition)
		{
			TMap<int32, TArray<UMovieSceneAudioSection*> > AudioSectionsBySectionIndex;
			for (int32 i = 0; i < AudioSections.Num(); ++i)
			{
				UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(AudioSections[i]);
				if (AudioSection->IsActive())
				{
					int32 SectionIndex = AudioSection->GetRowIndex();
					AudioSectionsBySectionIndex.FindOrAdd(SectionIndex).Add(AudioSection);
				}
			}

			for (TMap<int32, TArray<UMovieSceneAudioSection*> >::TIterator It(AudioSectionsBySectionIndex); It; ++It)
			{
				int32 RowIndex = It.Key();
				TArray<UMovieSceneAudioSection*>& MovieSceneAudioSections = It.Value();

				for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex)
				{
					TWeakObjectPtr<UAudioComponent> Component = GetAudioComponent(Player, Actors[ActorIndex], RowIndex);

					bool bComponentIsPlaying = false;
					if (Component.IsValid())
					{
						for (int32 i = 0; i < MovieSceneAudioSections.Num(); ++i)
						{
							UMovieSceneAudioSection* AudioSection = MovieSceneAudioSections[i];
							if (AudioSection->IsTimeWithinAudioRange(UpdateData.Position))
							{
								if (!AudioSection->IsTimeWithinAudioRange(UpdateData.LastPosition) || !Component->IsPlaying())
								{
									PlaySound(AudioSection, Component, UpdateData.Position);
								}
								bComponentIsPlaying = true;
							}
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
		if (!FMath::IsNearlyEqual(UpdateData.Position, UpdateData.LastPosition))
		{
			for (int32 i = 0; i < AudioSections.Num(); ++i)
			{
				auto AudioSection = Cast<UMovieSceneAudioSection>(AudioSections[i]);

				if (!AudioSection->IsActive())
				{
					continue;
				}

				int32 RowIndex = AudioSection->GetRowIndex();
				
				for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex)
				{
					TWeakObjectPtr<UAudioComponent> Component = GetAudioComponent(Player, Actors[ActorIndex], RowIndex);

					if (!Component.IsValid())
					{
						continue;
					}

					if (AudioSection->IsTimeWithinAudioRange(UpdateData.Position) && !Component->IsPlaying())
					{
						PlaySound(AudioSection, Component, UpdateData.Position);
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
				UAudioComponent* Component = GetAudioComponent(Player, Actors[ActorIndex], RowIndex).Get();
				if (Component && Component->IsPlaying())
				{
					if (FAudioDevice* AudioDevice = Component->GetAudioDevice())
					{
						DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.MovieSceneUpdateAudioTransform"), STAT_MovieSceneUpdateAudioTransform, STATGROUP_TaskGraphTasks);

						const FTransform ActorTransform = Actors[ActorIndex]->GetTransform();
						const uint64 ActorComponentID = Component->GetUniqueID();
						FAudioThread::RunCommandOnAudioThread([AudioDevice, ActorComponentID, ActorTransform]()
						{
							if (FActiveSound* ActiveSound = AudioDevice->FindActiveSound(ActorComponentID))
							{
								ActiveSound->bLocationDefined = true;
								ActiveSound->Transform = ActorTransform;
							}
						}, GET_STATID(STAT_MovieSceneUpdateAudioTransform));
					}
				}
			}
		}
	}
}


void FMovieSceneAudioTrackInstance::PlaySound(UMovieSceneAudioSection* AudioSection, TWeakObjectPtr<UAudioComponent> Component, float Time)
{
	if (!Component.IsValid())
	{
		return;
	}

	float PitchMultiplier = 1.f / AudioSection->GetAudioDilationFactor();
	
	Component->bAllowSpatialization = !AudioTrack->IsAMasterTrack();
	Component->Stop();
	Component->SetSound(AudioSection->GetSound());
	Component->SetVolumeMultiplier(AudioSection->GetAudioVolume());
	Component->SetPitchMultiplier(PitchMultiplier);
	Component->bIsUISound = true;
	Component->Play(Time - AudioSection->GetAudioStartTime());
}


void FMovieSceneAudioTrackInstance::StopSound(int32 RowIndex)
{
	if (RowIndex >= PlaybackAudioComponents.Num())
	{
		return;
	}

	TMap<AActor*, TWeakObjectPtr<UAudioComponent>>& AudioComponents = PlaybackAudioComponents[RowIndex];
	for (TMap<AActor*, TWeakObjectPtr<UAudioComponent>>::TIterator It(AudioComponents); It; ++It)
	{
		if (It.Value().IsValid())
		{
			It.Value()->Stop();
		}
	}
}


void FMovieSceneAudioTrackInstance::StopAllSounds()
{
	for (int32 i = 0; i < PlaybackAudioComponents.Num(); ++i)
	{
		TMap<AActor*, TWeakObjectPtr<UAudioComponent>>& AudioComponents = PlaybackAudioComponents[i];
		for (TMap<AActor*, TWeakObjectPtr<UAudioComponent>>::TIterator It(AudioComponents); It; ++It)
		{
			if (It.Value().IsValid())
			{
				It.Value()->Stop();
			}
		}
	}
}


TWeakObjectPtr<UAudioComponent> FMovieSceneAudioTrackInstance::GetAudioComponent(IMovieScenePlayer& Player, AActor* Actor, int32 RowIndex)
{
	if (RowIndex + 1 > PlaybackAudioComponents.Num())
	{
		while (PlaybackAudioComponents.Num() < RowIndex + 1)
		{
			TMap<AActor*, TWeakObjectPtr<UAudioComponent>> AudioComponentMap;
			PlaybackAudioComponents.Add(AudioComponentMap);
		}
	}

	if (PlaybackAudioComponents[RowIndex].Find(Actor) == nullptr || !(*PlaybackAudioComponents[RowIndex].Find(Actor)).IsValid())
	{
		USoundCue* TempPlaybackAudioCue = NewObject<USoundCue>();
		UWorld* World = Actor ? Actor->GetWorld() : (Player.GetPlaybackContext() != nullptr) ? Player.GetPlaybackContext()->GetWorld() : nullptr;
		UAudioComponent* AudioComponent = FAudioDevice::CreateComponent(TempPlaybackAudioCue, World, Actor, false, false);

		PlaybackAudioComponents[RowIndex].Add(Actor);
		PlaybackAudioComponents[RowIndex][Actor] = TWeakObjectPtr<UAudioComponent>(AudioComponent);
	}

	return *PlaybackAudioComponents[RowIndex].Find(Actor);
}


TArray<AActor*> FMovieSceneAudioTrackInstance::GetRuntimeActors(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects) const
{
	TArray<AActor*> Actors;

	for (int32 ObjectIndex = 0; ObjectIndex < RuntimeObjects.Num(); ++ObjectIndex)
	{
		UObject* Object = RuntimeObjects[ObjectIndex].Get();

		if (Object->IsA<AActor>())
		{
			Actors.Add(Cast<AActor>(Object));
		}
	}

	if (AudioTrack->IsAMasterTrack())
	{
		check(Actors.Num() == 0);
		Actors.Add(nullptr);
	}

	return Actors;
}
