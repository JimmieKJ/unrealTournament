// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIGraphSchema.h"
#include "EdGraphSchema_EnvironmentQuery.generated.h"

UCLASS(MinimalAPI)
class UEdGraphSchema_EnvironmentQuery : public UAIGraphSchema
{
	GENERATED_UCLASS_BODY()

	// Begin EdGraphSchema interface
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual const FPinConnectionResponse CanMergeNodes(const UEdGraphNode* A, const UEdGraphNode* B) const override;
	virtual int32 GetNodeSelectionCount(const UEdGraph* Graph) const override;
	// End EdGraphSchema interface

	// Begin UAIGraphSchema interface
	virtual void GetSubNodeClasses(int32 SubNodeFlags, TArray<FGraphNodeClassData>& ClassData, UClass*& GraphNodeClass) const override;
	// End UAIGraphSchema interface
};
