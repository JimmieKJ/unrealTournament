// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimationEditorCommands.h"

#define LOCTEXT_NAMESPACE "AnimationEditorCommands"

void FAnimationEditorCommands::RegisterCommands()
{
	UI_COMMAND(ImportAnimation, "Import Animation", "Import new animation for the skeleton.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReimportAnimation, "Reimport Animation", "Reimport current animation.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ApplyCompression, "Apply Compression", "Apply compression to current animation", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ExportToFBX, "Export to FBX", "Export current animation to FBX", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddLoopingInterpolation, "Add Looping Interpolation", "Add an extra first frame at the end of the animation to create interpolation when looping", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SetKey, "Set Key", "Add Bone Transform to Additive Layer Tracks", EUserInterfaceActionType::Button, FInputChord(EKeys::S));
	UI_COMMAND(ApplyAnimation, "Apply Animation", "Apply Additive Layer Tracks to Runtime Animation Data", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
