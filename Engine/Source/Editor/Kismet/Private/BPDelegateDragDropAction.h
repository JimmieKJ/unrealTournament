// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditor.h"
#include "BPVariableDragDropAction.h"

/** DragDropAction class for dropping a Variable onto a graph */
class KISMET_API FKismetDelegateDragDropAction : public FKismetVariableDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FKismetDelegateDragDropAction, FKismetVariableDragDropAction)

	// FGraphEditorDragDropAction interface
	virtual void HoverTargetChanged() override { FGraphEditorDragDropAction::HoverTargetChanged(); }
	virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) override { return FGraphEditorDragDropAction::DroppedOnPin(ScreenPosition, GraphPosition); }
	virtual FReply DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition) override { return FGraphEditorDragDropAction::DroppedOnNode(ScreenPosition, GraphPosition); }

	virtual FReply DroppedOnPanel(const TSharedRef< SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;

	virtual bool IsSupportedBySchema(const class UEdGraphSchema* Schema) const override;
	// End of FGraphEditorDragDropAction

	bool IsValid() const;
	
	static TSharedRef<FKismetDelegateDragDropAction> New( FName InVariableName, UStruct* InSource, FNodeCreationAnalytic AnalyticCallback)
	{
		TSharedRef<FKismetDelegateDragDropAction> Operation = MakeShareable(new FKismetDelegateDragDropAction);
		Operation->VariableName = InVariableName;
		Operation->VariableSource = InSource;
		Operation->AnalyticCallback = AnalyticCallback;
		Operation->Construct();
		return Operation;
	}

	/** Structure for required node construction parameters */
	struct FNodeConstructionParams
	{
		FVector2D GraphPosition;
		UEdGraph* Graph;
		bool bSelfContext;
		const UProperty* Property;
		FNodeCreationAnalytic AnalyticCallback;
	};

	template<class TNode> static void MakeMCDelegateNode(FNodeConstructionParams Params)
	{
		check(Params.Graph && Params.Property);
		TNode* Node = NewObject<TNode>();
		Node->SetFromProperty(Params.Property, Params.bSelfContext);
		FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<TNode>(Params.Graph, Node, Params.GraphPosition);
		Params.AnalyticCallback.ExecuteIfBound();
	}

	/** Create new custom event node from construction parameters */
	static void MakeEvent(FNodeConstructionParams Params);

	/** Assign new delegate node from construction parameters */
	static void AssignEvent(FNodeConstructionParams Params);

protected:
	FKismetDelegateDragDropAction() {}
};
