// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Application/IInputProcessor.h"

class UViewportWorldInteraction;

class FViewportInteractionInputProcessor : public IInputProcessor
{
public:
	FViewportInteractionInputProcessor ( UViewportWorldInteraction& InWorldInteraction );
	virtual ~FViewportInteractionInputProcessor ();

	virtual void Tick( const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor ) override;

	/** Key down input */
	virtual bool HandleKeyDownEvent( FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent ) override;
	
	/** Key up input */
	virtual bool HandleKeyUpEvent( FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent ) override;
	
	/** Analog axis input */
	virtual bool HandleAnalogInputEvent( FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent ) override;

	/** Mouse movement input */
	virtual bool HandleMouseMoveEvent( FSlateApplication& SlateApp, const FPointerEvent& MouseEvent ) override;

private:

	/** The WorldInteraction that will receive the input */
	UViewportWorldInteraction& WorldInteraction;
};
