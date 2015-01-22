// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class IBspModeModule : public IModuleInterface
{
public:
	virtual TSharedRef< SWidget > CreateBspModeWidget() const = 0;

	virtual void RegisterBspBuilderType( class UClass* InBuilderClass, const FText& InBuilderName, const FText& InBuilderTooltip, const FSlateBrush* InBuilderIcon ) = 0;
	virtual void UnregisterBspBuilderType( class UClass* InBuilderClass ) = 0;
};

