// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMotionController.h"
#include "InputCoreTypes.h"

#if USE_OVR_MOTION_SDK

/**
 * Digital buttons on the SteamVR controller
 */
enum class EOculusTouchControllerButton
{
	// NOTE: The Trigger and Grip digital buttons are synthetic.  Oculus hardware doesn't support a digital press for these
	Trigger,
	Grip,

	XA,
	YB,
	Thumbstick,

	/** Total number of controller buttons */
	TotalButtonCount
};

enum class EOculusTouchCapacitiveAxes
{
	Thumbstick,
	Trigger,
	XA,
	YB,
	IndexPointing,
	ThumbUp,
	
	/** Total number of capacitive axes */
	TotalAxisCount
};

struct FOculusTouchCapacitiveKey
{
	static const FKey OculusTouch_Left_Thumbstick;
	static const FKey OculusTouch_Left_Trigger;
	static const FKey OculusTouch_Left_FaceButton1; // X or A
	static const FKey OculusTouch_Left_FaceButton2; // Y or B
	static const FKey OculusTouch_Left_IndexPointing;
	static const FKey OculusTouch_Left_ThumbUp;

	static const FKey OculusTouch_Right_Thumbstick;
	static const FKey OculusTouch_Right_Trigger;
	static const FKey OculusTouch_Right_FaceButton1; // X or A
	static const FKey OculusTouch_Right_FaceButton2; // Y or B
	static const FKey OculusTouch_Right_IndexPointing;
	static const FKey OculusTouch_Right_ThumbUp;
};

struct FOculusTouchCapacitiveKeyNames
{
	typedef FName Type;

	static const FName OculusTouch_Left_Thumbstick;
	static const FName OculusTouch_Left_Trigger;
	static const FName OculusTouch_Left_FaceButton1; // X or A
	static const FName OculusTouch_Left_FaceButton2; // Y or B
	static const FName OculusTouch_Left_IndexPointing;
	static const FName OculusTouch_Left_ThumbUp;

	static const FName OculusTouch_Right_Thumbstick;
	static const FName OculusTouch_Right_Trigger;
	static const FName OculusTouch_Right_FaceButton1; // X or A
	static const FName OculusTouch_Right_FaceButton2; // Y or B
	static const FName OculusTouch_Right_IndexPointing;
	static const FName OculusTouch_Right_ThumbUp;
};

/**
 * Digital button state
 */
struct FOculusButtonState
{
	/** The Unreal button this maps to.  Different depending on whether this is the Left or Right hand controller */
	FName Key;

	/** Whether we're pressed or not.  While pressed, we will generate repeat presses on a timer */
	bool bIsPressed;

	/** Next time a repeat event should be generated for each button */
	double NextRepeatTime;


	/** Default constructor that just sets sensible defaults */
	FOculusButtonState()
		: Key( NAME_None ),
		  bIsPressed( false ),
		  NextRepeatTime( 0.0 )
	{
	}
};

/**
 * Capacitive Axis State
 */
struct FOculusTouchCapacitiveState
{
	/** The axis that this button state maps to */
	FName Axis;

	/** How close the finger is to this button, from 0.f to 1.f */
	float State;

	FOculusTouchCapacitiveState()
		: Axis(NAME_None)
		, State(0.f)
	{
	}
};

/**
 * Input state for an Oculus motion controller
 */
struct FOculusTouchControllerState
{
	/** True if the device is connected and actively being tracked, otherwise false */
	bool bIsCurrentlyTracked;

	/** Location of the controller in the local tracking space */
	FVector Location;

	/** Orientation of the controller in the local tracking space */
	FQuat Orientation;

	/** Analog trigger */
	float TriggerAxis;

	/** Grip trigger */
	float GripAxis;

	/** Thumbstick */
	FVector2D ThumbstickAxes;

	/** Button states */
	FOculusButtonState Buttons[ EOculusTouchControllerButton::TotalButtonCount ];

	/** Capacitive Touch axes */
	FOculusTouchCapacitiveState CapacitiveAxes[EOculusTouchCapacitiveAxes::TotalAxisCount];

