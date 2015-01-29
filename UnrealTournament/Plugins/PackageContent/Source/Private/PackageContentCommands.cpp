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
	UI_COMMAND(PackageLevel, "Package This Level", "Cook and package up this level.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PackageWeapon, "Package A Weapon", "Cook and package up a weapon.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PackageHat, "Package A Cosmetic Item", "Cook and package up a cosmetic item.", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE