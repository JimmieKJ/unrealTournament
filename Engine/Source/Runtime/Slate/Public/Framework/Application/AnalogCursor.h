// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IInputProcessor.h"

namespace AnalogCursorMode
{
	enum Type
	{
		Accelerated,
		Direct,
	};
}

/**
 * A class that simulates a cursor driven by an analog stick.
 */
class SLATE_API FAnalogCursor : public IInputProcessor
{
public:
	FAnalogCursor();

	/** Dtor */
	virtual ~FAnalogCursor()
	{}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;

	void SetAcceleration(float NewAcceleration);
	void SetMaxSpeed(float NewMaxSpeed);
	void SetStickySlowdown(float NewStickySlowdown);
	void SetDeadZone(float NewDeadZone);
	void SetMode(AnalogCursorMode::Type NewMode);

protected:

	/** Getter */
	FORCEINLINE FVector2D GetAnalogValues() const
	{
		return AnalogValues;
	}

	/** Handles updating the cursor position and processing a Mouse Move Event */
	void UpdateCursorPosition(FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor, const FVector2D& NewPosition);

	/** Current speed of the cursor */
	FVector2D CurrentSpeed;

	/** Current sub-pixel offset */
	FVector2D CurrentOffset;

	/** Current settings */
	float Acceleration;
	float MaxSpeed;
	float StickySlowdown;
	float DeadZone;
	AnalogCursorMode::Type Mode;

private:

	/** Input from the gamepad */
	FVector2D AnalogValues;
};

