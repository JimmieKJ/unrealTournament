// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class FPackageContentCommands : public TCommands<FPackageContentCommands>
{
public:
	FPackageContentCommands();

	virtual void RegisterCommands() override;

public:

	TSharedPtr< FUICommandInfo > OpenPackageContent;
};