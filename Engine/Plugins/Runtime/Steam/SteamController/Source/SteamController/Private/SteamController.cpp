// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SteamControllerPrivatePCH.h"

DEFINE_LOG_CATEGORY_STATIC(LogSteamController, Log, All);

#if WITH_STEAM_CONTROLLER

/** @todo - do something about this define */
#ifndef MAX_STEAM_CONTROLLERS
	#define MAX_STEAM_CONTROLLERS 8
#endif

/** Max number of controller buttons.  Must be < 256*/
#define MAX_NUM_CONTROLLER_BUTTONS 26

//
// Gamepad thresholds
//
#define LEFT_THUMBPAD_DEADZONE  0
#define RIGHT_THUMBPAD_DEADZONE 0

namespace
{
	inline float ShortToNormalizedFloat(short AxisVal)
	{
			// normalize [-32768..32767] -> [-1..1]
			const float Norm = (AxisVal <= 0 ? 32768.f : 32767.f);
			return float(AxisVal) / Norm;
	}
};

class FSteamController : public IInputDevice
{

public:

	FSteamController(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
		: MessageHandler(InMessageHandler)
	{
		// [RCL] 2015-01-23 FIXME: move to some other code than constructor so we can handle failures more gracefully
		if (SteamController() != nullptr)
		{
			FString PluginsDir = FPaths::EnginePluginsDir();
			FString ContentDir = FPaths::Combine(*PluginsDir, TEXT("Runtime"), TEXT("Steam"), TEXT("SteamController"), TEXT("Content"));
			FString VdfPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(*ContentDir, TEXT("Controller.vdf")));

			bool bInited = SteamController()->Init(TCHAR_TO_ANSI(*VdfPath));
			UE_LOG(LogSteamController, Log, TEXT("SteamController %s initialized with vdf file '%s'."), bInited ? TEXT("could not be") : TEXT("has been"), *VdfPath);

			// [RCL] 2014-05-05 FIXME: disable when could not init?
			if (bInited)
			{
				FMemory::MemZero(ControllerStates);
			}
		}
		else
		{
			UE_LOG(LogSteamController, Log, TEXT("SteamController is not available"));
		}

		InitialButtonRepeatDelay = 0.2f;
		ButtonRepeatDelay = 0.1f;

		// set up mapping
		Buttons[0] = EControllerButtons::FaceButtonBottom;
		Buttons[1] = EControllerButtons::FaceButtonRight;
		Buttons[2] = EControllerButtons::FaceButtonLeft;
		Buttons[3] = EControllerButtons::FaceButtonTop;
		Buttons[4] = EControllerButtons::LeftShoulder;
		Buttons[5] = EControllerButtons::RightShoulder;
		Buttons[6] = EControllerButtons::SpecialRight;
		Buttons[7] = EControllerButtons::SpecialLeft;
		Buttons[8] = EControllerButtons::LeftThumb;
		Buttons[9] = EControllerButtons::RightThumb;
		Buttons[10] = EControllerButtons::LeftTriggerThreshold;
		Buttons[11] = EControllerButtons::RightTriggerThreshold;
		Buttons[12] = EControllerButtons::Touch0;
		Buttons[13] = EControllerButtons::Touch1;
		Buttons[14] = EControllerButtons::Touch2;
		Buttons[15] = EControllerButtons::Touch3;
		Buttons[16] = EControllerButtons::LeftStickUp;
		Buttons[17] = EControllerButtons::LeftStickDown;
		Buttons[18] = EControllerButtons::LeftStickLeft;
		Buttons[19] = EControllerButtons::LeftStickRight;
		Buttons[20] = EControllerButtons::RightStickUp;
		Buttons[21] = EControllerButtons::RightStickDown;
		Buttons[22] = EControllerButtons::RightStickLeft;
		Buttons[23] = EControllerButtons::RightStickRight;
		Buttons[24] = EControllerButtons::BackLeft;
		Buttons[25] = EControllerButtons::BackRight;
	}

	virtual ~FSteamController()
	{
		if (SteamController() != nullptr)
		{
			SteamController()->Shutdown();
		}
	}

