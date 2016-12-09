// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "BoneContainer.h"
#include "SmartName.generated.h"

struct FSmartName;

/** in the future if we need more bools, please convert to bitfield 
 * These are not saved in asset but per skeleton. 
 */
USTRUCT()
struct ENGINE_API FAnimCurveType
{
	GENERATED_USTRUCT_BODY()

	bool bMaterial;
	bool bMorphtarget;

	FAnimCurveType(bool bInMorphtarget = false, bool bInMaterial = false)
		: bMaterial(bInMaterial)
		, bMorphtarget(bInMorphtarget)
	{
	}
};

/** Curve Meta Data for each name
 * Unfortunately this should be linked to FName, but no GUID because we don't have GUID in run-time
 * We only add this if anything changed, by default, it is attribute curve
 */
USTRUCT()
struct FCurveMetaData
{
	GENERATED_USTRUCT_BODY()

	struct FAnimCurveType Type;
	TArray<struct FBoneReference> LinkedBones;

	friend FArchive& operator<<(FArchive& Ar, FCurveMetaData& B)
	{
		Ar << B.Type.bMaterial;
		Ar << B.Type.bMorphtarget;
		Ar << B.LinkedBones;

		return Ar;
	}

	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}

	FCurveMetaData()
	{
	}
};

USTRUCT()
struct ENGINE_API FSmartNameMapping
{
	GENERATED_USTRUCT_BODY();

	FSmartNameMapping();

	// Add a name to the mapping, if it exists, get it.
	// @param Name - The name to add/get
	// @param OUT OutUid - The UID of the newly created or retrieved name
	// @param OUT OutGuid - The GUID of the newly created or retrieved name
	// @return bool - true if the name was added, false if it existed (OutUid will be correctly set still in this case)
	bool AddOrFindName(FName Name, SmartName::UID_Type& OutUid, FGuid& OutGuid);
	// Add a name to the mapping if it doesn't exist
	// @param Name - The name to add/get
	// @param OUT OutUid - The UID of the newly created or retrieved name
	// @param InGuid - the guid of the name
	// @return bool - true if the name was added, false if it existed (OutUid will be correctly set still in this case)
	bool AddName(FName Name, SmartName::UID_Type& OutUid, const FGuid& InGuid);

	// Get a name from the mapping
	// @param Uid - SmartName::UID_Type of the name to retrieve
	// @param OUT OutName - Retrieved name
	// @return bool - true if name existed and OutName is valid
	bool GetName(const SmartName::UID_Type& Uid, FName& OutName) const;
	bool GetNameByGuid(const FGuid& Guid, FName& OutName) const;
	// Fill an array with all used UIDs
	// @param Array - Array to fill
	void FillUidArray(TArray<SmartName::UID_Type>& Array) const;

	// Fill an array with all used names
	// @param Array - Array to fill
	void FillNameArray(TArray<FName>& Array) const;

	// Change a name
	// @param Uid - SmartName::UID_Type of the name to change
	// @param NewName - New name to set 
	// @return bool - true if the name was found and changed, false if the name wasn't present in the mapping
	bool Rename(const SmartName::UID_Type& Uid, FName NewName);

	// Remove a name from the mapping
	// @param Uid - SmartName::UID_Type of the name to remove
	// @return bool - true if the name was found and removed, false if the name wasn't present in the mapping
	bool Remove(const SmartName::UID_Type& Uid);

	// Return SmartName::UID_Type * if it finds it
	//  @param NewName - New name to set 
	// @return SmartName::UID_Type pointer - null if it doesn't find. pointer if it finds. 
	const SmartName::UID_Type* FindUID(const FName& Name) const;

	// Check whether a name already exists in the mapping
	// @param Uid - the SmartName::UID_Type to check
	// @return bool - whether the name was found
	bool Exists(const SmartName::UID_Type& Uid) const;

	// Check whether a name already exists in the mapping
	// @param Name - the name to check
	// @return bool - whether the name was found
	bool Exists(const FName& Name) const;

	// Get the number of names currently stored in this container
	int32 GetNumNames() const;

	// Find Or Add Smart Names
#if WITH_EDITOR
	bool FindOrAddSmartName(FName Name, FSmartName& OutName);
	bool AddSmartName(FSmartName& OutName);
#else
	// in cooked build, you don't have GUID, so register without GUID
	bool FindOrAddSmartName(FName Name, SmartName::UID_Type& OutUid);
#endif // WITH_EDITOR
	bool FindSmartName(FName Name, FSmartName& OutName) const;
	bool FindSmartNameByUID(SmartName::UID_Type UID, FSmartName& OutName) const;

	// Curve Meta Data Accessors
	FCurveMetaData* GetCurveMetaData(FName CurveName)
	{
		checkSlow(Exists(CurveName));
		return &CurveMetaDataMap.FindOrAdd(CurveName);
	}

	const FCurveMetaData* GetCurveMetaData(FName CurveName) const
	{
		checkSlow(Exists(CurveName));
		return CurveMetaDataMap.Find(CurveName);
	}

	// Serialize this to the provided archive; required for TMap serialization
	void Serialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FSmartNameMapping& Elem);

	/* initialize curve meta data for the container */
	void InitializeCurveMetaData(class USkeleton* Skeleton);
private:
	SmartName::UID_Type NextUid;				// The next SmartName::UID_Type to use 
	TMap<SmartName::UID_Type, FName> UidMap;	// Mapping of UIDs and names. This is transient and built upon serialize - this is run-time data, and searching from SmartName::UID_Type to FName is important
	TMap<FName, FGuid> GuidMap;	// Mapping of GUIDs and names. This is the one serializing
	TMap<FName, FCurveMetaData> CurveMetaDataMap;
};

USTRUCT()
struct ENGINE_API FSmartNameContainer
{
	GENERATED_USTRUCT_BODY();

	// Add a new smartname container with the provided name
	void AddContainer(FName NewContainerName);

	// Get a container by name	
	const FSmartNameMapping* GetContainer(FName ContainerName) const;

	// Serialize this to the provided archive; required for TMap serialization
	void Serialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FSmartNameContainer& Elem);

	/** Only restricted classes can access the protected interface */
	friend class USkeleton;
protected:
	FSmartNameMapping* GetContainerInternal(const FName& ContainerName);
	const FSmartNameMapping* GetContainerInternal(const FName& ContainerName) const;

	/* initialize curve meta data for the container */
	void InitializeCurveMetaData(class USkeleton* Skeleton);

private:
	TMap<FName, FSmartNameMapping> NameMappings;	// List of smartname mappings
};

USTRUCT()
struct ENGINE_API FSmartName
{

	GENERATED_USTRUCT_BODY();

	// name 
	UPROPERTY(VisibleAnywhere, Category=FSmartName)
	FName DisplayName;

	// SmartName::UID_Type - for faster access
	SmartName::UID_Type	UID;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid	Guid;
#endif // WITH_EDITORONLY_DATA

	FSmartName()
		: DisplayName(NAME_None)
		, UID(SmartName::MaxUID)
	{}

#if WITH_EDITORONLY_DATA
	FSmartName(const FName& InDisplayName, SmartName::UID_Type InUID, const FGuid& InGuid)
		: DisplayName(InDisplayName)
		, UID(InUID)
		, Guid(InGuid)
	{}
#else
	FSmartName(const FName& InDisplayName, SmartName::UID_Type InUID)
		: DisplayName(InDisplayName)
		, UID(InUID)
	{}
#endif // WITH_EDITORONLY_DATA

	bool operator==(FSmartName const& Other) const
	{
#if WITH_EDITORONLY_DATA
		return (DisplayName == Other.DisplayName && UID == Other.UID && Guid == Other.Guid);
#else
		return (DisplayName == Other.DisplayName && UID == Other.UID);
#endif // WITH_EDITORONLY_DATA
	}
	bool operator!=(const FSmartName& Other) const
	{
		return !(*this == Other);
	}
	/**
	* Serialize the SmartName
	*
	* @param Ar	Archive to serialize to
	*
	* @return True if the container was serialized
	*/
	bool Serialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FSmartName& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

	bool IsValid() const
	{
#if WITH_EDITORONLY_DATA
		return UID != SmartName::MaxUID && Guid.IsValid(); 
#else
		return UID != SmartName::MaxUID;
#endif  // WITH_EDITORONLY_DATA
	}
};

template<>
struct TStructOpsTypeTraits<FSmartName> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithSerializer = true,
		WithIdenticalViaEquality = true
	};
};
