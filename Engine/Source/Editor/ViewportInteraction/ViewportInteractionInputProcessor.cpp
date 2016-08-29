// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ViewportInteractionModule.h"
#include "ViewportInteractionInputProcessor.h"
#include "ViewportWorldInteraction.h"
#include "SLevelViewport.h"

FViewportInteractionInputProcessor::FViewportInteractionInputProcessor( UViewportWorldInteraction& InWorldInteraction  )
	: WorldInteraction( InWorldInteraction  )
{
}

FViewportInteractionInputProcessor::~FViewportInteractionInputProcessor()
{
}

void FViewportInteractionInputProcessor::Tick( const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor )
{
}

bool FViewportInteractionInputProcessor::HandleKeyDownEvent( FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent )
{
	return WorldInteraction.HandleInputKey( InKeyEvent.GetKey(), InKeyEvent.IsRepeat() ? IE_Repeat : IE_Pressed );
}

bool FViewportInteractionInputProcessor::HandleKeyUpEvent( FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent )
{
	return WorldInteraction.HandleInputKey( InKeyEvent.GetKey(), IE_Released );
}

bool FViewportInteractionInputProcessor::HandleAnalogInputEvent( FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent )
{
	return WorldInteraction.HandleInputAxis( InAnalogInputEvent.GetUserIndex(), InAnalogInputEvent.GetKey(), InAnalogInputEvent.GetAnalogValue(), FApp::GetDeltaTime() );
}

bool FViewportInteractionInputProcessor::HandleMouseMoveEvent( FSlateApplication& SlateApp, const FPointerEvent& MouseEvent )
{
	return false;
}
