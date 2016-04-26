// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

void ULevelSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context)
{
	if (Context)
	{
		ObjectReferences.CreateBinding(ObjectId, &PossessedObject, Context);
	}
}

void ULevelSequence::BindPossessableObject(const FGuid& ObjectId, const FLevelSequenceObjectReference& ObjectReference)
{
	ObjectReferences.CreateBinding(ObjectId, ObjectReference);
}

bool ULevelSequence::CanPossessObject(UObject& Object) const
{
	return Object.IsA<AActor>() || Object.IsA<UActorComponent>();
}

UObject* ULevelSequence::FindPossessableObject(const FGuid& ObjectId, UObject* Context) const
{
	return Context ? ObjectReferences.ResolveBinding(ObjectId, Context) : nullptr;
}

FGuid ULevelSequence::FindPossessableObjectId(UObject& Object) const
{
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

void ULevelSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
	ObjectReferences.RemoveBinding(ObjectId);
}
