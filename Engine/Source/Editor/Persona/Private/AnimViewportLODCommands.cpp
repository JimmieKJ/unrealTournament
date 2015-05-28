// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "AnimViewportLODCommands.h"

#define LOCTEXT_NAMESPACE ""

void FAnimViewportLODCommands::RegisterCommands()
{
	UI_COMMAND( LODAuto, "LOD Auto", "Automatically select LOD", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( LOD0, "LOD 0", "Force select LOD 0", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( LOD1, "LOD 1", "Force select LOD 1", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( LOD2, "LOD 2", "Force select LOD 2", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( LOD3, "LOD 3", "Force select LOD 3", EUserInterfaceActionType::RadioButton, FInputChord() );
}

#undef LOCTEXT_NAMESPACE