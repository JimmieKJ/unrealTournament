// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PLUGIN_NAMEPrivatePCH.h"
#include "PLUGIN_NAMECommands.h"

#define LOCTEXT_NAMESPACE "FPLUGIN_NAMEModule"

void FPLUGIN_NAMECommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "PLUGIN_NAME", "Execute PLUGIN_NAME action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