	virtual void Tick( float DeltaTime ) override
	{
	}

	virtual void SendControllerEvents() override
	{
		SteamControllerState_t ControllerState;

		if (SteamController() != nullptr)
		{
			const double CurrentTime = FPlatformTime::Seconds();

			for (uint32 ControllerIndex=0; ControllerIndex < MAX_STEAM_CONTROLLERS; ++ControllerIndex)
			{
				if (SteamController()->GetControllerState(ControllerIndex, &ControllerState))
				{
					if (ControllerState.unPacketNum != ControllerStates[ControllerIndex].PacketNum )
					{
						bool CurrentStates[MAX_NUM_CONTROLLER_BUTTONS] = {0};
			
						// Get the current state of all buttons
						CurrentStates[0] = !!(ControllerState.ulButtons & STEAM_BUTTON_3_MASK);
						CurrentStates[1] = !!(ControllerState.ulButtons & STEAM_BUTTON_1_MASK);
						CurrentStates[2] = !!(ControllerState.ulButtons & STEAM_BUTTON_2_MASK);
						CurrentStates[3] = !!(ControllerState.ulButtons & STEAM_BUTTON_0_MASK);
						CurrentStates[4] = !!(ControllerState.ulButtons & STEAM_LEFT_BUMPER_MASK);
						CurrentStates[5] = !!(ControllerState.ulButtons & STEAM_RIGHT_BUMPER_MASK);
						CurrentStates[6] = !!(ControllerState.ulButtons & STEAM_BUTTON_ESCAPE_MASK);
						CurrentStates[7] = !!(ControllerState.ulButtons & STEAM_BUTTON_MENU_MASK);
						CurrentStates[8] = !!(ControllerState.ulButtons & STEAM_BUTTON_LEFTPAD_CLICKED_MASK);
						CurrentStates[9] = !!(ControllerState.ulButtons & STEAM_BUTTON_RIGHTPAD_CLICKED_MASK);
						CurrentStates[10] = !!(ControllerState.ulButtons & STEAM_LEFT_TRIGGER_MASK);
						CurrentStates[11] = !!(ControllerState.ulButtons & STEAM_RIGHT_TRIGGER_MASK);
						CurrentStates[12] = !!(ControllerState.ulButtons & STEAM_TOUCH_0_MASK);
						CurrentStates[13] = !!(ControllerState.ulButtons & STEAM_TOUCH_1_MASK);
						CurrentStates[14] = !!(ControllerState.ulButtons & STEAM_TOUCH_2_MASK);
						CurrentStates[15] = !!(ControllerState.ulButtons & STEAM_TOUCH_3_MASK);
						CurrentStates[16] = !!(ControllerState.sLeftPadY > LEFT_THUMBPAD_DEADZONE);
						CurrentStates[17] = !!(ControllerState.sLeftPadY < -LEFT_THUMBPAD_DEADZONE);
						CurrentStates[18] = !!(ControllerState.sLeftPadX < -LEFT_THUMBPAD_DEADZONE);
						CurrentStates[19] = !!(ControllerState.sLeftPadX > LEFT_THUMBPAD_DEADZONE);
						CurrentStates[20] = !!(ControllerState.sRightPadY > RIGHT_THUMBPAD_DEADZONE);
						CurrentStates[21] = !!(ControllerState.sRightPadY < -RIGHT_THUMBPAD_DEADZONE);
						CurrentStates[22] = !!(ControllerState.sRightPadX < -RIGHT_THUMBPAD_DEADZONE);
						CurrentStates[23] = !!(ControllerState.sRightPadX > RIGHT_THUMBPAD_DEADZONE);
						CurrentStates[24] = !!(ControllerState.ulButtons & STEAM_BUTTON_BACK_LEFT_MASK);
						CurrentStates[25] = !!(ControllerState.ulButtons & STEAM_BUTTON_BACK_RIGHT_MASK);

						if (ControllerStates[ControllerIndex].LeftXAnalog != ControllerState.sLeftPadX)
						{
							MessageHandler->OnControllerAnalog(EControllerButtons::LeftAnalogX, ControllerIndex, ShortToNormalizedFloat(ControllerState.sLeftPadX));
							ControllerStates[ControllerIndex].LeftXAnalog = ControllerState.sLeftPadX;
						}

						if (ControllerStates[ControllerIndex].LeftYAnalog != ControllerState.sLeftPadY)
						{
							MessageHandler->OnControllerAnalog(EControllerButtons::LeftAnalogY, ControllerIndex, ShortToNormalizedFloat(ControllerState.sLeftPadY));
							ControllerStates[ControllerIndex].LeftYAnalog = ControllerState.sLeftPadY;
						}

						if (ControllerStates[ControllerIndex].RightXAnalog != ControllerState.sRightPadX)
						{
							MessageHandler->OnControllerAnalog(EControllerButtons::RightAnalogX, ControllerIndex, ShortToNormalizedFloat(ControllerState.sRightPadX));
							ControllerStates[ControllerIndex].RightXAnalog = ControllerState.sRightPadX;
						}

						if (ControllerStates[ControllerIndex].RightYAnalog != ControllerState.sRightPadY)
						{
							MessageHandler->OnControllerAnalog(EControllerButtons::RightAnalogY, ControllerIndex, ShortToNormalizedFloat(ControllerState.sRightPadY));
							ControllerStates[ControllerIndex].RightYAnalog = ControllerState.sRightPadY;
						}


						// For each button check against the previous state and send the correct message if any
						for (int32 ButtonIndex = 0; ButtonIndex < MAX_NUM_CONTROLLER_BUTTONS; ++ButtonIndex)
						{
							if (CurrentStates[ButtonIndex] != ControllerStates[ControllerIndex].ButtonStates[ButtonIndex])
							{
								if (CurrentStates[ButtonIndex])
								{
									MessageHandler->OnControllerButtonPressed(Buttons[ButtonIndex], ControllerIndex, false);
								}
								else
								{
									MessageHandler->OnControllerButtonReleased(Buttons[ButtonIndex], ControllerIndex, false);
								}

								if (CurrentStates[ButtonIndex] != 0)
								{
									// this button was pressed - set the button's NextRepeatTime to the InitialButtonRepeatDelay
									ControllerStates[ControllerIndex].NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
								}
							}

							// Update the state for next time
							ControllerStates[ControllerIndex].ButtonStates[ButtonIndex] = CurrentStates[ButtonIndex];
						}

						ControllerStates[ControllerIndex].PacketNum = ControllerState.unPacketNum;
					}
				}

				for (int32 ButtonIndex = 0; ButtonIndex < MAX_NUM_CONTROLLER_BUTTONS; ++ButtonIndex)
				{
					if (ControllerStates[ControllerIndex].ButtonStates[ButtonIndex] != 0 && ControllerStates[ControllerIndex].NextRepeatTime[ButtonIndex] <= CurrentTime)
					{
						MessageHandler->OnControllerButtonPressed(Buttons[ButtonIndex], ControllerIndex, true);

						// set the button's NextRepeatTime to the ButtonRepeatDelay
						ControllerStates[ControllerIndex].NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
					}
				}
			}
		}
	}

	void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override
	{
		// @todo: implement
	}

	void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &Values) override
	{
		// @todo: implement
	}

	virtual void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override
	{
		MessageHandler = InMessageHandler;
	}

	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		return false;
	}

private:

	struct FControllerState
	{
		/** If packet num matches that on your prior call, then the controller state hasn't been changed since 
		  * your last call and there is no need to process it.
		  */
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

	/** Mapping of controller buttons */
	EControllerButtons::Type Buttons[MAX_NUM_CONTROLLER_BUTTONS];

	/** handler to send all messages to */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;
};

#endif // WITH_STEAM_CONTROLLER

class FSteamControllerPlugin : public IInputDeviceModule
{
	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override
	{
#if WITH_STEAM_CONTROLLER
		// @todo: load vdf here
		return TSharedPtr< class IInputDevice >(new FSteamController(InMessageHandler));
#else
		return nullptr;
#endif
	}
};

IMPLEMENT_MODULE( FSteamControllerPlugin, SteamController)
