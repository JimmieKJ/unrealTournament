// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class FPackageContentCommands : public TCommands<FPackageContentCommands>
{
public:
	FPackageContentCommands();

	virtual void RegisterCommands() override;

public:

	TSharedPtr< FUICommandInfo > PackageLevel;
	TSharedPtr< FUICommandInfo > PackageWeapon;
	TSharedPtr< FUICommandInfo > PackageHat;
	TSharedPtr< FUICommandInfo > PackageCharacter;
	TSharedPtr< FUICommandInfo > PackageTaunt;
	TSharedPtr< FUICommandInfo > PackageMutator;
	TSharedPtr< FUICommandInfo > PackageCrosshair;

	TSharedPtr< FUICommandInfo > ComingSoon;
};