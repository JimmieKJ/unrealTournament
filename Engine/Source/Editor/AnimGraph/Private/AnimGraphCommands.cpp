// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphCommands.h"

#define LOCTEXT_NAMESPACE "AnimGraphCommands"

void FAnimGraphCommands::RegisterCommands()
{
	UI_COMMAND(TogglePoseWatch, "Toggle Pose Watch", "Toggle Pose Watching on this node", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE