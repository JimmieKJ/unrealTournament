// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop.h"

/**
 * This is the drag/drop class used for UMG, all UMG drag drop operations utilize this operation.
 * It supports moving a UObject payload and using a UWidget decorator.
 */
class FUMGDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FUMGDragDropOp, FDragDropOperation)
	
	static TSharedRef<FUMGDragDropOp> New(UDragDropOperation* Operation, const FVector2D &CursorPosition, const FVector2D &ScreenPositionOfNode, TSharedPtr<SObjectWidget> SourceUserWidget);

	virtual void OnDrop( bool bDropWasHandled, const FPointerEvent& MouseEvent ) override;

	virtual void OnDragged( const class FDragDropEvent& DragDropEvent ) override;

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	UDragDropOperation* GetOperation() const { return DragOperation.Get(); }

private:

	TWeakObjectPtr<UDragDropOperation> DragOperation;

	/** Source User Widget */
	TSharedPtr<SObjectWidget> SourceUserWidget;

	/** The widget used during the drag/drop action to show something being dragged. */
	TSharedPtr<SWidget> DecoratorWidget;

	/** The offset to use when dragging the object so that it says the same distance away from the mouse. */
	FVector2D Offset;

	/** The starting screen location where the drag operation started. */
	FVector2D StartingScreenPos;
};
