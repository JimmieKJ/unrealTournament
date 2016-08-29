// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop.h"

// Base class for drag-drop actions that pass into the graph editor and perform an action when dropped
class GRAPHEDITOR_API FGraphEditorDragDropAction : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FGraphEditorDragDropAction, FDragDropOperation)

	void SetHoveredPin(class UEdGraphPin* InPin);
	void SetHoveredNode(const TSharedPtr<class SGraphNode>& InNode);
	void SetHoveredGraph(const TSharedPtr<class SGraphPanel>& InGraph);
	void SetHoveredCategoryName(const FText& InHoverCategoryName);
	void SetHoveredAction(TSharedPtr<struct FEdGraphSchemaAction> Action);
	void SetDropTargetValid( bool bValid ) { bDropTargetValid = bValid; }

	// Interface to override
	virtual void HoverTargetChanged() {}
	virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) { return FReply::Unhandled(); }
	virtual FReply DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition) { return FReply::Unhandled(); }
	virtual FReply DroppedOnPanel( const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) { return FReply::Unhandled(); }
	virtual FReply DroppedOnAction(TSharedRef<struct FEdGraphSchemaAction> Action) { return FReply::Unhandled(); }
	virtual FReply DroppedOnCategory(FText Category) { return FReply::Unhandled(); }
	virtual void OnDragBegin(const TSharedRef<class SGraphPin>& InPin) {}
	// End of interface to override
	
	virtual bool IsSupportedBySchema(const class UEdGraphSchema* Schema) const { return true; }

	bool HasFeedbackMessage();
	void SetFeedbackMessage(const TSharedPtr<SWidget>& Message);
	void SetSimpleFeedbackMessage(const FSlateBrush* Icon, const FSlateColor& IconColor, const FText& Message);

protected:
	UEdGraphPin* GetHoveredPin() const;
	UEdGraphNode* GetHoveredNode() const;
	UEdGraph* GetHoveredGraph() const;

	/** Constructs the window and widget if applicable */
	virtual void Construct() override;

private:

	EVisibility GetIconVisible() const;
	EVisibility GetErrorIconVisible() const;

	// The pin that the drag action is currently hovering over
	FEdGraphPinReference HoveredPin;

	// The node that the drag action is currently hovering over
	TSharedPtr<class SGraphNode> HoveredNode;

	// The graph that the drag action is currently hovering over
	TSharedPtr<class SGraphPanel> HoveredGraph;

protected:

	// Name of category we are hovering over
	FText HoveredCategoryName;

	// Action we are hovering over
	TWeakPtr<struct FEdGraphSchemaAction> HoveredAction;

	// drop target status
	bool bDropTargetValid;
};

// Drag-drop action where an FEdGraphSchemaAction should be performed when dropped
class GRAPHEDITOR_API FGraphSchemaActionDragDropAction : public FGraphEditorDragDropAction
{
public:
	
	DRAG_DROP_OPERATOR_TYPE(FGraphSchemaActionDragDropAction, FGraphEditorDragDropAction)

	// FGraphEditorDragDropAction interface
	virtual void HoverTargetChanged() override;
	virtual FReply DroppedOnPanel( const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	// End of FGraphEditorDragDropAction

	static TSharedRef<FGraphSchemaActionDragDropAction> New(TSharedPtr<FEdGraphSchemaAction> InActionNode )
	{
		TSharedRef<FGraphSchemaActionDragDropAction> Operation = MakeShareable(new FGraphSchemaActionDragDropAction);
		Operation->ActionNode = InActionNode;
		Operation->Construct();
		return Operation;
	}

protected:
	/** */
	TSharedPtr<FEdGraphSchemaAction> ActionNode;
};
