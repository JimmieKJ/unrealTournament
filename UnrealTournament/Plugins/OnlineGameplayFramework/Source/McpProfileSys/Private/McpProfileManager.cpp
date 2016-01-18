// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "McpProfileSysPCH.h"
#include "Online.h"
#include "McpProfileManager.h"
#include "McpProfileGroup.h"

UMcpProfileManager::UMcpProfileManager(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

UMcpProfileGroup* UMcpProfileManager::CreateProfileGroup(const FUniqueNetIdWrapper& AccountId, bool bAsServer, bool bReplaceExisting)
{
	// see if there is an existing group for this account
	UMcpProfileGroup* ExistingGroup = GetProfileGroup(AccountId, bAsServer);
	if (ExistingGroup != nullptr)
	{
		if (!bReplaceExisting)
		{
			// just return what we found
			return ExistingGroup;
		}

		// destroy any existing group for this account ID
		DestroyProfileGroup(ExistingGroup);
	}

	// get that outer object
	UObject* OuterObj = GetOuter();

	// make a new one
	FProfileGroupEntry Entry;
	Entry.AccountId = AccountId;
	Entry.ProfileGroup = NewObject<UMcpProfileGroup>(OuterObj);

#if WITH_ENGINE
	// Get the Mcp online subsystem for this world
	FOnlineSubsystemMcp* OnlineSubMcp = (FOnlineSubsystemMcp*)Online::GetSubsystem(OuterObj ? OuterObj->GetWorld() : nullptr, MCP_SUBSYSTEM); // this may be NULL in nomcp mode
#else
	FOnlineSubsystemMcp* OnlineSubMcp = (FOnlineSubsystemMcp*)IOnlineSubsystem::Get(MCP_SUBSYSTEM); // this may be NULL in nomcp mode
#endif

	// initialize it
	Entry.ProfileGroup->Initialize(OnlineSubMcp, bAsServer, AccountId);

	// add to our list
	TArray<FProfileGroupEntry>& ProfileGroupList = Entry.ProfileGroup->IsActingAsServer() ? ServerProfileGroups : ClientProfileGroups;
	ProfileGroupList.Add(Entry);
	return Entry.ProfileGroup;
}

UMcpProfileGroup* UMcpProfileManager::GetProfileGroup(const FUniqueNetIdWrapper& AccountId, bool bAsServer) const
{
	const TArray<FProfileGroupEntry>& ProfileGroupList = bAsServer ? ServerProfileGroups : ClientProfileGroups;
	for (const FProfileGroupEntry& Entry : ProfileGroupList)
	{
		if (Entry.AccountId == AccountId)
		{
			return Entry.ProfileGroup;
		}
	}
	return nullptr;
}

void UMcpProfileManager::DestroyProfileGroup(UMcpProfileGroup* ProfileGroup)
{
	if (ProfileGroup)
	{
		TArray<FProfileGroupEntry>& ProfileGroupList = ProfileGroup->IsActingAsServer() ? ServerProfileGroups : ClientProfileGroups;

		// remove our reference to this profile group (there should be only one)
		for (int32 i = 0; i < ProfileGroupList.Num(); ++i)
		{
			if (ProfileGroupList[i].ProfileGroup == ProfileGroup)
			{
				ProfileGroupList.RemoveAtSwap(i);
				return;
			}
		}
	}
}