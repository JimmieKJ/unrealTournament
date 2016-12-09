// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Animation/SmartName.h"
#include "UObject/FrameworkObjectVersion.h"
#include "Animation/Skeleton.h"

////////////////////////////////////////////////////////////////////////
//
// FSmartNameMapping
//
///////////////////////////////////////////////////////////////////////
FSmartNameMapping::FSmartNameMapping()
: NextUid(0)
{}

bool FSmartNameMapping::AddOrFindName(FName Name, SmartName::UID_Type& OutUid, FGuid& OutGuid)
{
	check(Name.IsValid());

	// Check for SmartName::UID_Type overflow
	const SmartName::UID_Type* ExistingUid = UidMap.FindKey(Name);
	const FGuid* ExistingGuid = GuidMap.Find(Name);

	// make sure they both exists and same 
	check(!!ExistingUid == !!ExistingGuid);
	if(ExistingUid && ExistingGuid->IsValid())
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

bool FSmartNameMapping::AddName(FName Name, SmartName::UID_Type& OutUid, const FGuid& InGuid)
{
	check(Name.IsValid());
	check(InGuid.IsValid());

	const SmartName::UID_Type* ExistingUid = UidMap.FindKey(Name);
	const FGuid* ExistingGuid = GuidMap.Find(Name);

	if (ExistingUid == nullptr && (ExistingGuid == nullptr || !ExistingGuid->IsValid()))
	{
		// make sure we didn't reach till end
		check(NextUid != SmartName::MaxUID);

		OutUid = NextUid;
		UidMap.Add(OutUid, Name);
		GuidMap.Add(Name, InGuid);

		++NextUid;

		return true;
	}

	return false;
}

bool FSmartNameMapping::GetName(const SmartName::UID_Type& Uid, FName& OutName) const
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
	if (Guid.IsValid())
	{
		const FName* FoundName = GuidMap.FindKey(Guid);
		if (FoundName)
		{
			OutName = *FoundName;
			return true;
		}
	}

	return false;
}

bool FSmartNameMapping::Rename(const SmartName::UID_Type& Uid, FName NewName)
{
	FName* ExistingName = UidMap.Find(Uid);
	if(ExistingName)
	{
		FGuid* Guid = GuidMap.Find(*ExistingName);
		check(Guid);

		// re add new value
		GuidMap.Remove(*ExistingName);
		GuidMap.Add(NewName, *Guid);

		// fix up meta data
		FCurveMetaData* MetaDataToCopy = CurveMetaDataMap.Find(*ExistingName);
		if (MetaDataToCopy)
		{
			FCurveMetaData& NewMetaData = CurveMetaDataMap.Add(NewName);
			NewMetaData = *MetaDataToCopy;
			
			// remove old one
			CurveMetaDataMap.Remove(*ExistingName);			
		}

		check(GuidMap.Num() == UidMap.Num());

		*ExistingName = NewName;
		return true;
	}
	return false;
}

bool FSmartNameMapping::Remove(const SmartName::UID_Type& Uid)
{
	FName* ExistingName = UidMap.Find(Uid);
	if (ExistingName)
	{
		FGuid* Guid = GuidMap.Find(*ExistingName);
		check(Guid);

		// re add new value
		GuidMap.Remove(*ExistingName);
		CurveMetaDataMap.Remove(*ExistingName);
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
		if (Ar.IsSaving() && Ar.IsCooking())
		{
			// stript out guid from the map
			for (TPair<FName, FGuid>& GuidPair: GuidMap)
			{
				// clear guid, so that we don't use it once cooked
				GuidPair.Value = FGuid();
			}
		}

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

		TMap<SmartName::UID_Type, FName> NewUidMap;
		// convert to GuidMap
		NextUid = 0;
		GuidMap.Empty(UidMap.Num());
		for (TPair<SmartName::UID_Type, FName>& UidPair : UidMap)
		{
			GuidMap.Add(UidPair.Value, FGuid::NewGuid());
			NewUidMap.Add(NextUid++, UidPair.Value);
		}

		UidMap = NewUidMap;
	}

	if (Ar.CustomVer(FFrameworkObjectVersion::GUID) >= FFrameworkObjectVersion::MoveCurveTypesToSkeleton)
	{
		Ar << CurveMetaDataMap;
	}
}

int32 FSmartNameMapping::GetNumNames() const
{
	return UidMap.Num();
}

void FSmartNameMapping::FillUidArray(TArray<SmartName::UID_Type>& Array) const
{
	UidMap.GenerateKeyArray(Array);
}

void FSmartNameMapping::FillNameArray(TArray<FName>& Array) const
{
	UidMap.GenerateValueArray(Array);
}

bool FSmartNameMapping::Exists(const SmartName::UID_Type& Uid) const
{
	return UidMap.Find(Uid) != nullptr;
}

bool FSmartNameMapping::Exists(const FName& Name) const
{
	return UidMap.FindKey(Name) != nullptr;
}

const SmartName::UID_Type* FSmartNameMapping::FindUID(const FName& Name) const
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
	SmartName::UID_Type NewUID;
	FGuid NewGuid;
	bool bNewlyAdded = AddOrFindName(Name, NewUID, NewGuid);
	OutName = FSmartName(Name, NewUID, NewGuid);

	return bNewlyAdded;
}

bool FSmartNameMapping::AddSmartName(FSmartName& OutName)
{
	return AddName(OutName.DisplayName, OutName.UID, OutName.Guid);
}

#else 

// in cooked build, we don't have Guid, so just register with empty guid. 
// you only should come here if it it hasn't found yet. 
bool FSmartNameMapping::FindOrAddSmartName(FName Name, SmartName::UID_Type& OutUid)
{
	FSmartName FoundName;
	if (FindSmartName(Name, FoundName))
	{
		OutUid = FoundName.UID;
		return true;
	}
	else
	{
		// the guid is discarded, just we want to make sure it's not same
		return AddName(Name, OutUid, FGuid::NewGuid());
	}
}
#endif // WITH_EDITOR

bool FSmartNameMapping::FindSmartName(FName Name, FSmartName& OutName) const
{
	const SmartName::UID_Type* ExistingUID = FindUID(Name);
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

bool FSmartNameMapping::FindSmartNameByUID(SmartName::UID_Type UID, FSmartName& OutName) const
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

/* initialize curve meta data for the container */
void FSmartNameMapping::InitializeCurveMetaData(class USkeleton* Skeleton)
{
	// initialize bone indices for skeleton
	for (TPair<FName, FCurveMetaData>& Iter : CurveMetaDataMap)
	{
		FCurveMetaData& CurveMetaData = Iter.Value;
		for (int32 LinkedBoneIndex = 0; LinkedBoneIndex < CurveMetaData.LinkedBones.Num(); ++LinkedBoneIndex)
		{
			CurveMetaData.LinkedBones[LinkedBoneIndex].Initialize(Skeleton);
		}
	}
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

const FSmartNameMapping* FSmartNameContainer::GetContainerInternal(const FName& ContainerName) const
{
	return NameMappings.Find(ContainerName);
}

/* initialize curve meta data for the container */
void FSmartNameContainer::InitializeCurveMetaData(class USkeleton* Skeleton)
{
	FSmartNameMapping* CurveMappingTable = GetContainerInternal(USkeleton::AnimCurveMappingName);
	if (CurveMappingTable)
	{
		CurveMappingTable->InitializeCurveMetaData(Skeleton);
	}
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
