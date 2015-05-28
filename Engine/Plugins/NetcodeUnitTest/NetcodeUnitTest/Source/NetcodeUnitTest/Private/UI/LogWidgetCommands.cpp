// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"
#include "LogWidgetCommands.h"
#include "Framework/Commands/UICommandInfo.h"

#define LOCTEXT_NAMESPACE ""

void FLogWidgetCommands::RegisterCommands()
{
#if TARGET_UE4_CL < CL_INPUTCHORD
	UI_COMMAND(CopyLogLines, "Copy", "Copies the selected log lines to the clipboard", EUserInterfaceActionType::Button,
				FInputGesture(EModifierKey::Control, EKeys::C));
#else
	UI_COMMAND(CopyLogLines, "Copy", "Copies the selected log lines to the clipboard", EUserInterfaceActionType::Button,
				FInputChord(EModifierKey::Control, EKeys::C));
#endif

#if TARGET_UE4_CL < CL_INPUTCHORD
	UI_COMMAND(FindLogText, "Find", "Find text within the current log window tab", EUserInterfaceActionType::Button,
				FInputGesture(EModifierKey::Control, EKeys::F));
#else
	UI_COMMAND(FindLogText, "Find", "Find text within the current log window tab", EUserInterfaceActionType::Button,
				FInputChord(EModifierKey::Control, EKeys::F));
#endif
}

#undef LOCTEXT_NAMESPACE
