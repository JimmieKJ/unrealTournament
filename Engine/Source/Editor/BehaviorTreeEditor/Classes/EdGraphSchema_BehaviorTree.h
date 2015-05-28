// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIGraphSchema.h"
#include "EdGraphSchema_BehaviorTree.generated.h"

/** Action to auto arrange the graph */
USTRUCT()
struct FBehaviorTreeSchemaAction_AutoArrange : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	FBehaviorTreeSchemaAction_AutoArrange() 
		: FEdGraphSchemaAction() {}

	FBehaviorTreeSchemaAction_AutoArrange(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping) 
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface
};

UCLASS(MinimalAPI)
class UEdGraphSchema_BehaviorTree : public UAIGraphSchema
{
	GENERATED_UCLASS_BODY()

	// Begin EdGraphSchema interface
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual void GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual const FPinConnectionResponse CanMergeNodes(const UEdGraphNode* A, const UEdGraphNode* B) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual class FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const override;
	virtual int32 GetNodeSelectionCount(const UEdGraph* Graph) const override;
	virtual bool IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const override;
	virtual int32 GetCurrentVisualizationCacheID() const override;
	virtual void ForceVisualizationCacheClear() const override;
	// End EdGraphSchema interface

	virtual void GetGraphNodeContextActions(FGraphContextMenuBuilder& ContextMenuBuilder, int32 SubNodeFlags) const override;
	virtual void GetSubNodeClasses(int32 SubNodeFlags, TArray<FGraphNodeClassData>& ClassData, UClass*& GraphNodeClass) const override;

private:
	// ID for checking dirty status of node titles against, increases whenever 
	static int32 CurrentCacheRefreshID;
};

