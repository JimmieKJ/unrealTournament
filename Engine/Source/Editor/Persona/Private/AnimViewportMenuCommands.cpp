// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "AnimViewportMenuCommands.h"

#define LOCTEXT_NAMESPACE ""

void FAnimViewportMenuCommands::RegisterCommands()
{
	UI_COMMAND( Auto, "Auto", "Preview what is active on the current window", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( Lock, "Lock", "Lock to current preview", EUserInterfaceActionType::RadioButton, FInputChord() );

	UI_COMMAND( CameraFollow, "Camera Follow", "Follow the bound of the mesh ", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( UseInGameBound, "In-game Bound", "Use in-game bound on preview mesh", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( SetShowNormals, "Normals", "Toggles display of vertex normals in the Preview Pane.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetShowTangents, "Tangents", "Toggles display of vertex tangents in the Preview Pane.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetShowBinormals, "Binormals", "Toggles display of vertex binormals (orthogonal vector to normal and tangent) in the Preview Pane.", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( AnimSetDrawUVs, "UV", "Toggles display of the mesh's UVs for the specified channel.", EUserInterfaceActionType::ToggleButton, FInputChord() );
}

#undef LOCTEXT_NAMESPACE