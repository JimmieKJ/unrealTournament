// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePCH.h"
#include "LevelSequenceObjectReference.h"

FLevelSequenceObjectReference::FLevelSequenceObjectReference(UObject* InObject, UObject* InContext)
{
	if (InObject->IsA<AActor>() || InObject->IsA<UActorComponent>())
	{
		if (InContext->IsA<AActor>())
		{
			// When the context is an actor, we only use path name lookup within the actor since it's assumed the actor will be a spawnable
			// As such, a persistent identifier for a dynamically spawned actor is not tenable
			ObjectPath = InObject->GetPathName(InContext);
			ObjectId = FUniqueObjectGuid();
		}
		else
		{
			ObjectId = FLazyObjectPtr(InObject).GetUniqueID();
			ObjectPath = InObject->GetPathName(InContext);
		}
	}
}

FLevelSequenceObjectReference::FLevelSequenceObjectReference(const FUniqueObjectGuid& InObjectId, const FString& InObjectPath)
	: ObjectId(InObjectId)
	, ObjectPath(InObjectPath)
{
}

UObject* FLevelSequenceObjectReference::Resolve(UObject* InContext) const
{
	if (ObjectId.IsValid() && InContext != nullptr)
	{
	#if WITH_EDITOR
		int32 PIEInstanceID = InContext->GetOutermost()->PIEInstanceID;
		FUniqueObjectGuid FixedUpId = PIEInstanceID == -1 ? ObjectId : ObjectId.FixupForPIE(PIEInstanceID);
	#else
		FUniqueObjectGuid FixedUpId = ObjectId;
	#endif
		

		FLazyObjectPtr LazyPtr;
		LazyPtr = FixedUpId;

		if (UObject* FoundObject = LazyPtr.Get())
		{
			return FoundObject;
		}
	}

	if (!ObjectPath.IsEmpty())
	{
		return FindObject<UObject>(InContext, *ObjectPath, false);
	}

	return nullptr;
}

bool FLevelSequenceObjectReferenceMap::Serialize(FArchive& Ar)
{
	int32 Num = Map.Num();
	Ar << Num;

	if (Ar.IsLoading())
	{
		while(Num-- > 0)
		{
			FGuid Key;
			Ar << Key;

			FLevelSequenceObjectReference Value;
			Ar << Value;

			Map.Add(Key, Value);
		}
	}
	else if (Ar.IsSaving() || Ar.IsCountingMemory() || Ar.IsObjectReferenceCollector())
	{
		for (auto& Pair : Map)
		{
			Ar << Pair.Key;
			Ar << Pair.Value;
		}
	}
	return true;
}

bool FLevelSequenceObjectReferenceMap::HasBinding(const FGuid& ObjectId) const
{
	if (const FLevelSequenceObjectReference* Reference = Map.Find(ObjectId))
	{
		return Reference->IsValid();
	}
	return false;
}

void FLevelSequenceObjectReferenceMap::CreateBinding(const FGuid& ObjectId, UObject* InObject, UObject* InContext)
{
	FLevelSequenceObjectReference NewReference(InObject, InContext);

	if (ensureMsgf(NewReference.IsValid(), TEXT("Unable to generate a reference for the specified object and context")))
	{
		Map.FindOrAdd(ObjectId) = NewReference;
	}
}

void FLevelSequenceObjectReferenceMap::CreateBinding(const FGuid& ObjectId, const FLevelSequenceObjectReference& ObjectReference)
{
	if (ensureMsgf(ObjectReference.IsValid(), TEXT("Invalid object reference specifed for binding")))
	{
		Map.FindOrAdd(ObjectId) = ObjectReference;
	}
}

void FLevelSequenceObjectReferenceMap::RemoveBinding(const FGuid& ObjectId)
{
	Map.Remove(ObjectId);
}

UObject* FLevelSequenceObjectReferenceMap::ResolveBinding(const FGuid& ObjectId, UObject* InContext) const
{
	const FLevelSequenceObjectReference* Reference = Map.Find(ObjectId);
	UObject* ResolvedObject = Reference ? Reference->Resolve(InContext) : nullptr;
	if (ResolvedObject != nullptr)
	{
		// if the resolved object does not have a valid world (e.g. world is being torn down), dont resolve
		return ResolvedObject->GetWorld() != nullptr ? ResolvedObject : nullptr;
	}
	return nullptr;
}

FGuid FLevelSequenceObjectReferenceMap::FindBindingId(UObject* InObject, UObject* InContext) const
{
	for (const auto& Pair : Map)
	{
		if (Pair.Value.Resolve(InContext) == InObject)
		{
			return Pair.Key;
		}
	}
	return FGuid();
}