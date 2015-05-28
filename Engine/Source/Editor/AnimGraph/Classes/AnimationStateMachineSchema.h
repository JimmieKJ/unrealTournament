// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "AnimationStateMachineSchema.generated.h"

/** Action to add a node to the graph */
USTRUCT()
struct ANIMGRAPH_API FEdGraphSchemaAction_NewStateNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	UEdGraphNode* NodeTemplate;

	FEdGraphSchemaAction_NewStateNode() 
		: FEdGraphSchemaAction()
		, NodeTemplate(NULL)
	{}

	FEdGraphSchemaAction_NewStateNode(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
		, NodeTemplate(NULL)
	{}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	template <typename NodeType>
	static NodeType* SpawnNodeFromTemplate(class UEdGraph* ParentGraph, NodeType* InTemplateNode, const FVector2D Location = FVector2D(0.0f, 0.0f), bool bSelectNewNode = true)
	{
		FEdGraphSchemaAction_NewStateNode Action;
		Action.NodeTemplate = InTemplateNode;

		return Cast<NodeType>(Action.PerformAction(ParentGraph, NULL, Location, bSelectNewNode));
	}
};

/** Action to create new comment */
USTRUCT()
struct ANIMGRAPH_API FEdGraphSchemaAction_NewStateComment : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	FEdGraphSchemaAction_NewStateComment() 
		: FEdGraphSchemaAction()
	{}

	FEdGraphSchemaAction_NewStateComment(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface
};


UCLASS(MinimalAPI)
class UAnimationStateMachineSchema : public UEdGraphSchema
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FString PC_Exec;

	// Begin UEdGraphSchema interface
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
	virtual bool CreateAutomaticConversionNodeAndConnections(UEdGraphPin* PinA, UEdGraphPin* PinB) const override;
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual EGraphType GetGraphType(const UEdGraph* TestEdGraph) const override;
	virtual void GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, FMenuBuilder* MenuBuilder, bool bIsDebugging) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual void GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const override;
	virtual void DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const override;
	virtual void DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const override;
	virtual void DroppedAssetsOnPin(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphPin* Pin) const override;
	virtual void GetAssetsNodeHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphNode* HoverNode, FString& OutTooltipText, bool& OutOkIcon) const override;
	virtual void GetAssetsPinHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphPin* HoverPin, FString& OutTooltipText, bool& OutOkIcon) const override;
	// End UEdGraphSchema interface
};
