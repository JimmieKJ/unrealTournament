// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "McpProfileSysPCH.h"
#include "McpItemAware.h"

bool IMcpItemAware::HasPendingPersistentChanges() const
{
	return false;
}

void IMcpItemAware::MarkPersistentChangesResolved()
{
}

TSharedPtr<FMcpItem> IMcpItemAware::CreateMcpItem() const
{
	return nullptr;
}
