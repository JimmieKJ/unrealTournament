// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartName.generated.h"

USTRUCT()
struct ENGINE_API FSmartNameMapping
{
	GENERATED_USTRUCT_BODY();

	FSmartNameMapping();

	// ID type, should be used to access names as fundamental type may change.
	// NOTE: If changing this, also change the type of NextUid below. UHT ignores,
	// this typedef and needs to know the fundamental type to parse properties.
	// If this changes and UHT handles typedefs, set NextUid type to UID.
	typedef uint16 UID;
	// Max UID used for overflow checking
	static const UID MaxUID = MAX_uint16;

	// Add a name to the mapping, if it exists, get it.
	// @param Name - The name to add/get
	// @param OUT OutUid - The UID of the newly created or retrieved name
	// @return bool - true if the name was added, false if it existed (OutUid will be correctly set still in this case)
	bool AddOrFindName(FName Name, UID& OutUid);

	// Get a name from the mapping
	// @param Uid - UID of the name to retrieve
	// @param OUT OutName - Retrieved name
	// @return bool - true if name existed and OutName is valid
	bool GetName(const UID& Uid, FName& OutName) const;

	// Fill an array with all used UIDs
	// @param Array - Array to fill
	void FillUidArray(TArray<UID>& Array) const;

	// Fill an array with all used names
	// @param Array - Array to fill
	void FillNameArray(TArray<FName>& Array) const;

	// Change a name
	// @param Uid - UID of the name to change
	// @param NewName - New name to set 
	// @return bool - true if the name was found and changed, false if the name wasn't present in the mapping
	bool Rename(const UID& Uid, FName NewName);

	// Remove a name from the mapping
	// @param Uid - UID of the name to remove
	// @return bool - true if the name was found and removed, false if the name wasn't present in the mapping
	bool Remove(const UID& Uid);

	// Return UID * if it finds it
	//  @param NewName - New name to set 
	// @return UID pointer - null if it doesn't find. pointer if it finds. 
	const FSmartNameMapping::UID* FindUID(const FName& Name);

	// Check whether a name already exists in the mapping
	// @param Uid - the UID to check
	// @return bool - whether the name was found
	bool Exists(const UID& Uid);

	// Check whether a name already exists in the mapping
	// @param Name - the name to check
	// @return bool - whether the name was found
	bool Exists(const FName& Name);

	// Get the number of names currently stored in this container
	int32 GetNumNames();

	// Serialize this to the provided archive; required for TMap serialization
	void Serialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FSmartNameMapping& Elem);

private:

	UID NextUid;				// The next UID to use 
	TMap<UID, FName> UidMap;	// Mapping of UIDs and names
	TArray<UID> FreeList;		// Freelist of UIDs from removing/moving names around. Populated on load
};

USTRUCT()
struct ENGINE_API FSmartNameContainer
{
	GENERATED_USTRUCT_BODY();

	// Add a new smartname container with the provided name
	void AddContainer(FName NewContainerName);

	// Get a container by name
	FSmartNameMapping* GetContainer(FName ContainerName);
	const FSmartNameMapping* GetContainer(FName ContainerName) const;

	// Serialize this to the provided archive; required for TMap serialization
	void Serialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FSmartNameContainer& Elem);

private:
	TMap<FName, FSmartNameMapping> NameMappings;	// List of smartname mappings
};