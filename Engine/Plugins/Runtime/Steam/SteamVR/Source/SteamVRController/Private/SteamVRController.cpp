// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SteamVRControllerPrivatePCH.h"


DEFINE_LOG_CATEGORY_STATIC(LogSteamVRController, Log, All);

/** @todo steamvr - do something about this define */
#ifndef MAX_STEAMVR_CONTROLLERS
	#define MAX_STEAMVR_CONTROLLERS 8
#endif

/** Max number of controller buttons.  Must be < 256*/
#define MAX_NUM_CONTROLLER_BUTTONS 9

/** Controller axis mappings. @todo steamvr: should enumerate rather than hard code */
#define TOUCHPAD_AXIS	0
#define TRIGGER_AXIS	1

//
// Gamepad thresholds
//
#define TOUCHPAD_DEADZONE  0.0f


class FSteamVRController : public IInputDevice
{

public:

	FSteamVRController(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
		: MessageHandler(InMessageHandler),
		  SteamVRPlugin(nullptr)
	{
		FMemory::Memzero(ControllerStates, sizeof(ControllerStates));

		for (int32 i=0; i < vr::k_unMaxTrackedDeviceCount; ++i)
		{
			DeviceToControllerMap[i] = -1;
		}

		for (int32 i=0; i < MAX_STEAMVR_CONTROLLERS; ++i)
		{
			ControllerToDeviceMap[i] = -1;
		}

		NumControllersMapped = 0;

		InitialButtonRepeatDelay = 0.2f;
		ButtonRepeatDelay = 0.1f;

		// set up mapping	//@todo steamvr: these are all presses, do we want to map touches as well?

		Buttons[0] = EControllerButtons::SpecialLeft;				// system
		Buttons[1] = EControllerButtons::SpecialRight;				// application menu
		Buttons[2] = EControllerButtons::LeftThumb;					// touchpad press
		Buttons[3] = EControllerButtons::LeftTriggerThreshold;		// trigger press
		Buttons[4] = EControllerButtons::BackLeft;					// grip
		Buttons[5] = EControllerButtons::LeftStickUp;				// touchpad d-pad up
		Buttons[6] = EControllerButtons::LeftStickDown;				// touchpad d-pad down
		Buttons[7] = EControllerButtons::LeftStickLeft;				// touchpad d-pad left
		Buttons[8] = EControllerButtons::LeftStickRight;			// touchpad d-pad right
	}

	virtual ~FSteamVRController()
	{
	}

	virtual void Tick( float DeltaTime ) override
	{
	}

	virtual void SendControllerEvents() override
	{
		vr::VRControllerState_t ControllerState;

		vr::IVRSystem* VRSystem = GetVRSystem();

		if (VRSystem != nullptr)
		{
			const double CurrentTime = FPlatformTime::Seconds();

			for (uint32 DeviceIndex=0; DeviceIndex < vr::k_unMaxTrackedDeviceCount; ++DeviceIndex)
			{
				// skip non-controllers
				if (VRSystem->GetTrackedDeviceClass(DeviceIndex) != vr::TrackedDeviceClass_Controller)
				{
					continue;
				}

				// update the mappings if this is a new device
				if (DeviceToControllerMap[DeviceIndex] == -1)
				{
					// don't map too many controllers
					if (NumControllersMapped >= MAX_STEAMVR_CONTROLLERS)
					{
						continue;
					}

					DeviceToControllerMap[DeviceIndex] = NumControllersMapped;
					ControllerToDeviceMap[NumControllersMapped] = DeviceIndex;
					++NumControllersMapped;

					// update the SteamVR plugin with the new mapping
					SteamVRPlugin->SetControllerToDeviceMap(ControllerToDeviceMap);
				}

				// get the controller index for this device
				int32 ControllerIndex = DeviceToControllerMap[DeviceIndex];

				if (VRSystem->GetControllerState(DeviceIndex, &ControllerState))
				{
					if (ControllerState.unPacketNum != ControllerStates[ControllerIndex].PacketNum )
					{
						bool CurrentStates[MAX_NUM_CONTROLLER_BUTTONS] = {0};
			
						// Get the current state of all buttons
						CurrentStates[0] = !!(ControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_System));
						CurrentStates[1] = !!(ControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu));
						CurrentStates[2] = !!(ControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad));
						CurrentStates[3] = !!(ControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger));
						CurrentStates[4] = !!(ControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip));
						CurrentStates[5] = !!(ControllerState.rAxis[TOUCHPAD_AXIS].y > TOUCHPAD_DEADZONE);
						CurrentStates[6] = !!(ControllerState.rAxis[TOUCHPAD_AXIS].y < -TOUCHPAD_DEADZONE);
						CurrentStates[7] = !!(ControllerState.rAxis[TOUCHPAD_AXIS].x < -TOUCHPAD_DEADZONE);
						CurrentStates[8] = !!(ControllerState.rAxis[TOUCHPAD_AXIS].x > TOUCHPAD_DEADZONE);

						if (ControllerStates[ControllerIndex].TouchPadXAnalog != ControllerState.rAxis[TOUCHPAD_AXIS].x)
						{
							MessageHandler->OnControllerAnalog(EControllerButtons::LeftAnalogX, ControllerIndex, ControllerState.rAxis[TOUCHPAD_AXIS].x);
							ControllerStates[ControllerIndex].TouchPadXAnalog = ControllerState.rAxis[TOUCHPAD_AXIS].x;
						}

						if (ControllerStates[ControllerIndex].TouchPadYAnalog != ControllerState.rAxis[TOUCHPAD_AXIS].y)
						{
							MessageHandler->OnControllerAnalog(EControllerButtons::LeftAnalogY, ControllerIndex, ControllerState.rAxis[TOUCHPAD_AXIS].y);
							ControllerStates[ControllerIndex].TouchPadYAnalog = ControllerState.rAxis[TOUCHPAD_AXIS].y;
						}

						if (ControllerStates[ControllerIndex].TriggerAnalog != ControllerState.rAxis[TRIGGER_AXIS].x)
						{
							MessageHandler->OnControllerAnalog(EControllerButtons::LeftTriggerAnalog, ControllerIndex, ControllerState.rAxis[TRIGGER_AXIS].x);
							ControllerStates[ControllerIndex].TriggerAnalog = ControllerState.rAxis[TRIGGER_AXIS].x;
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
		// Skip unless this is the left large channel, which we consider to be the only SteamVRController feedback channel
		if (ChannelType != FForceFeedbackChannelType::LEFT_LARGE)
		{
			return;
		}

		if ((ControllerId >= 0) && (ControllerId < MAX_STEAMVR_CONTROLLERS))
		{
			FControllerState& ControllerState = ControllerStates[ControllerId];

			ControllerState.VibeValues.LeftLarge = Value;

			UpdateVibration(ControllerId);
		}
	}

	void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &Values) override
	{
		if ((ControllerId >= 0) && (ControllerId < MAX_STEAMVR_CONTROLLERS))
		{
			FControllerState& ControllerState = ControllerStates[ControllerId];
			ControllerState.VibeValues = Values;

			UpdateVibration(ControllerId);
		}
	}

