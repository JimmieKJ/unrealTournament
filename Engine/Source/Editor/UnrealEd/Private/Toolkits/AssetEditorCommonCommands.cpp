// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AssetEditorCommonCommands.h"

#define LOCTEXT_NAMESPACE ""

void FAssetEditorCommonCommands::RegisterCommands()
{
	UI_COMMAND( SaveAsset, "Save", "Saves this asset", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ReimportAsset, "Reimport", "Reimports the asset being edited", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND( SwitchToStandaloneEditor, "Switch to Standalone Editor", "Closes the level-centric asset editor and reopens it in 'standalone' mode", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( SwitchToWorldCentricEditor, "Switch to World-Centric Editor", "Closes the standalone asset editor and reopens it in 'world-centric' mode, docked within the level editor that it was originally opened in.", EUserInterfaceActionType::Button, FInputChord() );
}

#undef LOCTEXT_NAMESPACE