// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "AndroidInputInterface.h"
#include "GenericApplicationMessageHandler.h"
#include <android/input.h>


TArray<TouchInput> FAndroidInputInterface::TouchInputStack = TArray<TouchInput>();
FCriticalSection FAndroidInputInterface::TouchInputCriticalSection;

FAndroidControllerData FAndroidInputInterface::OldControllerData[MAX_NUM_CONTROLLERS];
FAndroidControllerData FAndroidInputInterface::NewControllerData[MAX_NUM_CONTROLLERS];

EControllerButtons::Type FAndroidInputInterface::ButtonMapping[MAX_NUM_CONTROLLER_BUTTONS];
float FAndroidInputInterface::InitialButtonRepeatDelay;
float FAndroidInputInterface::ButtonRepeatDelay;

FDeferredAndroidMessage FAndroidInputInterface::DeferredMessages[MAX_DEFERRED_MESSAGE_QUEUE_SIZE];
int32 FAndroidInputInterface::DeferredMessageQueueLastEntryIndex = 0;
int32 FAndroidInputInterface::DeferredMessageQueueDroppedCount   = 0;

TArray<FAndroidInputInterface::MotionData> FAndroidInputInterface::MotionDataStack
	= TArray<FAndroidInputInterface::MotionData>();


TSharedRef< FAndroidInputInterface > FAndroidInputInterface::Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	return MakeShareable( new FAndroidInputInterface( InMessageHandler ) );
}

FAndroidInputInterface::~FAndroidInputInterface()
{
}

FAndroidInputInterface::FAndroidInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
	: MessageHandler( InMessageHandler )
{
	ButtonMapping[0] = EControllerButtons::FaceButtonBottom;
	ButtonMapping[1] = EControllerButtons::FaceButtonRight;
	ButtonMapping[2] = EControllerButtons::FaceButtonLeft;
	ButtonMapping[3] = EControllerButtons::FaceButtonTop;
	ButtonMapping[4] = EControllerButtons::LeftShoulder;
	ButtonMapping[5] = EControllerButtons::RightShoulder;
	ButtonMapping[6] = EControllerButtons::SpecialRight;
	ButtonMapping[7] = EControllerButtons::SpecialLeft;
	ButtonMapping[8] = EControllerButtons::LeftThumb;
	ButtonMapping[9] = EControllerButtons::RightThumb;
	ButtonMapping[10] = EControllerButtons::LeftTriggerThreshold;
	ButtonMapping[11] = EControllerButtons::RightTriggerThreshold;
	ButtonMapping[12] = EControllerButtons::DPadUp;
	ButtonMapping[13] = EControllerButtons::DPadDown;
	ButtonMapping[14] = EControllerButtons::DPadLeft;
	ButtonMapping[15] = EControllerButtons::DPadRight;
	ButtonMapping[16] = EControllerButtons::AndroidBack;  // Technically just an alias for SpecialLeft
	ButtonMapping[17] = EControllerButtons::AndroidVolumeUp;
	ButtonMapping[18] = EControllerButtons::AndroidVolumeDown;
	ButtonMapping[19] = EControllerButtons::AndroidMenu;  // Technically just an alias for SpecialRightNewControllerData[deviceId].ButtonStates[6] = buttonDown; 

	InitialButtonRepeatDelay = 0.2f;
	ButtonRepeatDelay = 0.1f;
}

void FAndroidInputInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;
}

void FAndroidInputInterface::Tick(float DeltaTime)
{


}

void FAndroidInputInterface::SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	// For now, force the device to 0
	// Should use Java to enumerate number of controllers and assign device ID
	// to controller number
	ControllerId = 0;

	// Note: only one motor on Android at the moment, but remember all the settings
	// update will look at combination of all values to pick state

	// Save a copy of the value for future comparison
	switch (ChannelType)
	{
		case FForceFeedbackChannelType::LEFT_LARGE:
			NewControllerData[ControllerId].VibeValues.LeftLarge = Value;
			break;

		case FForceFeedbackChannelType::LEFT_SMALL:
			NewControllerData[ControllerId].VibeValues.LeftSmall = Value;
			break;

		case FForceFeedbackChannelType::RIGHT_LARGE:
			NewControllerData[ControllerId].VibeValues.RightLarge = Value;
			break;

		case FForceFeedbackChannelType::RIGHT_SMALL:
			NewControllerData[ControllerId].VibeValues.RightSmall = Value;
			break;

		default:
			// Unknown channel, so ignore it
			break;
	}

	// Update with the latest values (wait for SendControllerEvents later?)
	UpdateVibeMotors(NewControllerData[ControllerId]);
}

