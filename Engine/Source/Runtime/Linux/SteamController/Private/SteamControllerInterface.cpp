// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Engine.h"
#include "steam/steam_api.h"
#include "SteamControllerInterface.h"
#include "GenericApplication.h"

//
// Gamepad thresholds
//
#define LEFT_THUMBPAD_DEADZONE  0
#define RIGHT_THUMBPAD_DEADZONE 0

IMPLEMENT_MODULE( FDefaultModuleImpl, SteamController );
DEFINE_LOG_CATEGORY(LogSteamController);

TSharedRef< SteamControllerInterface > SteamControllerInterface::Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	return MakeShareable( new SteamControllerInterface( InMessageHandler ) );
}

SteamControllerInterface::SteamControllerInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
	: MessageHandler( InMessageHandler )
{
	if(SteamController() != NULL)
	{
		FString vdfPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::GeneratedConfigDir(), TEXT("controller.vdf")));
		bool bInited = SteamController()->Init(TCHAR_TO_ANSI(*vdfPath));
		UE_LOG(LogSteamController, Log, TEXT("SteamController %s initialized with vdf file '%s'."), bInited ? TEXT("could not be") : TEXT("has been"), *vdfPath);

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

SteamControllerInterface::~SteamControllerInterface()
{
	if(SteamController() != NULL)
	{
		SteamController()->Shutdown();
	}
}

float ShortToNormalizedFloat(short AxisVal)
{
	// normalize [-32768..32767] -> [-1..1]
	const float Norm = (AxisVal <= 0 ? 32768.f : 32767.f);
	return float(AxisVal) / Norm;
}

