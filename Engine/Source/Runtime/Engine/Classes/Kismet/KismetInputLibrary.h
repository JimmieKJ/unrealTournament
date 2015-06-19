// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateCore.h"
typedef FKeyEvent FKeyEvent;
#include "KismetInputLibrary.generated.h"

UCLASS()
class ENGINE_API UKismetInputLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Calibrate the tilt for the input device */
	UFUNCTION(BlueprintCallable, Category="Input")
	static void CalibrateTilt();

	/**
	 * Test if the input key are equal (A == B)
	 * @param A - The key to compare against
	 * @param B - The key to compare
	 * @returns True if the key are equal, false otherwise
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Key)", CompactNodeTitle = "=="), Category="Utilities|Key")
	static bool EqualEqual_KeyKey(FKey A, FKey B);
	
	/**
	 * @returns True if the key is a modifier key: Ctrl, Command, Alt, Shift
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Key")
	static bool Key_IsModifierKey(const FKey& Key);
	
	/**
	 * @returns True if the key is a gamepad button
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Key")
	static bool Key_IsGamepadKey(const FKey& Key);
	
	/**
	 * @returns True if the key is a mouse button
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Key")
	static bool Key_IsMouseButton(const FKey& Key);
	
	/**
	 * @returns True if the key is a keyboard button
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Key")
	static bool Key_IsKeyboardKey(const FKey& Key);
	
	/**
	 * @returns True if the key is a float axis
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Key")
	static bool Key_IsFloatAxis(const FKey& Key);
	
	/**
	 * @returns True if the key is a vector axis
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Key")
	static bool Key_IsVectorAxis(const FKey& Key);
	
	/**
	 * @returns The display name of the key.
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Key")
	static FText Key_GetDisplayName(const FKey& Key);

	/**
	 * Returns whether or not this character is an auto-repeated keystroke
	 *
	 * @return  True if this character is a repeat
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsRepeat" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsRepeat(const FInputEvent& Input);

	/**
	 * Returns true if either shift key was down when this event occurred
	 *
	 * @return  True if shift is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsShiftDown" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsShiftDown(const FInputEvent& Input);

	/**
	 * Returns true if left shift key was down when this event occurred
	 *
	 * @return True if left shift is pressed.
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsLeftShiftDown" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsLeftShiftDown(const FInputEvent& Input);

	/**
	 * Returns true if right shift key was down when this event occurred
	 *
	 * @return True if right shift is pressed.
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsRightShiftDown" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsRightShiftDown(const FInputEvent& Input);

	/**
	 * Returns true if either control key was down when this event occurred
	 *
	 * @return  True if control is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsControlDown" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsControlDown(const FInputEvent& Input);

	/**
	 * Returns true if left control key was down when this event occurred
	 *
	 * @return  True if left control is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsLeftControlDown" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsLeftControlDown(const FInputEvent& Input);

	/**
	 * Returns true if left control key was down when this event occurred
	 *
	 * @return  True if left control is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsRightControlDown" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsRightControlDown(const FInputEvent& Input);

	/**
	 * Returns true if either alt key was down when this event occurred
	 *
	 * @return  True if alt is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsAltDown" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsAltDown(const FInputEvent& Input);

	/**
	 * Returns true if left alt key was down when this event occurred
	 *
	 * @return  True if left alt is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsLeftAltDown" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsLeftAltDown(const FInputEvent& Input);

	/**
	 * Returns true if right alt key was down when this event occurred
	 *
	 * @return  True if right alt is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsRightAltDown" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsRightAltDown(const FInputEvent& Input);

	/**
	 * Returns true if either command key was down when this event occurred
	 *
	 * @return  True if command is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsCommandDown" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsCommandDown(const FInputEvent& Input);

	/**
	 * Returns true if left command key was down when this event occurred
	 *
	 * @return  True if left command is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsLeftCommandDown" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsLeftCommandDown(const FInputEvent& Input);

	/**
	 * Returns true if right command key was down when this event occurred
	 *
	 * @return  True if right command is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsRightCommandDown" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsRightCommandDown(const FInputEvent& Input);


	/**
	 * Returns the key for this event.
	 *
	 * @return  Key name
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|KeyEvent")
	static FKey GetKey(const FKeyEvent& Input);

	UFUNCTION(BlueprintCallable, Category = "Utilities|KeyEvent")
	static int32 GetUserIndex(const FKeyEvent& Input);

	UFUNCTION(BlueprintCallable, Category = "Utilities|FAnalogInputEvent")
	static float GetAnalogValue(const FAnalogInputEvent& Input);

	/** @return The position of the cursor in screen space */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "GetScreenSpacePosition" ), Category="Utilities|PointerEvent")
	static FVector2D PointerEvent_GetScreenSpacePosition(const FPointerEvent& Input);

	/** @return The position of the cursor in screen space last time we handled an input event */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "GetLastScreenSpacePosition" ), Category="Utilities|PointerEvent")
	static FVector2D PointerEvent_GetLastScreenSpacePosition(const FPointerEvent& Input);

	/** @return the distance the mouse traveled since the last event was handled. */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "GetCursorDelta" ), Category="Utilities|PointerEvent")
	static FVector2D PointerEvent_GetCursorDelta(const FPointerEvent& Input);

	/** Mouse buttons that are currently pressed */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsMouseButtonDown" ), Category="Utilities|PointerEvent")
	static bool PointerEvent_IsMouseButtonDown(const FPointerEvent& Input, FKey MouseButton);

	/** Mouse button that caused this event to be raised (possibly EB_None) */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "GetEffectingButton" ), Category="Utilities|PointerEvent")
	static FKey PointerEvent_GetEffectingButton(const FPointerEvent& Input);

	/** How much did the mouse wheel turn since the last mouse event */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "GetWheelDelta" ), Category="Utilities|PointerEvent")
	static float PointerEvent_GetWheelDelta(const FPointerEvent& Input);

	/** @return The index of the user that caused the event */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "GetUserIndex" ), Category="Utilities|PointerEvent")
	static int32 PointerEvent_GetUserIndex(const FPointerEvent& Input);

	/** @return The unique identifier of the pointer (e.g., finger index) */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "GetPointerIndex" ), Category="Utilities|PointerEvent")
	static int32 PointerEvent_GetPointerIndex(const FPointerEvent& Input);

	/** @return The index of the touch pad that generated this event (for platforms with multiple touch pads per user) */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "GetTouchpadIndex" ), Category="Utilities|PointerEvent")
	static int32 PointerEvent_GetTouchpadIndex(const FPointerEvent& Input);

	/** @return Is this event a result from a touch (as opposed to a mouse) */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "IsTouchEvent" ), Category="Utilities|PointerEvent")
	static bool PointerEvent_IsTouchEvent(const FPointerEvent& Input);

	//TODO UMG Support GetGestureType()

	///** @return The type of touch gesture */
	//UFUNCTION(BlueprintPure, meta=( DisplayName = "GetGestureType" ), Category="Utilities|PointerEvent")
	//static EGestureEvent::Type PointerEvent_GetGestureType(const FPointerEvent& Input);

	/** @return The change in gesture value since the last gesture event of the same type. */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "GetGestureDelta" ), Category="Utilities|PointerEvent")
	static FVector2D PointerEvent_GetGestureDelta(const FPointerEvent& Input);


	/** @return The controller button that caused this event */
	UFUNCTION(BlueprintPure, meta = (DeprecatedFunction, DeprecationMessage = "Use GetKey() for KeyEvent instead", DisplayName = "GetEffectingButton"), Category = "Utilities|ControllerEvent")
	static FKey ControllerEvent_GetEffectingButton(const FControllerEvent& Input);

	/** @return The index of the user that caused the event */
	UFUNCTION(BlueprintPure, meta = (DeprecatedFunction, DeprecationMessage = "Use GetUserIndex() for KeyEvent instead"), Category = "Utilities|ControllerEvent")
	static int32 ControllerEvent_GetUserIndex(const FControllerEvent& Input);

	/** @return Analog value between 0 and 1.  1 being fully pressed, 0 being not pressed at all */
	UFUNCTION(BlueprintPure, meta = (DeprecatedFunction, DeprecationMessage = "Use GetAnalogValue() for AnalogInputEvent instead"), Category = "Utilities|ControllerEvent")
	static float ControllerEvent_GetAnalogValue(const FControllerEvent& Input);
};
