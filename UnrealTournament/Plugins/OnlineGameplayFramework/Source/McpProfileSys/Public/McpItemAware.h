// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemMcp.h"

#include "McpItemAware.generated.h"

class FMcpItem;

/** Interface for UObjects assigned to be created in the McpProfile's ClassMap that wish to know about the McpItem they are representing (most will) */
UINTERFACE()
class MCPPROFILESYS_API UMcpItemAware : public UInterface
{
	GENERATED_BODY()
};

class MCPPROFILESYS_API IMcpItemAware
{
	GENERATED_BODY()

public:
	/** Initialize this item from the data in the generic MCP item representation */
	virtual bool PopulateFromMcpItem(const FMcpItem& McpItem) = 0;

	/** Called when the associated McpItem is removed from the inventory */
	virtual void ProcessItemDestroy() = 0;

	/** Try to handle an attribute change by just adjusting the item. If this is not possible, return false and PopulateFromMcpItem will be called again. AttrValue may be invalid (null). */
	virtual bool ProcessAttributeChange(const FString& AttrName, const TSharedPtr<FJsonValue>& AttrValue, int64 ProfileRevision) = 0;

	/** Handle an update to just quantity */
	virtual void UpdateQuantity(int32 Quantity) = 0;

	/** Returns true if this item has local changes that may need to be sent up to the MCP */
	virtual bool HasPendingPersistentChanges() const;

	/** Marks that the persistent changes have been sent */
	virtual void MarkPersistentChangesResolved();

	/** Returns an FMcpItem instance that represents this item, for comparing against the canonical copy */
	virtual TSharedPtr<FMcpItem> CreateMcpItem() const;
};
