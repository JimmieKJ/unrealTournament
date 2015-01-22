// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "SoundCueGraphEditorCommands.h"

void FSoundCueGraphEditorCommands::RegisterCommands()
{
	UI_COMMAND(PlayCue, "Play Cue", "Plays the SoundCue", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PlayNode, "Play Node", "Plays the currently selected node", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(StopCueNode, "Stop", "Stops the currently playing cue/node", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(TogglePlayback, "Toggle Playback", "Plays the SoundCue or stops the currently playing cue/node", EUserInterfaceActionType::Button, FInputGesture(EKeys::SpaceBar));

	UI_COMMAND(BrowserSync, "Sync to Browser", "Selects the SoundWave in the content browser", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(AddInput, "Add Input", "Adds an input to the node", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(DeleteInput, "Delete Input", "Removes an input from the node", EUserInterfaceActionType::Button, FInputGesture());
}
