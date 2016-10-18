// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "AnalogCursor.h"

#include "HittestGrid.h"

FAnalogCursor::FAnalogCursor()
: CurrentSpeed(FVector2D::ZeroVector)
, CurrentOffset(FVector2D::ZeroVector)
, Acceleration(1000.0f)
, MaxSpeed(1500.0f)
, StickySlowdown(0.5f)
, DeadZone(0.1f)
, Mode(AnalogCursorMode::Accelerated)
{
	AnalogValues[ static_cast< uint8 >( EAnalogStick::Left ) ] = FVector2D::ZeroVector;
	AnalogValues[ static_cast< uint8 >( EAnalogStick::Right ) ] = FVector2D::ZeroVector;
}

void FAnalogCursor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	const FVector2D OldPosition = Cursor->GetPosition();

	float SpeedMult = 1.0f; // Used to do a speed multiplication before adding the delta to the position to make widgets sticky
	FVector2D AdjAnalogVals = GetAnalogValue(EAnalogStick::Left); // A copy of the analog values so I can modify them based being over a widget, not currently doing this

	// Adjust analog values according to dead zone
	const float AnalogValsSize = AdjAnalogVals.Size();

	if (AnalogValsSize > 0.0f)
	{
		const float TargetSize = FMath::Max(AnalogValsSize - DeadZone, 0.0f) / (1.0f - DeadZone);
		AdjAnalogVals /= AnalogValsSize;
		AdjAnalogVals *= TargetSize;
	}


	// Check if there is a sticky widget beneath the cursor
	FWidgetPath WidgetPath = SlateApp.LocateWindowUnderMouse(OldPosition, SlateApp.GetInteractiveTopLevelWindows());
	if (WidgetPath.IsValid())
	{
		const FArrangedChildren::FArrangedWidgetArray& AllArrangedWidgets = WidgetPath.Widgets.GetInternalArray();
		for(const FArrangedWidget& ArrangedWidget : AllArrangedWidgets)
		{
			TSharedRef<SWidget> Widget = ArrangedWidget.Widget;
			if (Widget->IsInteractable())
			{
				SpeedMult = StickySlowdown;
				//FVector2D Adjustment = WidgetsAndCursors.Last().Geometry.Position - OldPosition; // example of calculating distance from cursor to widget center
				break;
			}
		}
	}

	switch(Mode)
	{
		case AnalogCursorMode::Accelerated:
		{
			// Generate Min and Max for X to clamp the speed, this gives us instant direction change when crossing the axis
			float CurrentMinSpeedX = 0.0f;
			float CurrentMaxSpeedX = 0.0f;
			if (AdjAnalogVals.X > 0.0f)
			{ 
				CurrentMaxSpeedX = AdjAnalogVals.X * MaxSpeed;
			}
			else
			{
				CurrentMinSpeedX = AdjAnalogVals.X * MaxSpeed;
			}

			// Generate Min and Max for Y to clamp the speed, this gives us instant direction change when crossing the axis
			float CurrentMinSpeedY = 0.0f;
			float CurrentMaxSpeedY = 0.0f;
			if (AdjAnalogVals.Y > 0.0f)
			{
				CurrentMaxSpeedY = AdjAnalogVals.Y * MaxSpeed;
			}
			else
			{
				CurrentMinSpeedY = AdjAnalogVals.Y * MaxSpeed;
			}

			// Cubic acceleration curve
			FVector2D ExpAcceleration = AdjAnalogVals * AdjAnalogVals * AdjAnalogVals * Acceleration;
			// Preserve direction (if we use a squared equation above)
			//ExpAcceleration.X *= FMath::Sign(AnalogValues.X);
			//ExpAcceleration.Y *= FMath::Sign(AnalogValues.Y);

			CurrentSpeed += ExpAcceleration * DeltaTime;

			CurrentSpeed.X = FMath::Clamp(CurrentSpeed.X, CurrentMinSpeedX, CurrentMaxSpeedX);
			CurrentSpeed.Y = FMath::Clamp(CurrentSpeed.Y, CurrentMinSpeedY, CurrentMaxSpeedY);

			break;
		}

		case AnalogCursorMode::Direct:

			CurrentSpeed = AdjAnalogVals * MaxSpeed;

			break;
	}

	CurrentOffset += CurrentSpeed * DeltaTime * SpeedMult;
	const FVector2D NewPosition = OldPosition + CurrentOffset;

	// update the cursor position
	UpdateCursorPosition(SlateApp, Cursor, NewPosition);

	// save the remaining sub-pixel offset after setting the cursor
	CurrentOffset = NewPosition - Cursor->GetPosition();
}

