// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"
#include "EditorStyle.h"
#include "PluginCreatorStyle.h"

class FPluginCreatorCommands : public TCommands<FPluginCreatorCommands>
{
public:

	FPluginCreatorCommands()
		: TCommands<FPluginCreatorCommands>(TEXT("PCPlugin"), NSLOCTEXT("Contexts", "PCPlugin", "Plugin Creator Plugin"), NAME_None, FPluginCreatorStyle::GetStyleSetName())
	{
		}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(OpenPluginCreator, "New Plugin...", "Creates a new plugin.  The code can only be compiled if you have an appropriate C++ compiler installed.", EUserInterfaceActionType::Button, FInputGesture());
	}

	TSharedPtr< FUICommandInfo > OpenPluginCreator;

};
