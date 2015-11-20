// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerEdMode.h"
#include "Sequencer.h"

const FEditorModeID FSequencerEdMode::EM_SequencerMode(TEXT("EM_SequencerMode"));

FSequencerEdMode::FSequencerEdMode() :
	Sequencer(NULL)
{
	FSequencerEdModeTool* SequencerEdModeTool = new FSequencerEdModeTool();

	Tools.Add( SequencerEdModeTool );
	SetCurrentTool( SequencerEdModeTool );
}

FSequencerEdMode::~FSequencerEdMode()
{
}

void FSequencerEdMode::Enter()
{
	FEdMode::Enter();
}

void FSequencerEdMode::Exit()
{
	Sequencer = NULL;

	FEdMode::Exit();
}

bool FSequencerEdMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	// Compatible with all modes so that we can take over with the sequencer hotkeys
	return true;
}

bool FSequencerEdMode::InputKey( FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event )
{
	if (Sequencer == NULL)
	{
		return false;
	}

	if (Event != IE_Released)
	{
		FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();

		if (Sequencer->GetCommandBindings().Get()->ProcessCommandBindings(Key, KeyState, (Event == IE_Repeat) ))
		{
			return true;
		}
	}

	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

FSequencerEdModeTool::FSequencerEdModeTool()
{
}

FSequencerEdModeTool::~FSequencerEdModeTool()
{
}

bool FSequencerEdModeTool::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	return FModeTool::InputKey(ViewportClient, Viewport, Key, Event);
}