void FAndroidInputInterface::SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values)
{
	// For now, force the device to 0
	// Should use Java to enumerate number of controllers and assign device ID
	// to controller number
	ControllerId = 0;

	// Note: only one motor on Android at the moment, but remember all the settings
	// update will look at combination of all values to pick state

	NewControllerData[ControllerId].VibeValues = Values;

	// Update with the latest values (wait for SendControllerEvents later?)
	UpdateVibeMotors(NewControllerData[ControllerId]);
}

extern void AndroidThunkCpp_Vibrate(int32 Duration);

void FAndroidInputInterface::UpdateVibeMotors(FAndroidControllerData &State)
{
	// Use largest vibration state as value
	float MaxLeft = State.VibeValues.LeftLarge > State.VibeValues.LeftSmall ? State.VibeValues.LeftLarge : State.VibeValues.LeftSmall;
	float MaxRight = State.VibeValues.RightLarge > State.VibeValues.RightSmall ? State.VibeValues.RightLarge : State.VibeValues.RightSmall;
	float Value = MaxLeft > MaxRight ? MaxLeft : MaxRight;

	if (State.VibeIsOn)
	{
		// Turn it off if below threshold
		if (Value < 0.3f)
		{
			AndroidThunkCpp_Vibrate(0);
			State.VibeIsOn = false;
		}
	}
	else {
		if (Value >= 0.3f)
		{
			// Turn it on for 10 seconds (or until below threshold)
			AndroidThunkCpp_Vibrate(10000);
			State.VibeIsOn = true;
		}
	}
}

static uint32 CharMap[] =
{
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    L'0',
    L'1',
    L'2',
    L'3',
    L'4',
    L'5',
    L'6',
    L'7',
    L'8',
    L'9',
    L'*',
	L'#',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    L'a',
    L'b',
    L'c',
    L'd',
    L'e',
    L'f',
    L'g',
    L'h',
    L'i',
    L'j',
    L'k',
    L'l',
    L'm',
    L'n',
    L'o',
    L'p',
    L'q',
    L'r',
    L's',
    L't',
    L'u',
    L'v',
    L'w',
    L'x',
    L'y',
    L'z',
    L',',
    L'.',
    0,
    0,
    0,
    0,
    L'\t',
    L' ',
    0,
    0,
    0,
    L'\n',
    0,
    L'`',
    L'-',
    L'=',
    L'[',
    L']',
    L'\\',
    L';',
    L'\'',
    L'/',
    L'@',
    0,
    0,
    0,   // *Camera* focus
    L'+',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    L'0',
    L'1',
    L'2',
    L'3',
    L'4',
    L'5',
    L'6',
    L'7',
    L'8',
    L'9',
    L'/',
    L'*',
    L'-',
    L'+',
    L'.',
    L',',
    L'\n',
    L'=',
    L'(',
    L')',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

static uint32 CharMapShift[] =
{
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	L')',
	L'!',
	L'@',
	L'#',
	L'$',
	L'%',
	L'^',
	L'&',
	L'*',
	L'(',
	L'*',
	L'#',
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	L'A',
	L'B',
	L'C',
	L'D',
	L'E',
	L'F',
	L'G',
	L'H',
	L'I',
	L'J',
	L'K',
	L'L',
	L'M',
	L'N',
	L'O',
	L'P',
	L'Q',
	L'R',
	L'S',
	L'T',
	L'U',
	L'V',
	L'W',
	L'X',
	L'Y',
	L'Z',
	L'<',
	L'>',
	0,
	0,
	0,
	0,
	L'\t',
	L' ',
	0,
	0,
	0,
	L'\n',
	0,
	L'~',
	L'_',
	L'+',
	L'{',
	L'}',
	L'|',
	L':',
	L'\"',
	L'?',
	L'@',
	0,
	0,
	0,   // *Camera* focus
	L'+',
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	L'0',
	L'1',
	L'2',
	L'3',
	L'4',
	L'5',
	L'6',
	L'7',
	L'8',
	L'9',
	L'/',
	L'*',
	L'-',
	L'+',
	L'.',
	L',',
	L'\n',
	L'=',
	L'(',
	L')',
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

void FAndroidInputInterface::SendControllerEvents()
{
	FScopeLock Lock(&TouchInputCriticalSection);

	for(int i = 0; i < FAndroidInputInterface::TouchInputStack.Num(); ++i)
	{
		TouchInput Touch = FAndroidInputInterface::TouchInputStack[i];

		// send input to handler
		if (Touch.Type == TouchBegan)
		{
			MessageHandler->OnTouchStarted( NULL, Touch.Position, Touch.Handle, 0);
		}
		else if (Touch.Type == TouchEnded)
		{
			MessageHandler->OnTouchEnded(Touch.Position, Touch.Handle, 0);
		}
		else
		{
			MessageHandler->OnTouchMoved(Touch.Position, Touch.Handle, 0);
		}
	}

	// Extract differences in new and old states and send messages
	for (int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_CONTROLLERS; ControllerIndex++)
	{
		FAndroidControllerData& OldControllerState = OldControllerData[ControllerIndex];
		FAndroidControllerData& NewControllerState = NewControllerData[ControllerIndex];

		if (NewControllerState.LXAnalog != OldControllerState.LXAnalog)
		{
			MessageHandler->OnControllerAnalog(EControllerButtons::LeftAnalogX, NewControllerState.DeviceId, NewControllerState.LXAnalog);
		}
		if (NewControllerState.LYAnalog != OldControllerState.LYAnalog)
		{
			//LOGD("    Sending updated LeftAnalogY value of %f", NewControllerState.LYAnalog);
			MessageHandler->OnControllerAnalog(EControllerButtons::LeftAnalogY, NewControllerState.DeviceId, NewControllerState.LYAnalog);
		}
		if (NewControllerState.RXAnalog != OldControllerState.RXAnalog)
		{
			//LOGD("    Sending updated RightAnalogX value of %f", NewControllerState.RXAnalog);
			MessageHandler->OnControllerAnalog(EControllerButtons::RightAnalogX, NewControllerState.DeviceId, NewControllerState.RXAnalog);
		}
		if (NewControllerState.RYAnalog != OldControllerState.RYAnalog)
		{
			//LOGD("    Sending updated RightAnalogY value of %f", NewControllerState.RYAnalog);
			MessageHandler->OnControllerAnalog(EControllerButtons::RightAnalogY, NewControllerState.DeviceId, NewControllerState.RYAnalog);
		}
		if (NewControllerState.LTAnalog != OldControllerState.LTAnalog)
		{
			//LOGD("    Sending updated LeftTriggerAnalog value of %f", NewControllerState.LTAnalog);
			MessageHandler->OnControllerAnalog(EControllerButtons::LeftTriggerAnalog, NewControllerState.DeviceId, NewControllerState.LTAnalog);

			// Handle the trigger theshold "virtual" button state
			//check(ButtonMapping[10] == EControllerButtons::LeftTriggerThreshold);
			NewControllerState.ButtonStates[10] = NewControllerState.LTAnalog >= 0.1f;
		}
		if (NewControllerState.RTAnalog != OldControllerState.RTAnalog)
		{
			//LOGD("    Sending updated RightTriggerAnalog value of %f", NewControllerState.RTAnalog);
			MessageHandler->OnControllerAnalog(EControllerButtons::RightTriggerAnalog, NewControllerState.DeviceId, NewControllerState.RTAnalog);

			// Handle the trigger theshold "virtual" button state
			//check(ButtonMapping[11] == EControllerButtons::RightTriggerThreshold);
			NewControllerState.ButtonStates[11] = NewControllerState.RTAnalog >= 0.1f;
		}

		const double CurrentTime = FPlatformTime::Seconds();

		// For each button check against the previous state and send the correct message if any
		for (int32 ButtonIndex = 0; ButtonIndex < MAX_NUM_CONTROLLER_BUTTONS; ButtonIndex++)
		{
			if (NewControllerState.ButtonStates[ButtonIndex] != OldControllerState.ButtonStates[ButtonIndex])
			{
				if (NewControllerState.ButtonStates[ButtonIndex])
				{
					//LOGD("    Sending joystick button down %d (first)", ButtonMapping[ButtonIndex]);
					MessageHandler->OnControllerButtonPressed(ButtonMapping[ButtonIndex], NewControllerState.DeviceId, false);
				}
				else
				{
					//LOGD("    Sending joystick button up %d", ButtonMapping[ButtonIndex]);
					MessageHandler->OnControllerButtonReleased(ButtonMapping[ButtonIndex], NewControllerState.DeviceId, false);
				}

				if (NewControllerState.ButtonStates[ButtonIndex])
				{
					// This button was pressed - set the button's NextRepeatTime to the InitialButtonRepeatDelay
					NewControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
				}
			}
			else if (NewControllerState.ButtonStates[ButtonIndex] && NewControllerState.NextRepeatTime[ButtonIndex] <= CurrentTime)
			{
				// Send button repeat events
				MessageHandler->OnControllerButtonPressed(ButtonMapping[ButtonIndex], NewControllerState.DeviceId, true);

				// Set the button's NextRepeatTime to the ButtonRepeatDelay
				NewControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
			}
		}

		// Update the state for next time
		OldControllerState = NewControllerState;
	}

	for (int i = 0; i < FAndroidInputInterface::MotionDataStack.Num(); ++i)
	{
		MotionData motion_data = FAndroidInputInterface::MotionDataStack[i];

		MessageHandler->OnMotionDetected(
			motion_data.Tilt, motion_data.RotationRate,
			motion_data.Gravity, motion_data.Acceleration,
			0);
	}

	for (int32 MessageIndex = 0; MessageIndex < FMath::Min(DeferredMessageQueueLastEntryIndex, MAX_DEFERRED_MESSAGE_QUEUE_SIZE); ++MessageIndex)
	{
		const FDeferredAndroidMessage& DeferredMessage = DeferredMessages[MessageIndex];
		const int32 Char = DeferredMessage.KeyEventData.modifier & AMETA_SHIFT_ON ? CharMapShift[DeferredMessage.KeyEventData.keyId] : CharMap[DeferredMessage.KeyEventData.keyId];
		
		switch (DeferredMessage.messageType)
		{

			case MessageType_KeyDown:

				MessageHandler->OnKeyDown(DeferredMessage.KeyEventData.keyId, Char, DeferredMessage.KeyEventData.isRepeat);
				MessageHandler->OnKeyChar(Char,  DeferredMessage.KeyEventData.isRepeat);
				break;

			case MessageType_KeyUp:

				MessageHandler->OnKeyUp(DeferredMessage.KeyEventData.keyId, Char, false);
				break;
		} 
	}

	if (DeferredMessageQueueDroppedCount)
	{
		//should warn that messages got dropped, which message queue?
		DeferredMessageQueueDroppedCount = 0;
	}

	DeferredMessageQueueLastEntryIndex = 0;

	FAndroidInputInterface::TouchInputStack.Empty(0);

	FAndroidInputInterface::MotionDataStack.Empty();
}

void FAndroidInputInterface::QueueTouchInput(TArray<TouchInput> InTouchEvents)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	FAndroidInputInterface::TouchInputStack.Append(InTouchEvents);
}

void FAndroidInputInterface::JoystickAxisEvent(int32 deviceId, int32 axisId, float axisValue)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	// For now, force the device to 0
	// Should use Java to enumerate number of controllers and assign device ID
	// to controller number
	deviceId = 0;

	// Apply a small dead zone to the analog sticks
	const float deadZone = 0.2f;
	switch (axisId)
	{
		// Also, invert Y and RZ to match what the engine expects
		case AMOTION_EVENT_AXIS_X:        NewControllerData[deviceId].LXAnalog =  axisValue; break;
		case AMOTION_EVENT_AXIS_Y:        NewControllerData[deviceId].LYAnalog = -axisValue; break;
		case AMOTION_EVENT_AXIS_Z:        NewControllerData[deviceId].RXAnalog =  axisValue; break;
		case AMOTION_EVENT_AXIS_RZ:       NewControllerData[deviceId].RYAnalog = -axisValue; break;
		case AMOTION_EVENT_AXIS_LTRIGGER: NewControllerData[deviceId].LTAnalog =  axisValue; break;
		case AMOTION_EVENT_AXIS_RTRIGGER: NewControllerData[deviceId].RTAnalog =  axisValue; break;
	}
}

