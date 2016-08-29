// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneParticleTrackInstance.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneParticleTrack.h"
#include "MovieSceneParticleSection.h"
#include "Particles/Emitter.h"


FMovieSceneParticleTrackInstance::~FMovieSceneParticleTrackInstance()
{
}

static UParticleSystemComponent* ParticleSystemComponentFromRuntimeObject(UObject* Object)
{
	if(AEmitter* Emitter = Cast<AEmitter>(Object))
	{
		return Emitter->GetParticleSystemComponent();
	}
	else
	{
		return Cast<UParticleSystemComponent>(Object);
	}
}

void FMovieSceneParticleTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
	// @todo Sequencer We need something analagous to Matinee 1's particle replay tracks
	// What we have here is simple toggling/triggering

	if (UpdateData.Position >= UpdateData.LastPosition && Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Playing)
	{
		const TArray<UMovieSceneSection*> Sections = ParticleTrack->GetAllParticleSections();
		EParticleKey::Type ParticleKey = EParticleKey::Deactivate;
		bool bKeyFound = false;

		for (int32 i = 0; i < Sections.Num(); ++i)
		{
			UMovieSceneParticleSection* Section = Cast<UMovieSceneParticleSection>( Sections[i] );
			if (Section->IsActive())
			{
				FIntegralCurve& ParticleKeyCurve = Section->GetParticleCurve();
				FKeyHandle PreviousHandle = ParticleKeyCurve.FindKeyBeforeOrAt( UpdateData.Position );
				if ( ParticleKeyCurve.IsKeyHandleValid( PreviousHandle ) )
				{
					FIntegralKey& PreviousKey = ParticleKeyCurve.GetKey( PreviousHandle );
					if ( PreviousKey.Time >= UpdateData.LastPosition )
					{
						ParticleKey = (EParticleKey::Type)PreviousKey.Value;
						bKeyFound = true;
					}
				}
			}
		}
		
		if ( bKeyFound )
		{
			for (int32 i = 0; i < RuntimeObjects.Num(); ++i)
			{
				UParticleSystemComponent* ParticleSystemComponent = ParticleSystemComponentFromRuntimeObject(RuntimeObjects[i].Get());

				if ( ParticleSystemComponent != nullptr )
				{
					if ( ParticleKey == EParticleKey::Activate)
					{
						if ( ParticleSystemComponent->IsActive() )
						{
							ParticleSystemComponent->SetActive(false, true);
						}
						ParticleSystemComponent->SetActive(true, true);
					}
					else if( ParticleKey == EParticleKey::Deactivate )
					{
						ParticleSystemComponent->SetActive(false, true);
					}
					else if ( ParticleKey == EParticleKey::Trigger )
					{
						ParticleSystemComponent->ActivateSystem(true);
					}
				}
			}
		}
	}
	else
	{
		for (int32 i = 0; i < RuntimeObjects.Num(); ++i)
		{
			UObject* Object = RuntimeObjects[i].Get();
			AEmitter* Emitter = Cast<AEmitter>(Object);

			if (Emitter != nullptr)
			{
				Emitter->Deactivate();
			}
			else if (UParticleSystemComponent* Component =  Cast<UParticleSystemComponent>(Object))
			{
				Component->SetActive(false, true);
			}
		}
	}
}
