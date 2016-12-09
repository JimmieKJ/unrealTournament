// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaCommonCommands.h"

#define LOCTEXT_NAMESPACE "PersonaCommonCommands"

void FPersonaCommonCommands::RegisterCommands()
{
	UI_COMMAND(TogglePlay, "Play/Pause", "Play or pause the current animation", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::SpaceBar));
}

#undef LOCTEXT_NAMESPACE