void FAndroidInputInterface::JoystickButtonEvent(int32 deviceId, int32 buttonId, bool buttonDown)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	// For now, force the device to 0
	// Should use Java to enumerate number of controllers and assign device ID
	// to controller number
	deviceId = 0;

	switch (buttonId)
	{
		case AKEYCODE_BUTTON_A:
		case AKEYCODE_DPAD_CENTER:   NewControllerData[deviceId].ButtonStates[0] = buttonDown; break;
		case AKEYCODE_BUTTON_B:      NewControllerData[deviceId].ButtonStates[1] = buttonDown; break;
		case AKEYCODE_BUTTON_X:      NewControllerData[deviceId].ButtonStates[2] = buttonDown; break;
		case AKEYCODE_BUTTON_Y:      NewControllerData[deviceId].ButtonStates[3] = buttonDown; break;
		case AKEYCODE_BUTTON_L1:     NewControllerData[deviceId].ButtonStates[4] = buttonDown; break;
		case AKEYCODE_BUTTON_R1:     NewControllerData[deviceId].ButtonStates[5] = buttonDown; break;
		case AKEYCODE_BUTTON_START:
		case AKEYCODE_MENU:          NewControllerData[deviceId].ButtonStates[6] = buttonDown; NewControllerData[deviceId].ButtonStates[19] = buttonDown;  break;
		case AKEYCODE_BUTTON_SELECT: 
		case AKEYCODE_BACK:          NewControllerData[deviceId].ButtonStates[7] = buttonDown; NewControllerData[deviceId].ButtonStates[16] = buttonDown;  break;
		case AKEYCODE_BUTTON_THUMBL: NewControllerData[deviceId].ButtonStates[8] = buttonDown; break;
		case AKEYCODE_BUTTON_THUMBR: NewControllerData[deviceId].ButtonStates[9] = buttonDown; break;
		case AKEYCODE_BUTTON_L2:     NewControllerData[deviceId].ButtonStates[10] = buttonDown; break;
		case AKEYCODE_BUTTON_R2:     NewControllerData[deviceId].ButtonStates[11] = buttonDown; break;
		case AKEYCODE_DPAD_UP:       NewControllerData[deviceId].ButtonStates[12] = buttonDown; break;
		case AKEYCODE_DPAD_DOWN:     NewControllerData[deviceId].ButtonStates[13] = buttonDown; break;
		case AKEYCODE_DPAD_LEFT:     NewControllerData[deviceId].ButtonStates[14] = buttonDown; break;
		case AKEYCODE_DPAD_RIGHT:    NewControllerData[deviceId].ButtonStates[15] = buttonDown; break;
	}
}

void FAndroidInputInterface::DeferMessage(const FDeferredAndroidMessage& DeferredMessage)
{
	FScopeLock Lock(&TouchInputCriticalSection);
	// Get the index we should be writing to
	int32 Index = DeferredMessageQueueLastEntryIndex++;

	if (Index >= MAX_DEFERRED_MESSAGE_QUEUE_SIZE)
	{
		// Actually, if the queue is full, drop the message and increment a counter of drops
		DeferredMessageQueueDroppedCount++;
		return;
	}
	DeferredMessages[Index] = DeferredMessage;
}

void FAndroidInputInterface::QueueMotionData(const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	FAndroidInputInterface::MotionDataStack.Push(
		MotionData { Tilt, RotationRate, Gravity, Acceleration });
}
