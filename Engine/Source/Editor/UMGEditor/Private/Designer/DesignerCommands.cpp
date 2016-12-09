// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Designer/DesignerCommands.h"

#define LOCTEXT_NAMESPACE "DesignerCommands"

void FDesignerCommands::RegisterCommands()
{
	UI_COMMAND( LayoutTransform, "Layout Transform Mode", "Adjust widget layout transform", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::W) );
	UI_COMMAND( RenderTransform, "Render Transform Mode", "Adjust widget render transform", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::E) );

	UI_COMMAND( LocationGridSnap, "Grid Snap", "Enables or disables snapping to the grid when dragging objects around", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( RotationGridSnap, "Rotation Snap", "Enables or disables snapping objects to a rotation grid", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( ToggleOutlines, "Show Outlines", "Enables or disables showing the dashed outlines", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::G) );
}

#undef LOCTEXT_NAMESPACE
