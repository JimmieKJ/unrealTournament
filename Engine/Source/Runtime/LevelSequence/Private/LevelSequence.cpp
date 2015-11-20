// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePCH.h"
#include "LevelSequence.h"
#include "LevelSequenceObject.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "Engine/Blueprint.h"


ULevelSequence::ULevelSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MovieScene(nullptr)
{
}

void ULevelSequence::Initialize()
{
	// @todo sequencer: gmp: fix me
	MovieScene = NewObject<UMovieScene>(this, NAME_None, RF_Transactional);
}

bool ULevelSequence::Rename(const TCHAR* NewName, UObject* NewOuter, ERenameFlags Flags)
{
#if WITH_EDITOR
	if (Super::Rename(NewName, NewOuter, Flags))
	{
		ForEachObjectWithOuter(MovieScene, [&](UObject* Object){
			if (auto* Blueprint = Cast<UBlueprint>(Object))
			{
				Blueprint->RenameGeneratedClasses(nullptr, MovieScene, Flags);
			}
		}, false);

		return true;
	}
#endif	//#if WITH_EDITOR
	
	return false;
}

void ULevelSequence::ConvertPersistentBindingsToDefault(UObject* FixupContext)
{
	if (PossessedObjects_DEPRECATED.Num() == 0)
	{
		return;
	}

	MarkPackageDirty();
	for (auto& Pair : PossessedObjects_DEPRECATED)
	{
		UObject* Object = Pair.Value.GetObject();
		if (Object)
		{
			FGuid ObjectId;
			FGuid::Parse(Pair.Key, ObjectId);
			ObjectReferences.CreateBinding(ObjectId, Object, FixupContext);
		}
	}
	PossessedObjects_DEPRECATED.Empty();
}

void ULevelSequence::BindToContext(UObject* NewContext)
{
	ResolutionContext = NewContext;
	CachedObjectBindings.Reset();
}

void ULevelSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject)
{
	
	UObject* Context = ResolutionContext.Get();

	// Always use the parent as a context when there is a parent spawnable
	UObject* ParentObject = GetParentObject(&PossessedObject);
	FGuid ParentId = ParentObject ? FindObjectId(*ParentObject) : FGuid();

	FMovieSceneSpawnable* ParentSpawnable = ParentId.IsValid() ? MovieScene->FindSpawnable(ParentId) : nullptr;
	if (ParentSpawnable)
	{
		Context = ParentObject;
	}

	if (Context)
	{
		ObjectReferences.CreateBinding(ObjectId, &PossessedObject, Context);

		MovieSceneHelpers::SetRuntimeObjectMobility(&PossessedObject);

		// Ensure that we report possessables to their parent spawnable, if necessary
		FMovieScenePossessable* ChildPossessable = MovieScene->FindPossessable(ObjectId);

		if (ParentSpawnable && ensureMsgf(ChildPossessable, TEXT("Object being possessed without a corresponding possessable")))
		{
			ParentSpawnable->AddChildPossessable(ObjectId);
			ChildPossessable->SetParentSpawnable(ParentId);
		}
	}
}

bool ULevelSequence::CanPossessObject(UObject& Object) const
{
	return Object.IsA<AActor>() || Object.IsA<UActorComponent>();
}

UObject* ULevelSequence::FindObject(const FGuid& ObjectId) const
{
	// If it's already cached, we can just return that
	if (auto* WeakCachedObject = CachedObjectBindings.Find(ObjectId))
	{
		if (auto* CachedObj = WeakCachedObject->Get())
		{
			return CachedObj;
		}
	}

	// search spawned objects
	if (auto* SpawnedObject = SpawnedObjects.FindRef(ObjectId).Get())
	{
		return SpawnedObject;
	}

	UObject* Context = ResolutionContext.Get();

	// Always use the parent as a context when possible
	FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectId);
	if (Possessable && Possessable->GetParentSpawnable().IsValid())
	{
		Context = FindObject(Possessable->GetParentSpawnable());
	}

	// Attempt to resolve the object binding through the remapped bindings
	UObject* FoundObject = Context ? ObjectReferences.ResolveBinding(ObjectId, Context) : nullptr;

	if (FoundObject)
	{
		// Cache the object if we found one
		CachedObjectBindings.Add(ObjectId, FoundObject);
	}

	return FoundObject;
}

FGuid ULevelSequence::FindObjectId(UObject& Object) const
{
	for (auto& Pair : CachedObjectBindings)
	{
		if (Pair.Value == &Object)
		{
			return Pair.Key;
		}
	}

	// search spawned objects
	for (auto& Pair : SpawnedObjects)
	{
		if (Pair.Value.Get() == &Object)
		{
			return Pair.Key;
		}
	}

	// search possessed objects
	if (UObject* Context = ResolutionContext.Get())
	{
		// Search instances
		FGuid ObjectId = ObjectReferences.FindBindingId(&Object, Context);
		if (ObjectId.IsValid())
		{
			return ObjectId;
		}
	}

	// not found
	return FGuid();
}

UMovieScene* ULevelSequence::GetMovieScene() const
{
	return MovieScene;
}

UObject* ULevelSequence::GetParentObject(UObject* Object) const
{
	UActorComponent* Component = Cast<UActorComponent>(Object);

	if (Component != nullptr)
	{
		return Component->GetOwner();
	}

	return nullptr;
}

bool ULevelSequence::AllowsSpawnableObjects() const
{
	return true;
}

UObject* ULevelSequence::SpawnObject(const FGuid& ObjectId)
{
	// Return an existing object relating to this object ID, if possible
	UObject* ObjectPtr = SpawnedObjects.FindRef(ObjectId).Get();
	if (ObjectPtr)
	{
		return ObjectPtr;
	}

	// Find the spawnable definition
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectId);
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
	
	SpawnedObjects.Add(ObjectId, SpawnedObject);
	CachedObjectBindings.Add(ObjectId, SpawnedObject);

	MovieSceneHelpers::SetRuntimeObjectMobility(SpawnedObject);

	return SpawnedObject;
}

void ULevelSequence::DestroySpawnedObject(const FGuid& ObjectId)
{
	if (UObject* ObjectPtr = SpawnedObjects.FindRef(ObjectId).Get())
	{
		AActor* Actor = Cast<AActor>(ObjectPtr);
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

	CachedObjectBindings.Remove(ObjectId);
	SpawnedObjects.Remove(ObjectId);
}

void ULevelSequence::DestroyAllSpawnedObjects()
{
	// Use a key array to ensure we don't change the map while iterating it
	TArray<FGuid> Keys;
	Keys.Reserve(SpawnedObjects.Num());
	SpawnedObjects.GenerateKeyArray(Keys);

	for (auto& ObjectId : Keys)
	{
		DestroySpawnedObject(ObjectId);
	}
	SpawnedObjects.Reset();
}

#if WITH_EDITOR
bool ULevelSequence::TryGetObjectDisplayName(const FGuid& ObjectId, FText& OutDisplayName) const
{
	UObject* Object = FindObject(ObjectId);

	if (Object == nullptr)
	{
		return false;
	}

	AActor* Actor = Cast<AActor>(Object);

	if (Actor == nullptr)
	{
		return false;
	}

	OutDisplayName = FText::FromString(Actor->GetActorLabel());

	return true;
}

#endif

void ULevelSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
	ObjectReferences.RemoveBinding(ObjectId);
	CachedObjectBindings.Remove(ObjectId);
}
