// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SteamController.h: Unreal SteamController interface object.
=============================================================================*/

#pragma  once 

#ifndef MAX_STEAM_CONTROLLERS
	#define MAX_STEAM_CONTROLLERS 8
#endif

/** Max number of controller buttons.  Must be < 256*/
#define MAX_NUM_CONTROLLER_BUTTONS 26

DECLARE_LOG_CATEGORY_EXTERN(LogSteamController, Log, All);

/**
 * Interface class for XInput devices (xbox 360 controller)                 
 */
class SteamControllerInterface
{
public:
	~SteamControllerInterface();

	static TSharedRef< SteamControllerInterface > Create( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	/**
	 * Poll for controller state and send events if needed
	 *
	 * @param PathToJoystickCaptureWidget	The path to the joystick capture widget.  If invalid this function does not poll 
	 */
	void SendControllerEvents();

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

private:

	SteamControllerInterface( const TSharedRef< FGenericApplicationMessageHandler >& MessageHandler );

	struct FControllerState
	{
		// If packet num matches that on your prior call, then the controller state hasn't been changed since 
		// your last call and there is no need to process it
		uint32 PacketNum;

		/** Raw Left thumb x analog value */
		short LeftXAnalog;

		/** Raw left thumb y analog value */
		short LeftYAnalog;

		/** Raw Right thumb x analog value */
		short RightXAnalog;

		/** Raw Right thumb x analog value */
		short RightYAnalog;

		/** Last frame's button states, so we only send events on edges */
		bool ButtonStates[MAX_NUM_CONTROLLER_BUTTONS];

		/** Next time a repeat event should be generated for each button */
		double NextRepeatTime[MAX_NUM_CONTROLLER_BUTTONS];
	}; 

	/** Controller states */
	FControllerState ControllerStates[MAX_STEAM_CONTROLLERS];

	/** Delay before sending a repeat message after a button was first pressed */
	float InitialButtonRepeatDelay;

	/** Delay before sending a repeat message after a button has been pressed for a while */
	float ButtonRepeatDelay;

	/**  */
	EControllerButtons::Type Buttons[MAX_NUM_CONTROLLER_BUTTONS];

	/**  */
	TSharedRef< FGenericApplicationMessageHandler > MessageHandler;
};