bool FAnalogCursor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	FKey Key = InKeyEvent.GetKey();

	// Consume the sticks input so it doesn't effect other things
	if (Key == EKeys::Gamepad_LeftStick_Right || 
		Key == EKeys::Gamepad_LeftStick_Left ||
		Key == EKeys::Gamepad_LeftStick_Up ||
		Key == EKeys::Gamepad_LeftStick_Down)
	{
		return true;
	}

	// Bottom face button is a click
	if (Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		if ( !InKeyEvent.IsRepeat() )
		{
			FPointerEvent MouseEvent(
				0,
				SlateApp.GetCursorPos(),
				SlateApp.GetLastCursorPos(),
				SlateApp.PressedMouseButtons,
				EKeys::LeftMouseButton,
				0,
				SlateApp.GetPlatformApplication()->GetModifierKeys()
				);

			TSharedPtr<FGenericWindow> GenWindow;
			return SlateApp.ProcessMouseButtonDownEvent(GenWindow, MouseEvent);
		}

		return true;
	}

	return false;
}

bool FAnalogCursor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	FKey Key = InKeyEvent.GetKey();

	// Consume the sticks input so it doesn't effect other things
	if (Key == EKeys::Gamepad_LeftStick_Right ||
		Key == EKeys::Gamepad_LeftStick_Left ||
		Key == EKeys::Gamepad_LeftStick_Up ||
		Key == EKeys::Gamepad_LeftStick_Down)
	{
		return true;
	}

	// Bottom face button is a click
	if (Key == EKeys::Gamepad_FaceButton_Bottom && !InKeyEvent.IsRepeat())
	{
		FPointerEvent MouseEvent(
			0,
			SlateApp.GetCursorPos(),
			SlateApp.GetLastCursorPos(),
			SlateApp.PressedMouseButtons,
			EKeys::LeftMouseButton,
			0,
			SlateApp.GetPlatformApplication()->GetModifierKeys()
			);

		return SlateApp.ProcessMouseButtonUpEvent(MouseEvent);
	}
	
	return false;
}

bool FAnalogCursor::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
{
	FKey Key = InAnalogInputEvent.GetKey();
	float AnalogValue = InAnalogInputEvent.GetAnalogValue();

	if (Key == EKeys::Gamepad_LeftX)
	{
		FVector2D& Value = GetAnalogValue( EAnalogStick::Left );
		Value.X = AnalogValue;
	}
	else if (Key == EKeys::Gamepad_LeftY)
	{
		FVector2D& Value = GetAnalogValue( EAnalogStick::Left );
		Value.Y = -AnalogValue;
	}
	else if ( Key == EKeys::Gamepad_RightX )
	{
		FVector2D& Value = GetAnalogValue( EAnalogStick::Right );
		Value.X = AnalogValue;
	}
	else if ( Key == EKeys::Gamepad_RightY )
	{
		FVector2D& Value = GetAnalogValue( EAnalogStick::Right );
		Value.Y = -AnalogValue;
	}
	else 
	{
		return false;
	}

	return true;
}

bool FAnalogCursor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	return false;
}

void FAnalogCursor::SetAcceleration(float NewAcceleration)
{
	Acceleration = NewAcceleration;
}

void FAnalogCursor::SetMaxSpeed(float NewMaxSpeed)
{
	MaxSpeed = NewMaxSpeed;
}

void FAnalogCursor::SetStickySlowdown(float NewStickySlowdown)
{
	StickySlowdown = NewStickySlowdown;
}

void FAnalogCursor::SetDeadZone(float NewDeadZone)
{
	DeadZone = NewDeadZone;
}

void FAnalogCursor::SetMode(AnalogCursorMode::Type NewMode)
{
	Mode = NewMode;

	CurrentSpeed = FVector2D::ZeroVector;
}

void FAnalogCursor::ClearAnalogValues()
{
	AnalogValues[static_cast<uint8>(EAnalogStick::Left)] = FVector2D::ZeroVector;
	AnalogValues[static_cast<uint8>(EAnalogStick::Right)] = FVector2D::ZeroVector;
}

void FAnalogCursor::UpdateCursorPosition(FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor, const FVector2D& NewPosition)
{
	//grab the old position
	const FVector2D OldPosition = Cursor->GetPosition();

	//make sure we are actually moving
	if (OldPosition != NewPosition)
	{
		//put the cursor in the correct spot
		Cursor->SetPosition(NewPosition.X, NewPosition.Y);

		//create a new mouse event
		FPointerEvent MouseEvent(
			0,
			NewPosition,
			OldPosition,
			SlateApp.PressedMouseButtons,
			EKeys::Invalid,
			0,
			SlateApp.GetPlatformApplication()->GetModifierKeys()
			);

		
		//process the event
		SlateApp.ProcessMouseMoveEvent(MouseEvent);
	}
}

