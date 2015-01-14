// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PackageContentPrivatePCH.h"
#include "PackageContentCommands.h"

#define LOCTEXT_NAMESPACE "PackageContent"

FPackageContentCommands::FPackageContentCommands()
: TCommands<FPackageContentCommands>(
TEXT("PackageContent"), // Context name for fast lookup
LOCTEXT("PackageContent", "Package Content"), // Localized context name for displaying
NAME_None,	 // No parent context
FEditorStyle::GetStyleSetName() // Icon Style Set
)
{
}

void FPackageContentCommands::RegisterCommands()
{
	UI_COMMAND(OpenPackageContent, "Open Package Content", "Put your custom content into a package file to get it ready for marketplace.", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE