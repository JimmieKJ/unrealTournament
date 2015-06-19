// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop.h"

class FDragConnection : public FGraphEditorDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FDragConnection, FGraphEditorDragDropAction)

	static TSharedRef<FDragConnection> New(const TSharedRef<SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphPin> >& InStartingPins, bool bInShiftOperation);
	
	// FDragDropOperation interface
	virtual void OnDrop( bool bDropWasHandled, const FPointerEvent& MouseEvent ) override;
	// End of FDragDropOperation interface

	// FGraphEditorDragDropAction interface
	virtual void HoverTargetChanged() override;
	virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnPanel(const TSharedRef< SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	virtual void OnDragBegin(const TSharedRef<class SGraphPin>& InPin) override;
	virtual void OnDragged(const class FDragDropEvent& DragDropEvent) override;
	// End of FGraphEditorDragDropAction interface

	/*
	 *	Function to check validity of graph pins in the StartPins list. This check helps to prevent processing graph pins which are outdated.
	 */
	virtual void ValidateGraphPinList(TArray<UEdGraphPin*>& OutValidPins);

protected:
	typedef FGraphEditorDragDropAction Super;

	// Constructor: Make sure to call Construct() after factorying one of these
	FDragConnection(const TSharedRef<SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphPin> >& InStartingPins, bool bInShiftOperation);

protected:
	TSharedPtr<SGraphPanel> GraphPanel;
	TArray< TSharedRef<SGraphPin> > StartingPins;

	/** Offset information for the decorator widget */
	FVector2D DecoratorAdjust;

	/** Was shift pressed when the drag started? */
	bool bShiftOperation;
};
