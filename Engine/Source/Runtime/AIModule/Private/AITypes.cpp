// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "AITypes.h"

//----------------------------------------------------------------------//
// FAIResourceLock
//----------------------------------------------------------------------//
FAIResourceLock::FAIResourceLock()
{
	Locks = 0;
}

void FAIResourceLock::ForceClearAllLocks()
{
	FMemory::MemZero(Locks);
}

FString FAIResourceLock::GetLockPriorityName() const
{
	const static UEnum* SourceEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EAIRequestPriority"));

	FString LockNames;

	for (int32 LockLevel = 0; LockLevel < int32(EAIRequestPriority::MAX); ++LockLevel)
	{
		if (IsLocked() && IsLockedBy(EAIRequestPriority::Type(LockLevel)))
		{
			LockNames += FString::Printf(TEXT("%s, "), *SourceEnum->GetEnumName(LockLevel));
		}
	}

	return LockNames;
}

//----------------------------------------------------------------------//
// FAIResources
//----------------------------------------------------------------------//

namespace FAIResources
{
	const FAIResourceID InvalidResource;
	const FAIResourceID Movement = FAIResourceID(TEXT("Movement"));
	const FAIResourceID Logic = FAIResourceID(TEXT("Logic"));
	const FAIResourceID Perception = FAIResourceID(TEXT("Perception"));

	TArray<FAIResourceID> ResourceIDs;

	void RegisterResource(const FAIResourceID& Resource)
	{
		if (FAIResourceID::GetSize() - FAIResources::ResourceIDs.Num() > 0)
		{
			ResourceIDs.AddZeroed(FAIResourceID::GetSize() - FAIResources::ResourceIDs.Num());
		}
		ResourceIDs[Resource.Index] = Resource;
	}

	const FAIResourceID& GetResource(int32 ResourceIndex)
	{
		return ResourceIDs.IsValidIndex(ResourceIndex) ? ResourceIDs[ResourceIndex] : InvalidResource;
	}

	int32 GetResourcesCount()
	{ 
		return ResourceIDs.Num(); 
	}

	FString GetSetDescription(FAIResourcesSet ResourceSet)
	{
		if (ResourceSet.IsEmpty() == false)
		{
			FString Description;

			for (uint8 Index = 0; Index < uint8(ResourceIDs.Num()); ++Index)
			{
				if (ResourceSet.ContainsResourceIndex(Index))
				{
					Description += ResourceIDs[Index].Name.ToString();
					Description += TEXT(", ");
				}
			}

			return Description;
		}

		return TEXT("(empty)");
	}
}

static struct FAIResourceSetup
{
	FAIResourceSetup()
	{
		FAIResources::RegisterResource(FAIResources::Movement);
		FAIResources::RegisterResource(FAIResources::Logic);
		FAIResources::RegisterResource(FAIResources::Perception);
	}
} ResourceSetup;

//----------------------------------------------------------------------//
// FAIRequestID
//----------------------------------------------------------------------//
const FAIRequestID FAIRequestID::InvalidRequest(FAIRequestID::InvalidRequestID);
const FAIRequestID FAIRequestID::AnyRequest(FAIRequestID::AnyRequestID);
// same as AnyRequest on purpose. Just a readability thing
const FAIRequestID FAIRequestID::CurrentRequest(FAIRequestID::AnyRequestID);
