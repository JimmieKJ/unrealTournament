// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PLUGIN_NAMEPrivatePCH.h"
#include "PLUGIN_NAMECommands.h"

#define LOCTEXT_NAMESPACE "FPLUGIN_NAMEModule"

void FPLUGIN_NAMECommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "PLUGIN_NAME", "Bring up PLUGIN_NAME window", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
