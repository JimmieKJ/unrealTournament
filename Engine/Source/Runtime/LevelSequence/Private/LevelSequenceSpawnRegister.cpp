// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePCH.h"
#include "LevelSequenceSpawnRegister.h"
#include "MovieSceneSequenceInstance.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "Particles/ParticleSystem.h"
#include "IMovieScenePlayer.h"

static const FName SequencerActorTag(TEXT("SequencerActor"));

UObject* FLevelSequenceSpawnRegister::SpawnObject(const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance, IMovieScenePlayer& Player)
{
	FMovieSceneSpawnRegisterKey Key(BindingId, SequenceInstance);

	FSpawnedObject* Existing = Register.Find(Key);
	UObject* ObjectInstance = Existing ? Existing->Object.Get() : nullptr;
	if (ObjectInstance)
	{
		return ObjectInstance;
	}

	// Find the spawnable definition
	FMovieSceneSpawnable* Spawnable = SequenceInstance.GetSequence()->GetMovieScene()->FindSpawnable(BindingId);
	if (!Spawnable)
	{
		return nullptr;
	}

	AActor* ObjectTemplate = Cast<AActor>(Spawnable->GetObjectTemplate());
	if (!ObjectTemplate)
	{
		return nullptr;
	}

	// @todo sequencer: We should probably spawn these in a specific sub-level!
	// World->CurrentLevel = ???;

	const FName ActorName = NAME_None;

	const EObjectFlags ObjectFlags = RF_Transient;

	// @todo sequencer livecapture: Consider using SetPlayInEditorWorld() and RestoreEditorWorld() here instead
	
	// @todo sequencer actors: We need to make sure puppet objects aren't copied into PIE/SIE sessions!  They should be omitted from that duplication!

	// Spawn the puppet actor
	FActorSpawnParameters SpawnInfo;
	{
		SpawnInfo.Name = ActorName;
		SpawnInfo.ObjectFlags = ObjectFlags;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// @todo: Spawning with a non-CDO template is fraught with issues
		//SpawnInfo.Template = ObjectTemplate;
	}

	FTransform SpawnTransform;

	if (USceneComponent* RootComponent = ObjectTemplate->GetRootComponent())
	{
		SpawnTransform.SetTranslation(RootComponent->RelativeLocation);
		SpawnTransform.SetRotation(RootComponent->RelativeRotation.Quaternion());
	}

	{
		// Disable all particle components so that they don't auto fire as soon as the actor is spawned. The particles should be triggered through the particle track.
		TArray<UActorComponent*> ParticleComponents = ObjectTemplate->GetComponentsByClass(UParticleSystemComponent::StaticClass());
		for (int32 ComponentIdx = 0; ComponentIdx < ParticleComponents.Num(); ++ComponentIdx)
		{
			ParticleComponents[ComponentIdx]->bAutoActivate = false;
		}
	}

	UWorld* WorldContext = Cast<UWorld>(Player.GetPlaybackContext());
	if(WorldContext == nullptr)
	{
		WorldContext = GWorld;
	}

	AActor* SpawnedActor = WorldContext->SpawnActorAbsolute(ObjectTemplate->GetClass(), SpawnTransform, SpawnInfo);
	if (!SpawnedActor)
	{
		return nullptr;
	}
	
	UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
	CopyParams.bNotifyObjectReplacement = false;
	SpawnedActor->UnregisterAllComponents();
	UEngine::CopyPropertiesForUnrelatedObjects(ObjectTemplate, SpawnedActor, CopyParams);
	SpawnedActor->RegisterAllComponents();

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Explicitly set RF_Transactional on spawned actors so we can undo/redo properties on them. We don't add this as a spawn flag since we don't want to transact spawn/destroy events.
		SpawnedActor->SetFlags(RF_Transactional);

		for (UActorComponent* Component : TInlineComponentArray<UActorComponent*>(SpawnedActor))
		{
			Component->SetFlags(RF_Transactional);
		}
	}
#endif

	// tag this actor so we know it was spawned by sequencer
	SpawnedActor->Tags.Add(SequencerActorTag);

	Register.Add(Key, FSpawnedObject(BindingId, *SpawnedActor, Spawnable->GetSpawnOwnership()));

	SequenceInstance.OnObjectSpawned(BindingId, *SpawnedActor, Player);
	return SpawnedActor;
}

