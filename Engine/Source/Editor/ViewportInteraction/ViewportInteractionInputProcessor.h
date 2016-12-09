// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/ICursor.h"
#include "Framework/Application/IInputProcessor.h"

class FSlateApplication;
class FViewportWorldInteractionManager;
struct FAnalogInputEvent;
struct FKeyEvent;
struct FPointerEvent;

class FViewportInteractionInputProcessor : public IInputProcessor
{
public:
	FViewportInteractionInputProcessor ( FViewportWorldInteractionManager* InWorldInteractionManager );
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
	FViewportWorldInteractionManager* WorldInteractionManager;

};
