// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSlateApplication;

/**
 * Interface for a Slate Input Handler
 */
class SLATE_API IInputProcessor
{
public:
	IInputProcessor(){};
	virtual ~IInputProcessor(){}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) = 0;

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) = 0;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) = 0;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) = 0;

};