	/** Whether or not we're playing a haptic effect.  If true, force feedback calls will be early-outed in favor of the haptic effect */
	bool bPlayingHapticEffect;

	/** Haptic frequency (zero to disable) */
	float HapticFrequency;

	/** Haptic amplitude (zero to disable) */
	float HapticAmplitude;


	/** Explicit constructor sets up sensible defaults */
	FOculusTouchControllerState( const EControllerHand Hand )
		: bIsCurrentlyTracked( false ),
		  Location( FVector::ZeroVector ),
		  Orientation( FQuat::Identity ),
		  TriggerAxis( 0.0f ),
		  GripAxis( 0.0f ),
		  ThumbstickAxes( FVector2D::ZeroVector ),
		  bPlayingHapticEffect( false ),
		  HapticFrequency( 0.0f ),
		  HapticAmplitude( 0.0f )
	{
		for( FOculusButtonState& Button : Buttons )
		{
			Button.bIsPressed = false;
			Button.NextRepeatTime = 0.0;
		}
		
		Buttons[ (int32)EOculusTouchControllerButton::Trigger ].Key = ( Hand == EControllerHand::Left ) ? FGamepadKeyNames::MotionController_Left_Trigger : FGamepadKeyNames::MotionController_Right_Trigger;
		Buttons[ (int32)EOculusTouchControllerButton::Grip ].Key = ( Hand == EControllerHand::Left ) ? FGamepadKeyNames::MotionController_Left_Grip1 : FGamepadKeyNames::MotionController_Right_Grip1;
		Buttons[ (int32)EOculusTouchControllerButton::Thumbstick ].Key = ( Hand == EControllerHand::Left ) ? FGamepadKeyNames::MotionController_Left_Thumbstick : FGamepadKeyNames::MotionController_Right_Thumbstick;
		Buttons[ (int32)EOculusTouchControllerButton::XA ].Key = (Hand == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_FaceButton1 : FGamepadKeyNames::MotionController_Right_FaceButton1;
		Buttons[ (int32)EOculusTouchControllerButton::YB ].Key = (Hand == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_FaceButton2 : FGamepadKeyNames::MotionController_Right_FaceButton2;

		CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::Thumbstick].Axis = (Hand == EControllerHand::Left) ? FOculusTouchCapacitiveKeyNames::OculusTouch_Left_Thumbstick : FOculusTouchCapacitiveKeyNames::OculusTouch_Right_Thumbstick;
		CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::Trigger].Axis = (Hand == EControllerHand::Left) ? FOculusTouchCapacitiveKeyNames::OculusTouch_Left_Trigger : FOculusTouchCapacitiveKeyNames::OculusTouch_Right_Trigger;
		CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::XA].Axis = (Hand == EControllerHand::Left) ? FOculusTouchCapacitiveKeyNames::OculusTouch_Left_FaceButton1 : FOculusTouchCapacitiveKeyNames::OculusTouch_Right_FaceButton1;
		CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::YB].Axis = (Hand == EControllerHand::Left) ? FOculusTouchCapacitiveKeyNames::OculusTouch_Left_FaceButton2 : FOculusTouchCapacitiveKeyNames::OculusTouch_Right_FaceButton2;
		CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::IndexPointing].Axis = (Hand == EControllerHand::Left) ? FOculusTouchCapacitiveKeyNames::OculusTouch_Left_IndexPointing : FOculusTouchCapacitiveKeyNames::OculusTouch_Right_IndexPointing;
		CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::ThumbUp].Axis = (Hand == EControllerHand::Left) ? FOculusTouchCapacitiveKeyNames::OculusTouch_Left_ThumbUp : FOculusTouchCapacitiveKeyNames::OculusTouch_Right_ThumbUp;
	}

	/** Default constructor does nothing.  Don't use it.  This only exists because we cannot initialize an array of objects with no default constructor on non-C++ 11 compliant compilers (VS 2013) */
	FOculusTouchControllerState()
	{
	}
};

#endif	// USE_OVR_MOTION_SDK