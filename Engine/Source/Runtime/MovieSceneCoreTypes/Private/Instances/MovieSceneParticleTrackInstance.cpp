// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneParticleTrackInstance.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneParticleTrack.h"
#include "MovieSceneParticleSection.h"
#include "Particles/Emitter.h"
FMovieSceneParticleTrackInstance::~FMovieSceneParticleTrackInstance()
{
}

void FMovieSceneParticleTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) 
{
	// @todo Sequencer We need something analagous to Matinee 1's particle replay tracks
	// What we have here is simple toggling/triggering

	if (Position > LastPosition && Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Playing)
	{
		bool bTrigger = false, bOn = false, bOff = false;

		const TArray<UMovieSceneSection*> Sections = ParticleTrack->GetAllParticleSections();
		for (int32 i = 0; i < Sections.Num(); ++i)
		{
			UMovieSceneParticleSection* Section = Cast<UMovieSceneParticleSection>(Sections[i]);
			if (Section->GetKeyType() == EParticleKey::Trigger)
			{
				if (Position > Section->GetStartTime() && LastPosition < Section->GetStartTime())
				{
					bTrigger = true;
				}
			}
			else if (Section->GetKeyType() == EParticleKey::Toggle)
			{
				if (Position >= Section->GetStartTime() && Position <= Section->GetEndTime())
				{
					bOn = true;
				}
				else if (Position >= Section->GetEndTime() && LastPosition < Section->GetEndTime())
				{
					bOff = true;
				}
			}
		}
		
		if (bTrigger || bOn || bOff)
		{
			for (int32 i = 0; i < RuntimeObjects.Num(); ++i)
			{
				AEmitter* Emitter = Cast<AEmitter>(RuntimeObjects[i]);
				if (Emitter)
				{
					if (bTrigger)
					{
						Emitter->ToggleActive();
					}
					else if (bOn)
					{
						Emitter->Activate();
					}
					else if (bOff)
					{
						Emitter->Deactivate();
					}
				}
			}
		}
	}
	else
	{
		for (int32 i = 0; i < RuntimeObjects.Num(); ++i)
		{
			AEmitter* Emitter = Cast<AEmitter>(RuntimeObjects[i]);
			if (Emitter)
			{
				Emitter->Deactivate();
			}
		}
	}
}
