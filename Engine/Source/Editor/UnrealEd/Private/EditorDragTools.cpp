// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "EditorDragTools.h"
#include "SnappingUtils.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FDragTool
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FDragTool::FDragTool()
	:	bConvertDelta( true )
	,	Start( FVector::ZeroVector )
	,	End( FVector::ZeroVector )
	,	EndWk( FVector::ZeroVector )
	,	bUseSnapping( false )
	,   bIsDragging( false )
{
}

/**
 * Updates the drag tool's end location with the specified delta.  The end location is
 * snapped to the editor constraints if bUseSnapping is true.
 *
 * @param	InDelta		A delta of mouse movement.
 */
void FDragTool::AddDelta( const FVector& InDelta )
{
	EndWk += InDelta;

	End = EndWk;

	// Snap to constraints.
	if( bUseSnapping )
	{
		const float GridSize = GEditor->GetGridSize();
		const FVector GridBase( GridSize, GridSize, GridSize );
		FSnappingUtils::SnapPointToGrid( End, GridBase );
	}
}

/**
 * Starts a mouse drag behavior.  The start location is snapped to the editor constraints if bUseSnapping is true.
 *
 * @param	InViewportClient	The viewport client in which the drag event occurred.
 * @param	InStart				Where the mouse was when the drag started.
 */
void FDragTool::StartDrag(FEditorViewportClient* InViewportClient, const FVector& InStart, const FVector2D& StartScreen)
{
	Start = InStart;
	bIsDragging = true;

	// Snap to constraints.
	if( bUseSnapping )
	{
		const float GridSize = GEditor->GetGridSize();
		const FVector GridBase( GridSize, GridSize, GridSize );
		FSnappingUtils::SnapPointToGrid( Start, GridBase );
	}
	End = EndWk = Start;

	// Store button state when the drag began.
	bAltDown = InViewportClient->IsAltPressed();
	bShiftDown = InViewportClient->IsShiftPressed();
	bControlDown = InViewportClient->IsCtrlPressed();
	bLeftMouseButtonDown = InViewportClient->Viewport->KeyState(EKeys::LeftMouseButton);
	bRightMouseButtonDown = InViewportClient->Viewport->KeyState(EKeys::RightMouseButton);
	bMiddleMouseButtonDown = InViewportClient->Viewport->KeyState(EKeys::MiddleMouseButton);
}

/**
 * Ends a mouse drag behavior (the user has let go of the mouse button).
 */
void FDragTool::EndDrag()
{
	Start = End = EndWk = FVector::ZeroVector;
	bIsDragging = false;
}
