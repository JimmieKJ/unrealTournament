// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/SmartName.h"
#include "FrameworkObjectVersion.h"
////////////////////////////////////////////////////////////////////////
//
// FSmartNameMapping
//
///////////////////////////////////////////////////////////////////////
FSmartNameMapping::FSmartNameMapping()
: NextUid(0)
{}

bool FSmartNameMapping::AddOrFindName(FName Name, UID& OutUid, FGuid& OutGuid)
{
	check(Name.IsValid());

	// Check for UID overflow
	const UID* ExistingUid = UidMap.FindKey(Name);
	const FGuid* ExistingGuid = GuidMap.Find(Name);

	// make sure they both exists and same 
	check(!!ExistingUid == !!ExistingGuid);
	if(ExistingUid)
	{
		// Already present in the list
		OutUid = *ExistingUid;
		OutGuid = *ExistingGuid;
		return false;
	}

	// make sure we didn't reach till end
	OutGuid = FGuid::NewGuid();
	return AddName(Name, OutUid, OutGuid);
}

bool FSmartNameMapping::AddName(FName Name, UID& OutUid, const FGuid& InGuid)
{
	check(Name.IsValid());
	if (GuidMap.Find(Name) == nullptr && GuidMap.FindKey(InGuid) == nullptr)
	{
		// make sure we didn't reach till end
		check(NextUid != MaxUID);

		OutUid = NextUid;
		UidMap.Add(OutUid, Name);
		GuidMap.Add(Name, InGuid);

		++NextUid;

		return true;
	}

	return false;
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

bool FSmartNameMapping::GetNameByGuid(const FGuid& Guid, FName& OutName) const
{
	const FName* FoundName = GuidMap.FindKey(Guid);
	if (FoundName)
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
		FGuid* Guid = GuidMap.Find(*ExistingName);
		check(Guid);

		// re add new value
		GuidMap.Remove(*ExistingName);
		GuidMap.Add(NewName, *Guid);

		check(GuidMap.Num() == UidMap.Num());

		*ExistingName = NewName;
		return true;
	}
	return false;
}

bool FSmartNameMapping::Remove(const UID& Uid)
{
	FName* ExistingName = UidMap.Find(Uid);
	if (ExistingName)
	{
		FGuid* Guid = GuidMap.Find(*ExistingName);
		check(Guid);

		// re add new value
		GuidMap.Remove(*ExistingName);
		UidMap.Remove(Uid);

		check(GuidMap.Num() == UidMap.Num());

		return true;
	}

	return false;
}

void FSmartNameMapping::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
	if (Ar.CustomVer(FFrameworkObjectVersion::GUID) >= FFrameworkObjectVersion::SmartNameRefactor)
	{
		Ar << GuidMap;

		if (Ar.ArIsLoading)
		{
			NextUid = 0;
			//fill up uidmap
			UidMap.Empty(GuidMap.Num());
			for (const TPair<FName, FGuid>& GuidPair : GuidMap)
			{
				UidMap.Add(NextUid, GuidPair.Key);
				++NextUid;
			}
		}
	}
	else if(Ar.UE4Ver() >= VER_UE4_SKELETON_ADD_SMARTNAMES)
	{
		Ar << NextUid;
		Ar << UidMap;

		TMap<UID, FName> NewUidMap;
		// convert to GuidMap
		NextUid = 0;
		GuidMap.Empty(UidMap.Num());
		for (TPair<UID, FName>& UidPair : UidMap)
		{
			GuidMap.Add(UidPair.Value, FGuid::NewGuid());
			NewUidMap.Add(NextUid++, UidPair.Value);
		}

		UidMap = NewUidMap;
	}
}

int32 FSmartNameMapping::GetNumNames() const
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

bool FSmartNameMapping::Exists(const UID& Uid) const
{
	return UidMap.Find(Uid) != nullptr;
}

bool FSmartNameMapping::Exists(const FName& Name) const
{
	return UidMap.FindKey(Name) != nullptr;
}

const FSmartNameMapping::UID* FSmartNameMapping::FindUID(const FName& Name) const
{
	return UidMap.FindKey(Name);
}

FArchive& operator<<(FArchive& Ar, FSmartNameMapping& Elem)
{
	Elem.Serialize(Ar);

	return Ar;
}

#if WITH_EDITOR
bool FSmartNameMapping::FindOrAddSmartName(FName Name, FSmartName& OutName)
{
	FSmartNameMapping::UID NewUID;
	FGuid NewGuid;
	bool bNewlyAdded = AddOrFindName(Name, NewUID, NewGuid);
	OutName = FSmartName(Name, NewUID, NewGuid);

	return bNewlyAdded;
}

bool FSmartNameMapping::AddSmartName(FSmartName& OutName)
{
	return AddName(OutName.DisplayName, OutName.UID, OutName.Guid);
}
#endif // WITH_EDITOR

bool FSmartNameMapping::FindSmartName(FName Name, FSmartName& OutName) const
{
	const FSmartNameMapping::UID* ExistingUID = FindUID(Name);
	if (ExistingUID)
	{
#if WITH_EDITOR
		const FGuid* ExistingGuid = GuidMap.Find(Name);
		OutName = FSmartName(Name, *ExistingUID, *ExistingGuid);
#else
		OutName = FSmartName(Name, *ExistingUID);
#endif // WITH_EDITOR
		return true;
	}

	return false;
}

bool FSmartNameMapping::FindSmartNameByUID(FSmartNameMapping::UID UID, FSmartName& OutName) const
{
	FName ExistingName;
	if (GetName(UID, ExistingName))
	{
		OutName.DisplayName = ExistingName;
#if WITH_EDITORONLY_DATA
		OutName.Guid = *GuidMap.Find(ExistingName);
#endif // WITH_EDITORONLY_DATA
		OutName.UID = UID;
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////
//
// FSmartNameContainer
//
///////////////////////////////////////////////////////////////////////
void FSmartNameContainer::AddContainer(FName NewContainerName)
{
	if(!NameMappings.Find(NewContainerName))
	{
		NameMappings.Add(NewContainerName);
	}
}

const FSmartNameMapping* FSmartNameContainer::GetContainer(FName ContainerName) const
{
	return NameMappings.Find(ContainerName);
}

void FSmartNameContainer::Serialize(FArchive& Ar)
{
	Ar << NameMappings;
}

FSmartNameMapping* FSmartNameContainer::GetContainerInternal(const FName& ContainerName)
{
	return NameMappings.Find(ContainerName);
}

////////////////////////////////////////////////////////////////////////
//
// FSmartName
//
///////////////////////////////////////////////////////////////////////
bool FSmartName::Serialize(FArchive& Ar)
{
	Ar << DisplayName;
	Ar << UID;

	// only save if it's editor build and not cooking
#if WITH_EDITORONLY_DATA
	if (!Ar.IsSaving() || !Ar.IsCooking())
	{
		Ar << Guid;
	}
#endif // WITH_EDITORONLY_DATA

	return true;
}