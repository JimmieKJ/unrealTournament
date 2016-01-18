// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#define MAX_NUM_XINPUT_CONTROLLERS 4

/** Max number of controller buttons.  Must be < 256*/
#define MAX_NUM_CONTROLLER_BUTTONS 16

/**
 * Interface class for XInput devices (xbox 360 controller)                 
 */
class FWinRTInputInterface
{
public:

	static TSharedRef< FWinRTInputInterface > Create( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );


public:

	~FWinRTInputInterface() {}

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	/** Tick the interface (i.e check for new controllers) */
	void Tick( float DeltaTime );

	/**
	 * Poll for controller state and send events if needed
	 *
	 * @param PathToJoystickCaptureWidget	The path to the joystick capture widget.  If invalid this function does not poll 
	 */
	void SendControllerEvents();


private:

	FWinRTInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& MessageHandler );

	/**
	 * Scans for new or lost controllers                   
	 */
	void ConditionalScanForControllerChanges( float DeltaTime );

	/**
	 *	Scans for keyboard input
	 */
	void ConditionalScanForKeyboardChanges( float DeltaTime );

	/**
	 *	Convert the given keycode to the character it represents
	 */
	uint32 WinRTMapVirtualKeyToCharacter(uint32 InVirtualKey, bool bShiftIsDown);


private:

	struct FControllerState
	{
		/** Last frame's button states, so we only send events on edges */
		bool ButtonStates[MAX_NUM_CONTROLLER_BUTTONS];

		/** Next time a repeat event should be generated for each button */
		double NextRepeatTime[MAX_NUM_CONTROLLER_BUTTONS];

		/** Raw Left thumb x analog value */
		SHORT LeftXAnalog;

		/** Raw left thumb y analog value */
		SHORT LeftYAnalog;

		/** Raw Right thumb x analog value */
		SHORT RightXAnalog;

		/** Raw Right thumb x analog value */
		SHORT RightYAnalog;

		/** Left Trigger analog value */
		uint8 LeftTriggerAnalog;

		/** Right trigger analog value */
		uint8 RightTriggerAnalog;

		/** Id of the controller */
		int32 ControllerId;

		/** If the controller is currently connected */
		bool bIsConnected;

		/** If its been a while and we need to check this controllers connection */
		bool bCheckConnection;
	};


private:

	/** In the engine, all controllers map to xbox controllers for consistency */
	uint8 X360ToXboxControllerMapping[MAX_NUM_CONTROLLER_BUTTONS];

	/** Names of all the buttons */
	FGamepadKeyNames::Type Buttons[MAX_NUM_CONTROLLER_BUTTONS];

	/** Controller states */
	FControllerState ControllerStates[MAX_NUM_XINPUT_CONTROLLERS];

	/** Time before scanning for controller changes */
	double ControllerScanTime;

	/** Delay before sending a repeat message after a button was first pressed */
	float InitialButtonRepeatDelay;

	/** Delay before sendign a repeat message after a button has been pressed for a while */
	float ButtonRepeatDelay;

	/** The next controller to scan for changes (we do a round robin style scan) */
	int32 NextControllerToScan;

	TSharedRef< FGenericApplicationMessageHandler > MessageHandler;
};