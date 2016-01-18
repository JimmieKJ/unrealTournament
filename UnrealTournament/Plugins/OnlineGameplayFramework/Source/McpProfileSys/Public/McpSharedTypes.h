// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "JsonObjectWrapper.h"
#include "McpQueryResult.h"

#include "McpSharedTypes.generated.h"

USTRUCT()
struct MCPPROFILESYS_API FMcpLootEntry
{
	GENERATED_USTRUCT_BODY()

	/** The PersistentName of the item asset this represents */
	UPROPERTY()
	FString ItemType;

	/** (optional) Guid for the item that was created. World items will never have this field. Also persistent items that were rolled up into another stack may be missing this field. */
	UPROPERTY()
	FString ItemGuid;

	/** The stack count for this item entry. */
	UPROPERTY()
	int32 Quantity;

	/** Any custom attributes this entry got */
	UPROPERTY()
	FJsonObjectWrapper Attributes;

	FMcpLootEntry()
		: Quantity(1)
	{
	}

	FMcpLootEntry(const FString& InItemType, int32 InQuantity = 1)
		: ItemType(InItemType)
		, Quantity(InQuantity)
	{
	}
};

USTRUCT()
struct MCPPROFILESYS_API FMcpLootResult
{
	GENERATED_USTRUCT_BODY()

	/** The loot tier group that was used to generate this loot in the form "<Name>:<TierNum>" */
	UPROPERTY()
	FString TierGroupName;

	/** Items which have already been created by MCP. Should already be in the inventory before this notification is processed. */
	UPROPERTY()
	TArray<FMcpLootEntry> PersistentItems;

	/** World items which the MCP loot roll generated. It is the dedicated server's responsibility to create these items (if desired/expected) when this loot notification is processed */
	UPROPERTY()
	TArray<FMcpLootEntry> WorldItems;
};