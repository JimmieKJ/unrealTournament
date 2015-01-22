// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	 GestureRecognizer - handles detecting when gestures happen
=============================================================================*/

#pragma once

class FGestureRecognizer
{
public:
	/** Attempt to detect touch gestures */
	void DetectGestures(const FVector (&Touches)[EKeys::NUM_TOUCH_KEYS], class UPlayerInput* PlayerInput, float DeltaTime);

protected:
	/** Internal processing of gestures */
	void HandleGesture(class UPlayerInput* PlayerInput, FKey Gesture, bool bStarted, bool bEnded);
	
	/** A mapping of a gesture to it's current value (how far swiped, pinch amount, etc) */
	TMap<FKey, float> CurrentGestureValues;

	/** Special gesture tracking values */
	FVector2D AnchorPoints[EKeys::NUM_TOUCH_KEYS];
	bool bIsReadyForPinch;
	float AnchorDistance;
	bool bIsReadyForFlick;
	FVector2D FlickCurrent;
	float FlickTime;
	int32 PreviousTouchCount;
};



