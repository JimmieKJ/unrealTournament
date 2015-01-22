// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "PaperEditorCommands.h"

//////////////////////////////////////////////////////////////////////////
// FPaperEditorCommands

void FPaperEditorCommands::RegisterCommands()
{
	UI_COMMAND(EnterTileMapEditMode, "Enable Tile Map Mode", "Enables Tile Map editing mode", EUserInterfaceActionType::ToggleButton, FInputGesture());

	UI_COMMAND(SelectPaintTool, "Paint", "Paint", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(SelectEraserTool, "Eraser", "Eraser", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(SelectFillTool, "Fill", "Paint Bucket", EUserInterfaceActionType::ToggleButton, FInputGesture());

	UI_COMMAND(SelectVisualLayersPaintingMode, "VisualLayers", "Visual Layers", EUserInterfaceActionType::ToggleButton, FInputGesture());
	UI_COMMAND(SelectCollisionLayersPaintingMode, "CollisionLayers", "Collision Layers", EUserInterfaceActionType::ToggleButton, FInputGesture());
}
	
	