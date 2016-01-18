// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UniqueNetIdWrapper.h"
#include "McpProfileManager.generated.h"

class UMcpProfileGroup;

/** Just a struct to map AccountId to the UMcpProfileGroup object */
USTRUCT()
struct FProfileGroupEntry
{
	GENERATED_USTRUCT_BODY()
public:
	FUniqueNetIdWrapper AccountId;
	UPROPERTY()
	UMcpProfileGroup* ProfileGroup;
};

/**
 * This class controls access to the client profile representation for a given account.
 * It assumes that this classes outer will have GetWorld implemented so we can get the 
 * correct Online SubSystem (normally this would be the GameInstance).
 */
UCLASS()
class MCPPROFILESYS_API UMcpProfileManager : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	/**
	 * Get a ProfileGroup that has already been created. If no profile exists for this account, none
	 * will be created and nullptr is returned.
	 *
	 * @param AccountId The MCP account ID this profile group will abstract
	 * @param bAsServer Do we want a server interface to this profile or a client interface
	 * @returns A pointer to the profile group (should never be nullptr)
	 */
	UMcpProfileGroup* GetProfileGroup(const FUniqueNetIdWrapper& AccountId, bool bAsServer) const;

	/**
	 * Create a profile group if it doesn't already exist. If one exists for this AccountId, this is
	 * equivalent to destroying it and creating a new one
	 * 
	 * @param AccountId The MCP account ID this profile group will abstract
	 * @param bAsServer Do we want a server interface to this profile or a client interface
	 * @param bReplaceExisting If we already have a profile group for this AccountId should we replace it or return it.
	 * @returns A pointer to the profile group (should never be nullptr)
	 */
	virtual UMcpProfileGroup* CreateProfileGroup(const FUniqueNetIdWrapper& AccountId, bool bAsServer, bool bReplaceExisting);
	
	/**
	 * Remove our reference to this profile group. Does not actually destroy the object (GC will take
	 * care of that once nothing else is referencing it. Ideally we should be the last one though.
	 *
	 * @param ProfileGroup A pointer to the UMcpProfileGroup we want to destroy
	 */
	virtual void DestroyProfileGroup(UMcpProfileGroup* ProfileGroup);

private:
	
	UPROPERTY()
	TArray<FProfileGroupEntry> ServerProfileGroups;

	UPROPERTY()
	TArray<FProfileGroupEntry> ClientProfileGroups;
};