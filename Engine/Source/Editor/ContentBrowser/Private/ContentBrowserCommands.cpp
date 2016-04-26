// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"
#include "ContentBrowserCommands.h"


#define LOCTEXT_NAMESPACE "ContentBrowser"


void FContentBrowserCommands::RegisterCommands()
{
	UI_COMMAND(OpenAssetsOrFolders, "Open Assets or Folders", "Opens the selected assets or folders, depending on the selection", EUserInterfaceActionType::Button, FInputChord(EKeys::Enter));
	UI_COMMAND(PreviewAssets, "Preview Assets", "Loads the selected assets and previews them if possible", EUserInterfaceActionType::Button, FInputChord(EKeys::SpaceBar));
	UI_COMMAND(DirectoryUp, "Up", "Up to parent directory", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Up));
	UI_COMMAND(CreateNewFolder, "Create New Folder", "Creates new folder in selected path", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::N));
}


#undef LOCTEXT_NAMESPACE
