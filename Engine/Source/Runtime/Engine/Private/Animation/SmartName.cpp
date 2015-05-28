// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/SmartName.h"

FSmartNameMapping::FSmartNameMapping()
: NextUid(0)
{}

bool FSmartNameMapping::AddOrFindName(FName Name, UID& OutUid)
{
	check(Name.IsValid());

	// Check for UID overflow
	if(NextUid == MaxUID && FreeList.Num() == 0)
	{
		// No UIDs left
		UE_LOG(LogAnimation, Error, TEXT("No more UIDs left in skeleton smartname container, consider changing UID type to longer type."));
		return false;
	}
	else
	{
		const UID* ExistingUid = UidMap.FindKey(Name);

		if(ExistingUid)
		{
			// Already present in the list
			OutUid = *ExistingUid;
			return false;
		}

		if(FreeList.Num() > 0)
		{
			// We've got some UIDs that are unused < NextUid. Use them first
			OutUid = FreeList.Pop();
		}
		else
		{
			OutUid = NextUid++;
		}
		UidMap.Add(OutUid, Name);

		return true;
	}
}

bool FSmartNameMapping::GetName(const UID& Uid, FName& OutName) const
{
	const FName* FoundName = UidMap.Find(Uid);
	if(FoundName)
	{
		OutName = *FoundName;
		return true;
	}
	return false;
}

bool FSmartNameMapping::Rename(const UID& Uid, FName NewName)
{
	FName* ExistingName = UidMap.Find(Uid);
	if(ExistingName)
	{
		*ExistingName = NewName;
		return true;
	}
	return false;
}

bool FSmartNameMapping::Remove(const UID& Uid)
{
	bool bRemoved = UidMap.Remove(Uid) != 0;

	if(bRemoved)
	{
		// The provided UID has been cleared. If it is at the end of the mapping decrement next id,
		// otherwise add to the freelist
		if(Uid == NextUid - 1)
		{
			--NextUid;
		}
		else
		{
			FreeList.Add(Uid);
		}
	}

	return bRemoved;
}

void FSmartNameMapping::Serialize(FArchive& Ar)
{
	if(Ar.UE4Ver() >= VER_UE4_SKELETON_ADD_SMARTNAMES)
	{
		Ar << NextUid;
		Ar << UidMap;

		if(Ar.ArIsLoading)
		{
			// Sort the Uid Map
			UidMap.KeySort([](const UID& A, const UID& B)
			{
				return A < B;
			});

			// Build freelist as necessary
			UID ExpectedID = 0;
			UID Last = 0;
			for(TPair<UID, FName>& Pair : UidMap)
			{
				UID CurrentUid = Pair.Key;
				while(CurrentUid != ExpectedID)
				{
					FreeList.Add(ExpectedID++);
				}

				Last = CurrentUid;
				++ExpectedID;
			}

			// In the case of multipled queued deletions previously,
			// fix up the next UID to match the last used key we found.
			NextUid = Last + 1;
		}
	}
}

int32 FSmartNameMapping::GetNumNames()
{
	return UidMap.Num();
}

void FSmartNameMapping::FillUidArray(TArray<UID>& Array) const
{
	UidMap.GenerateKeyArray(Array);
}

void FSmartNameMapping::FillNameArray(TArray<FName>& Array) const
{
	UidMap.GenerateValueArray(Array);
}

bool FSmartNameMapping::Exists(const UID& Uid)
{
	return UidMap.Find(Uid) != nullptr;
}

bool FSmartNameMapping::Exists(const FName& Name)
{
	return UidMap.FindKey(Name) != nullptr;
}

const FSmartNameMapping::UID* FSmartNameMapping::FindUID(const FName& Name)
{
	return UidMap.FindKey(Name);
}

FArchive& operator<<(FArchive& Ar, FSmartNameMapping& Elem)
{
	Elem.Serialize(Ar);

	return Ar;
}

void FSmartNameContainer::AddContainer(FName NewContainerName)
{
	if(!NameMappings.Find(NewContainerName))
	{
		NameMappings.Add(NewContainerName);
	}
}

FSmartNameMapping* FSmartNameContainer::GetContainer(FName ContainerName)
{
	return NameMappings.Find(ContainerName);
}

const FSmartNameMapping* FSmartNameContainer::GetContainer(FName ContainerName) const
{
	return NameMappings.Find(ContainerName);
}

void FSmartNameContainer::Serialize(FArchive& Ar)
{
	Ar << NameMappings;
}