void FLevelSequenceSpawnRegister::DestroySpawnedObject(const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance, IMovieScenePlayer& Player)
{
	FMovieSceneSpawnRegisterKey Key(BindingId, SequenceInstance);
	
	FSpawnedObject* Existing = Register.Find(Key);
	UObject* SpawnedObject = Existing ? Existing->Object.Get() : nullptr;
	if (SpawnedObject)
	{
		PreDestroyObject(*SpawnedObject, BindingId, SequenceInstance);
		DestroySpawnedObject(*SpawnedObject);
	}

	SequenceInstance.OnSpawnedObjectDestroyed(BindingId, Player);
	Register.Remove(Key);
}

void FLevelSequenceSpawnRegister::PreDestroyObject(UObject& Object, const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance)
{
}

void FLevelSequenceSpawnRegister::DestroySpawnedObject(UObject& Object)
{
	AActor* Actor = Cast<AActor>(&Object);
	if (!ensure(Actor))
	{
		return;
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Explicitly remove RF_Transactional on spawned actors since we don't want to trasact spawn/destroy events
		Actor->ClearFlags(RF_Transactional);
		for (UActorComponent* Component : TInlineComponentArray<UActorComponent*>(Actor))
		{
			Component->ClearFlags(RF_Transactional);
		}
	}
#endif

	UWorld* World = Actor->GetWorld();
	if (ensure(World))
	{
		const bool bNetForce = false;
		const bool bShouldModifyLevel = false;
		World->DestroyActor(Actor, bNetForce, bShouldModifyLevel);
	}
}

void FLevelSequenceSpawnRegister::ForgetExternallyOwnedSpawnedObjects(IMovieScenePlayer& Player)
{
	for (auto It = Register.CreateIterator(); It; ++It)
	{
		if (It.Value().Ownership == ESpawnOwnership::External)
		{
			TSharedPtr<FMovieSceneSequenceInstance> SequenceInstance = It.Key().SequenceInstance.Pin();
			if (SequenceInstance.IsValid())
			{
				SequenceInstance->OnSpawnedObjectDestroyed(It.Key().BindingId, Player);
			}
			It.RemoveCurrent();
		}
	}
}

void FLevelSequenceSpawnRegister::DestroyObjectsByPredicate(IMovieScenePlayer& Player, const TFunctionRef<bool(const FGuid&, ESpawnOwnership, FMovieSceneSequenceInstance&)>& Predicate)
{
	for (auto It = Register.CreateIterator(); It; ++It)
	{
		FMovieSceneSequenceInstance* ThisInstance = It.Key().SequenceInstance.Pin().Get();
		if (!ThisInstance || Predicate(It.Value().Guid, It.Value().Ownership, *ThisInstance))
		{
			UObject* SpawnedObject = It.Value().Object.Get();
			if (SpawnedObject)
			{
				if (ThisInstance)
				{
					PreDestroyObject(*SpawnedObject, It.Key().BindingId, *ThisInstance);
				}
				DestroySpawnedObject(*SpawnedObject);
			}

			if (ThisInstance)
			{
				ThisInstance->OnSpawnedObjectDestroyed(It.Key().BindingId, Player);
			}
			It.RemoveCurrent();
		}
	}
}

void FLevelSequenceSpawnRegister::PreUpdateSequenceInstance(FMovieSceneSequenceInstance& Instance, IMovieScenePlayer& Player)
{
	++CurrentlyUpdatingSequenceCount;
	ActiveInstances.Add(Instance.AsShared());
}

void FLevelSequenceSpawnRegister::PostUpdateSequenceInstance(FMovieSceneSequenceInstance& InInstance, IMovieScenePlayer& Player)
{
	if (--CurrentlyUpdatingSequenceCount == 0)
	{
		for (TWeakPtr<FMovieSceneSequenceInstance>& WeakInstance : PreviouslyActiveInstances)
		{
			TSharedPtr<FMovieSceneSequenceInstance> Instance = WeakInstance.Pin();
			if (Instance.IsValid() && !ActiveInstances.Contains(Instance))
			{
				OnSequenceExpired(*Instance, Player);
			}
		}

		Swap(ActiveInstances, PreviouslyActiveInstances);
		ActiveInstances.Reset();
	}
}