	void UpdateVibration(int32 ControllerId)
	{
		// make sure there is a valid device for the controllerid
		int32 DeviceIndex = ControllerToDeviceMap[ControllerId];
		if (DeviceIndex < 0)
		{
			return;
		}

		const FControllerState& ControllerState = ControllerStates[ControllerId];
		vr::IVRSystem* VRSystem = GetVRSystem();

		if (VRSystem == nullptr)
		{
			return;
		}

		// Map the float values from [0,1] to be more reasonable values for the SteamController.  The docs say that [100,2000] are reasonable values
 		const float LeftIntensity = FMath::Clamp(ControllerState.VibeValues.LeftLarge * 2000.f, 0.f, 2000.f);
		if (LeftIntensity > 0.f)
		{
			VRSystem->TriggerHapticPulse(DeviceIndex, TOUCHPAD_AXIS, LeftIntensity);
		}
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

	inline vr::IVRSystem* GetVRSystem()
	{
		if (SteamVRPlugin == nullptr)
		{
			SteamVRPlugin = &FModuleManager::LoadModuleChecked<ISteamVRPlugin>(TEXT("SteamVR"));
			if (SteamVRPlugin == nullptr)
			{
				return nullptr;
			}
		}

		return SteamVRPlugin->GetVRSystem();
	}

	struct FControllerState
	{
		/** If packet num matches that on your prior call, then the controller state hasn't been changed since 
		  * your last call and there is no need to process it.
		  */
		uint32 PacketNum;

		/** touchpad analog values */
		float TouchPadXAnalog;
		float TouchPadYAnalog;

		/** trigger analog value */
		float TriggerAnalog;

		/** Last frame's button states, so we only send events on edges */
		bool ButtonStates[MAX_NUM_CONTROLLER_BUTTONS];

		/** Next time a repeat event should be generated for each button */
		double NextRepeatTime[MAX_NUM_CONTROLLER_BUTTONS];

		/** Values for force feedback on this controller.  We only consider the LEFT_LARGE channel for SteamControllers */
		FForceFeedbackValues VibeValues;
	};

	/** Mappings between tracked devices and 0 indexed controllers */
	int32 NumControllersMapped;
	int32 ControllerToDeviceMap[MAX_STEAMVR_CONTROLLERS];
	int32 DeviceToControllerMap[vr::k_unMaxTrackedDeviceCount];

	/** Controller states */
	FControllerState ControllerStates[MAX_STEAMVR_CONTROLLERS];

	/** Delay before sending a repeat message after a button was first pressed */
	float InitialButtonRepeatDelay;

	/** Delay before sending a repeat message after a button has been pressed for a while */
	float ButtonRepeatDelay;

	/** Mapping of controller buttons */
	EControllerButtons::Type Buttons[MAX_NUM_CONTROLLER_BUTTONS];

	/** handler to send all messages to */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	/** the SteamVR plugin module */
	ISteamVRPlugin* SteamVRPlugin;

	/** weak pointer to the IVRSystem owned by the HMD module */
	TWeakPtr<vr::IVRSystem> HMDVRSystem;
};


class FSteamVRControllerPlugin : public IInputDeviceModule
{
	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override
	{
		return TSharedPtr< class IInputDevice >(new FSteamVRController(InMessageHandler));

	}
};

IMPLEMENT_MODULE( FSteamVRControllerPlugin, SteamVRController)
