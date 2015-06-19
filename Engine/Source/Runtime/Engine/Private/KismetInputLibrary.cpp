// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Kismet/KismetInputLibrary.h"
#include "MessageLog.h"
#include "UObjectToken.h"


//////////////////////////////////////////////////////////////////////////
// UKismetInputLibrary

#define LOCTEXT_NAMESPACE "KismetInputLibrary"


UKismetInputLibrary::UKismetInputLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UKismetInputLibrary::CalibrateTilt()
{
	GEngine->Exec(NULL, TEXT("CALIBRATEMOTION"));
}


bool UKismetInputLibrary::EqualEqual_KeyKey(FKey A, FKey B)
{
	return A == B;
}

bool UKismetInputLibrary::Key_IsModifierKey(const FKey& Key)
{
	return Key.IsModifierKey();
}

bool UKismetInputLibrary::Key_IsGamepadKey(const FKey& Key)
{
	return Key.IsGamepadKey();
}

bool UKismetInputLibrary::Key_IsMouseButton(const FKey& Key)
{
	return Key.IsMouseButton();
}

bool UKismetInputLibrary::Key_IsKeyboardKey(const FKey& Key)
{
	return Key.IsBindableInBlueprints() && (Key.IsGamepadKey() == false && Key.IsMouseButton() == false);
}

bool UKismetInputLibrary::Key_IsFloatAxis(const FKey& Key)
{
	return Key.IsFloatAxis();
}

bool UKismetInputLibrary::Key_IsVectorAxis(const FKey& Key)
{
	return Key.IsVectorAxis();
}

FText UKismetInputLibrary::Key_GetDisplayName(const FKey& Key)
{
	return Key.GetDisplayName();
}

bool UKismetInputLibrary::InputEvent_IsRepeat(const FInputEvent& Input)
{
	return Input.IsRepeat();
}

bool UKismetInputLibrary::InputEvent_IsShiftDown(const FInputEvent& Input)
{
	return Input.IsShiftDown();
}

bool UKismetInputLibrary::InputEvent_IsLeftShiftDown(const FInputEvent& Input)
{
	return Input.IsLeftShiftDown();
}

bool UKismetInputLibrary::InputEvent_IsRightShiftDown(const FInputEvent& Input)
{
	return Input.IsRightShiftDown();
}

bool UKismetInputLibrary::InputEvent_IsControlDown(const FInputEvent& Input)
{
	return Input.IsControlDown();
}

bool UKismetInputLibrary::InputEvent_IsLeftControlDown(const FInputEvent& Input)
{
	return Input.IsLeftControlDown();
}

bool UKismetInputLibrary::InputEvent_IsRightControlDown(const FInputEvent& Input)
{
	return Input.IsRightControlDown();
}

bool UKismetInputLibrary::InputEvent_IsAltDown(const FInputEvent& Input)
{
	return Input.IsAltDown();
}

bool UKismetInputLibrary::InputEvent_IsLeftAltDown(const FInputEvent& Input)
{
	return Input.IsLeftAltDown();
}

bool UKismetInputLibrary::InputEvent_IsRightAltDown(const FInputEvent& Input)
{
	return Input.IsRightAltDown();
}

bool UKismetInputLibrary::InputEvent_IsCommandDown(const FInputEvent& Input)
{
	return Input.IsCommandDown();
}

bool UKismetInputLibrary::InputEvent_IsLeftCommandDown(const FInputEvent& Input)
{
	return Input.IsLeftCommandDown();
}

bool UKismetInputLibrary::InputEvent_IsRightCommandDown(const FInputEvent& Input)
{
	return Input.IsRightCommandDown();
}


FKey UKismetInputLibrary::GetKey(const FKeyEvent& Input)
{
	return Input.GetKey();
}

int32 UKismetInputLibrary::GetUserIndex(const FKeyEvent& Input)
{
	return Input.GetUserIndex();
}

float UKismetInputLibrary::GetAnalogValue(const FAnalogInputEvent& Input)
{
	return Input.GetAnalogValue();
}


FVector2D UKismetInputLibrary::PointerEvent_GetScreenSpacePosition(const FPointerEvent& Input)
{
	return Input.GetScreenSpacePosition();
}

FVector2D UKismetInputLibrary::PointerEvent_GetLastScreenSpacePosition(const FPointerEvent& Input)
{
	return Input.GetLastScreenSpacePosition();
}

FVector2D UKismetInputLibrary::PointerEvent_GetCursorDelta(const FPointerEvent& Input)
{
	return Input.GetCursorDelta();
}

bool UKismetInputLibrary::PointerEvent_IsMouseButtonDown(const FPointerEvent& Input, FKey MouseButton)
{
	return Input.IsMouseButtonDown(MouseButton);
}

FKey UKismetInputLibrary::PointerEvent_GetEffectingButton(const FPointerEvent& Input)
{
	return Input.GetEffectingButton();
}

float UKismetInputLibrary::PointerEvent_GetWheelDelta(const FPointerEvent& Input)
{
	return Input.GetWheelDelta();
}

int32 UKismetInputLibrary::PointerEvent_GetUserIndex(const FPointerEvent& Input)
{
	return Input.GetUserIndex();
}

int32 UKismetInputLibrary::PointerEvent_GetPointerIndex(const FPointerEvent& Input)
{
	return Input.GetPointerIndex();
}

int32 UKismetInputLibrary::PointerEvent_GetTouchpadIndex(const FPointerEvent& Input)
{
	return Input.GetTouchpadIndex();
}

bool UKismetInputLibrary::PointerEvent_IsTouchEvent(const FPointerEvent& Input)
{
	return Input.IsTouchEvent();
}

//EGestureEvent::Type UKismetInputLibrary::PointerEvent_GetGestureType(const FPointerEvent& Input)
//{
//	return Input.GetGestureType();
//}

FVector2D UKismetInputLibrary::PointerEvent_GetGestureDelta(const FPointerEvent& Input)
{
	return Input.GetGestureDelta();
}


FKey UKismetInputLibrary::ControllerEvent_GetEffectingButton(const FControllerEvent& Input)
{
	return Input.GetEffectingButton();
}

int32 UKismetInputLibrary::ControllerEvent_GetUserIndex(const FControllerEvent& Input)
{
	return Input.GetUserIndex();
}

float UKismetInputLibrary::ControllerEvent_GetAnalogValue(const FControllerEvent& Input)
{
	return Input.GetAnalogValue();
}

#undef LOCTEXT_NAMESPACE