void SteamControllerInterface::SendControllerEvents()
{
	SteamControllerState_t controllerState;

	if(SteamController() != NULL)
	{
		const double CurrentTime = FPlatformTime::Seconds();

		for( uint32 i=0; i < MAX_STEAM_CONTROLLERS; ++i )
		{
			if( SteamController()->GetControllerState( i, &controllerState ) )
			{
				if(controllerState.unPacketNum != ControllerStates[i].PacketNum )
				{
					bool CurrentStates[MAX_NUM_CONTROLLER_BUTTONS] = {0};
		
					// Get the current state of all buttons
					CurrentStates[0] = !!(controllerState.ulButtons & STEAM_BUTTON_3_MASK);
					CurrentStates[1] = !!(controllerState.ulButtons & STEAM_BUTTON_1_MASK);
					CurrentStates[2] = !!(controllerState.ulButtons & STEAM_BUTTON_2_MASK);
					CurrentStates[3] = !!(controllerState.ulButtons & STEAM_BUTTON_0_MASK);
					CurrentStates[4] = !!(controllerState.ulButtons & STEAM_LEFT_BUMPER_MASK);
					CurrentStates[5] = !!(controllerState.ulButtons & STEAM_RIGHT_BUMPER_MASK);
					CurrentStates[6] = !!(controllerState.ulButtons & STEAM_BUTTON_ESCAPE_MASK);
					CurrentStates[7] = !!(controllerState.ulButtons & STEAM_BUTTON_MENU_MASK);
					CurrentStates[8] = !!(controllerState.ulButtons & STEAM_BUTTON_LEFTPAD_CLICKED_MASK);
					CurrentStates[9] = !!(controllerState.ulButtons & STEAM_BUTTON_RIGHTPAD_CLICKED_MASK);
					CurrentStates[10] = !!(controllerState.ulButtons & STEAM_LEFT_TRIGGER_MASK);
					CurrentStates[11] = !!(controllerState.ulButtons & STEAM_RIGHT_TRIGGER_MASK);
					CurrentStates[12] = !!(controllerState.ulButtons & STEAM_TOUCH_0_MASK);
					CurrentStates[13] = !!(controllerState.ulButtons & STEAM_TOUCH_1_MASK);
					CurrentStates[14] = !!(controllerState.ulButtons & STEAM_TOUCH_2_MASK);
					CurrentStates[15] = !!(controllerState.ulButtons & STEAM_TOUCH_3_MASK);
					CurrentStates[16] = !!(controllerState.sLeftPadY > LEFT_THUMBPAD_DEADZONE);
					CurrentStates[17] = !!(controllerState.sLeftPadY < -LEFT_THUMBPAD_DEADZONE);
					CurrentStates[18] = !!(controllerState.sLeftPadX < -LEFT_THUMBPAD_DEADZONE);
					CurrentStates[19] = !!(controllerState.sLeftPadX > LEFT_THUMBPAD_DEADZONE);
					CurrentStates[20] = !!(controllerState.sRightPadY > RIGHT_THUMBPAD_DEADZONE);
					CurrentStates[21] = !!(controllerState.sRightPadY < -RIGHT_THUMBPAD_DEADZONE);
					CurrentStates[22] = !!(controllerState.sRightPadX < -RIGHT_THUMBPAD_DEADZONE);
					CurrentStates[23] = !!(controllerState.sRightPadX > RIGHT_THUMBPAD_DEADZONE);
					CurrentStates[24] = !!(controllerState.ulButtons & STEAM_BUTTON_BACK_LEFT_MASK);
					CurrentStates[25] = !!(controllerState.ulButtons & STEAM_BUTTON_BACK_RIGHT_MASK);

					if( ControllerStates[i].LeftXAnalog != controllerState.sLeftPadX )
					{
						MessageHandler->OnControllerAnalog( EControllerButtons::LeftAnalogX, i, ShortToNormalizedFloat(controllerState.sLeftPadX) );
						ControllerStates[i].LeftXAnalog = controllerState.sLeftPadX;
					}

					if( ControllerStates[i].LeftYAnalog != controllerState.sLeftPadY )
					{
						MessageHandler->OnControllerAnalog( EControllerButtons::LeftAnalogY, i, ShortToNormalizedFloat(controllerState.sLeftPadY) );
						ControllerStates[i].LeftYAnalog = controllerState.sLeftPadY;
					}

					if( ControllerStates[i].RightXAnalog != controllerState.sRightPadX )
					{
						MessageHandler->OnControllerAnalog( EControllerButtons::RightAnalogX, i, ShortToNormalizedFloat(controllerState.sRightPadX) );
						ControllerStates[i].RightXAnalog = controllerState.sRightPadX;
					}

					if( ControllerStates[i].RightYAnalog != controllerState.sRightPadY )
					{
						MessageHandler->OnControllerAnalog( EControllerButtons::RightAnalogY, i, ShortToNormalizedFloat(controllerState.sRightPadY) );
						ControllerStates[i].RightYAnalog = controllerState.sRightPadY;
					}


					// For each button check against the previous state and send the correct message if any
					for (int32 ButtonIndex = 0; ButtonIndex < MAX_NUM_CONTROLLER_BUTTONS; ++ButtonIndex)
					{
						if (CurrentStates[ButtonIndex] != ControllerStates[i].ButtonStates[ButtonIndex])
						{
							if( CurrentStates[ButtonIndex] )
							{
								MessageHandler->OnControllerButtonPressed( Buttons[ButtonIndex], i, false );
							}
							else
							{
								MessageHandler->OnControllerButtonReleased( Buttons[ButtonIndex], i, false );
							}

							if ( CurrentStates[ButtonIndex] != 0 )
							{
								// this button was pressed - set the button's NextRepeatTime to the InitialButtonRepeatDelay
								ControllerStates[i].NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
							}
						}

						// Update the state for next time
						ControllerStates[i].ButtonStates[ButtonIndex] = CurrentStates[ButtonIndex];
					}

					ControllerStates[i].PacketNum = controllerState.unPacketNum;
				}
			}

			for (int32 ButtonIndex = 0; ButtonIndex < MAX_NUM_CONTROLLER_BUTTONS; ++ButtonIndex)
			{
				if ( ControllerStates[i].ButtonStates[ButtonIndex] != 0 && ControllerStates[i].NextRepeatTime[ButtonIndex] <= CurrentTime )
				{
					MessageHandler->OnControllerButtonPressed( Buttons[ButtonIndex], i, true );

					// set the button's NextRepeatTime to the ButtonRepeatDelay
					ControllerStates[i].NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
				}
			}
		}
	}
}

void SteamControllerInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;
}