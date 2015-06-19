// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPrivatePCH.h"

#define LOCTEXT_NAMESPACE ""

void FMediaPlayerEditorCommands::RegisterCommands()
{
	UI_COMMAND(ForwardMedia, "Forward", "Fast forwards media playback", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PauseMedia, "Pause", "Pause media playback", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PlayMedia, "Play", "Start media playback", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReverseMedia, "Reverse", "Reverses media playback", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RewindMedia, "Rewind", "Rewinds the media to the beginning", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE