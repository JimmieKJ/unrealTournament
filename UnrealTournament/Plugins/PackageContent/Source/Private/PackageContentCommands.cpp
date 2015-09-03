// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PackageContentPrivatePCH.h"
#include "PackageContentCommands.h"

#define LOCTEXT_NAMESPACE "PackageContent"

FPackageContentCommands::FPackageContentCommands()
: TCommands<FPackageContentCommands>(
TEXT("PackageContent"), // Context name for fast lookup
LOCTEXT("PackageContent", "Share Content"), // Localized context name for displaying
NAME_None,	 // No parent context
FEditorStyle::GetStyleSetName() // Icon Style Set
)
{
}

void FPackageContentCommands::RegisterCommands()
{
	UI_COMMAND(PackageLevel, "Share This Level", "Cook and package up this level.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PackageWeapon, "Share A Weapon", "Cook and package up a weapon.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PackageHat, "Share A Cosmetic Item", "Cook and package up a cosmetic item.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PackageTaunt, "Share A Taunt", "Cook and package up a taunt.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PackageMutator, "Share A Mutator", "Cook and package up a mutator.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PackageCharacter, "Share A Character", "Cook and package up a character.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PackageCrosshair, "Share A Crosshair", "Cook and package up a crosshair.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ComingSoon, "Coming Soon", "We will support content publishing on this platform in the near future.", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE