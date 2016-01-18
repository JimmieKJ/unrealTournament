// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePCH.h"
#include "LevelSequenceSpawnRegister.h"
#include "MovieSceneSequenceInstance.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"

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

	UClass* SpawnableClass = Spawnable->GetClass();
	if (!SpawnableClass || !SpawnableClass->IsChildOf(AActor::StaticClass()))
	{
		return nullptr;
	}

	// @todo sequencer: We should probably spawn these in a specific sub-level!
	// World->CurrentLevel = ???;

	const FName ActorName = NAME_None;

	// Override the object flags so that RF_Transactional is not set.  Puppet actors are never transactional
	// @todo sequencer: These actors need to avoid any transaction history.  However, RF_Transactional can currently be set on objects on the fly!
	// NOTE: We are omitting RF_Transactional intentionally
	const EObjectFlags ObjectFlags = RF_Transient;

	// @todo sequencer livecapture: Consider using SetPlayInEditorWorld() and RestoreEditorWorld() here instead
	
	// @todo sequencer actors: We need to make sure puppet objects aren't copied into PIE/SIE sessions!  They should be omitted from that duplication!

	// Spawn the puppet actor
	FActorSpawnParameters SpawnInfo;
	{
		SpawnInfo.Name = ActorName;
		SpawnInfo.ObjectFlags = ObjectFlags;
	}

	FTransform SpawnTransform;

	AActor* ActorCDO = CastChecked<AActor>(SpawnableClass->ClassDefaultObject);
	if (USceneComponent* RootComponent = ActorCDO->GetRootComponent())
	{
		SpawnTransform.SetTranslation(RootComponent->RelativeLocation);
		SpawnTransform.SetRotation(RootComponent->RelativeRotation.Quaternion());
	}

	//@todo: This is in place of SpawnActorAbsolute which doesn't exist yet in the Orion branch
	FTransform NewTransform = SpawnTransform;
	{
		AActor* Template = SpawnInfo.Template;

		if(!Template)
		{
			// Use class's default actor as a template.
			Template = SpawnableClass->GetDefaultObject<AActor>();
		}

		USceneComponent* TemplateRootComponent = (Template)? Template->GetRootComponent() : NULL;
		if(TemplateRootComponent)
		{
			TemplateRootComponent->UpdateComponentToWorld();
			NewTransform = TemplateRootComponent->GetComponentToWorld().Inverse() * NewTransform;
		}
	}

	UObject* SpawnedObject = GWorld->SpawnActor(SpawnableClass, &NewTransform, SpawnInfo);
	if (!SpawnedObject)
	{
		return nullptr;
	}

	MovieSceneHelpers::SetRuntimeObjectMobility(SpawnedObject);

	Register.Add(Key, FSpawnedObject(*SpawnedObject, Spawnable->GetSpawnOwnership()));

	SequenceInstance.OnObjectSpawned(BindingId, *SpawnedObject, Player);
	return SpawnedObject;
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

void FLevelSequenceSpawnRegister::DestroyObjects(IMovieScenePlayer& Player, TFunctionRef<bool(FMovieSceneSequenceInstance&, FSpawnedObject&)> Pred)
{
	for (auto It = Register.CreateIterator(); It; ++It)
	{
		FMovieSceneSequenceInstance* ThisInstance = It.Key().SequenceInstance.Pin().Get();
		if (!ThisInstance || Pred(*ThisInstance, It.Value()))
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

void FLevelSequenceSpawnRegister::DestroyObjectsOwnedByInstance(FMovieSceneSequenceInstance& SequenceInstance, IMovieScenePlayer& Player)
{
	DestroyObjects(Player, [&](FMovieSceneSequenceInstance& ThisInstance, FSpawnedObject& SpawnedObject){
		if (SpawnedObject.Ownership == ESpawnOwnership::InnerSequence)
		{
			return &ThisInstance == &SequenceInstance;
		}
		return false;
	});
}

void FLevelSequenceSpawnRegister::DestroyObjectsSpawnedByInstance(FMovieSceneSequenceInstance& SequenceInstance, IMovieScenePlayer& Player)
{
	DestroyObjects(Player, [&](FMovieSceneSequenceInstance& ThisInstance, FSpawnedObject& SpawnedObject){
		return &ThisInstance == &SequenceInstance;
	});
}

void FLevelSequenceSpawnRegister::DestroyAllOwnedObjects(IMovieScenePlayer& Player)
{
	DestroyObjects(Player, [&](FMovieSceneSequenceInstance&, FSpawnedObject& SpawnedObject){
		return SpawnedObject.Ownership != ESpawnOwnership::External;
	});
}

void FLevelSequenceSpawnRegister::DestroyAllObjects(IMovieScenePlayer& Player)
{
	DestroyObjects(Player, [&](FMovieSceneSequenceInstance&, FSpawnedObject&){
		return true;
	});
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
				DestroyObjectsOwnedByInstance(*Instance, Player);
			}
		}

		Swap(ActiveInstances, PreviouslyActiveInstances);
		ActiveInstances.Reset();
	}
}