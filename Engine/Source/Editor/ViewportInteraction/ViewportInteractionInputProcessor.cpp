// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ViewportInteractionInputProcessor.h"
#include "Input/Events.h"
#include "Misc/App.h"
#include "ViewportWorldInteractionManager.h"

FViewportInteractionInputProcessor::FViewportInteractionInputProcessor( FViewportWorldInteractionManager* InWorldInteractionManager )
	: WorldInteractionManager( InWorldInteractionManager )
{
}

FViewportInteractionInputProcessor::~FViewportInteractionInputProcessor()
{
	WorldInteractionManager = nullptr;
}

void FViewportInteractionInputProcessor::Tick( const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor )
{
}

bool FViewportInteractionInputProcessor::HandleKeyDownEvent( FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent )
{
	return WorldInteractionManager->HandleInputKey( InKeyEvent.GetKey(), InKeyEvent.IsRepeat() ? IE_Repeat : IE_Pressed );
}

bool FViewportInteractionInputProcessor::HandleKeyUpEvent( FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent )
{
	return WorldInteractionManager->HandleInputKey( InKeyEvent.GetKey(), IE_Released );
}

bool FViewportInteractionInputProcessor::HandleAnalogInputEvent( FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent )
{
	return WorldInteractionManager->HandleInputAxis( InAnalogInputEvent.GetUserIndex(), InAnalogInputEvent.GetKey(), InAnalogInputEvent.GetAnalogValue(), FApp::GetDeltaTime() );
}

bool FViewportInteractionInputProcessor::HandleMouseMoveEvent( FSlateApplication& SlateApp, const FPointerEvent& MouseEvent )
{
	return false;
}
