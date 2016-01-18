// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SteamVRControllerPrivatePCH.h"
#include "ISteamVRControllerPlugin.h"
#include "IMotionController.h"
#include "../../SteamVR/Private/SteamVRHMD.h"

DEFINE_LOG_CATEGORY_STATIC(LogSteamVRController, Log, All);

/** Total number of controllers in a set */
#define CONTROLLERS_PER_PLAYER	2

/** Controller axis mappings. @todo steamvr: should enumerate rather than hard code */
#define TOUCHPAD_AXIS	0
#define TRIGGER_AXIS	1
#define DOT_45DEG		0.7071f

//
// Gamepad thresholds
//
#define TOUCHPAD_DEADZONE  0.0f

// Controls whether or not we need to swap the input routing for the hands, for debugging
static TAutoConsoleVariable<int32> CVarSwapHands(
	TEXT("vr.SwapMotionControllerInput"),
	0,
	TEXT("This command allows you to swap the button / axis input handedness for the input controller, for debugging purposes.\n")
	TEXT(" 0: don't swap (default)\n")
	TEXT(" 1: swap left and right buttons"),
	ECVF_Cheat);

namespace SteamVRControllerKeyNames
{
	const FGamepadKeyNames::Type Touch0("Steam_Touch_0");
	const FGamepadKeyNames::Type Touch1("Steam_Touch_1");
}



class FSteamVRController : public IInputDevice, public IMotionController
{
	FSteamVRHMD* GetSteamVRHMD() const
	{
		if (GEngine->HMDDevice.IsValid() && (GEngine->HMDDevice->GetHMDDeviceType() == EHMDDeviceType::DT_SteamVR))
		{
			return static_cast<FSteamVRHMD*>(GEngine->HMDDevice.Get());
		}

		return nullptr;
	}

public:

	/** The maximum number of Unreal controllers.  Each Unreal controller represents a pair of motion controller devices */
	static const int32 MaxUnrealControllers = MAX_STEAMVR_CONTROLLER_PAIRS;

	/** Total number of motion controllers we'll support */
	static const int32 MaxControllers = MaxUnrealControllers * 2;

	/**
	 * Buttons on the SteamVR controller
	 */
	struct ESteamVRControllerButton
	{
		enum Type
		{
			System,
			ApplicationMenu,
			TouchPadPress,
			TouchPadTouch,
			TriggerPress,
			Grip,
			TouchPadUp,
			TouchPadDown,
			TouchPadLeft,
			TouchPadRight,

			/** Max number of controller buttons.  Must be < 256 */
			TotalButtonCount
		};
	};


	FSteamVRController(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
		: MessageHandler(InMessageHandler),
		  SteamVRPlugin(nullptr)
	{
		FMemory::Memzero(ControllerStates, sizeof(ControllerStates));

		for (int32 i=0; i < vr::k_unMaxTrackedDeviceCount; ++i)
		{
			DeviceToControllerMap[i] = INDEX_NONE;
		}

		for (int32 i=0; i < MaxControllers; ++i)
		{
			ControllerToDeviceMap[i] = INDEX_NONE;
		}

		NumControllersMapped = 0;

		InitialButtonRepeatDelay = 0.2f;
		ButtonRepeatDelay = 0.1f;

		Buttons[ (int32)EControllerHand::Left ][ ESteamVRControllerButton::System ] = FGamepadKeyNames::SpecialLeft;
		Buttons[ (int32)EControllerHand::Left ][ ESteamVRControllerButton::ApplicationMenu ] = FGamepadKeyNames::MotionController_Left_Shoulder;
		Buttons[ (int32)EControllerHand::Left ][ ESteamVRControllerButton::TouchPadPress ] = FGamepadKeyNames::MotionController_Left_Thumbstick;
		Buttons[ (int32)EControllerHand::Left ][ ESteamVRControllerButton::TouchPadTouch ] = SteamVRControllerKeyNames::Touch0;
		Buttons[ (int32)EControllerHand::Left ][ ESteamVRControllerButton::TriggerPress ] = FGamepadKeyNames::MotionController_Left_Trigger;
		Buttons[ (int32)EControllerHand::Left ][ ESteamVRControllerButton::Grip ] = FGamepadKeyNames::MotionController_Left_Grip1;
		Buttons[ (int32)EControllerHand::Left ][ ESteamVRControllerButton::TouchPadUp ] = FGamepadKeyNames::MotionController_Left_FaceButton1;
		Buttons[ (int32)EControllerHand::Left ][ ESteamVRControllerButton::TouchPadDown ] = FGamepadKeyNames::MotionController_Left_FaceButton3;
		Buttons[ (int32)EControllerHand::Left ][ ESteamVRControllerButton::TouchPadLeft ] = FGamepadKeyNames::MotionController_Left_FaceButton4;
		Buttons[ (int32)EControllerHand::Left ][ ESteamVRControllerButton::TouchPadRight ] = FGamepadKeyNames::MotionController_Left_FaceButton2;

		Buttons[ (int32)EControllerHand::Right ][ ESteamVRControllerButton::System ] = FGamepadKeyNames::SpecialRight;
		Buttons[ (int32)EControllerHand::Right ][ ESteamVRControllerButton::ApplicationMenu ] = FGamepadKeyNames::MotionController_Right_Shoulder;
		Buttons[ (int32)EControllerHand::Right ][ ESteamVRControllerButton::TouchPadPress ] = FGamepadKeyNames::MotionController_Right_Thumbstick;
		Buttons[ (int32)EControllerHand::Right ][ ESteamVRControllerButton::TouchPadTouch ] = SteamVRControllerKeyNames::Touch1;
		Buttons[ (int32)EControllerHand::Right ][ ESteamVRControllerButton::TriggerPress ] = FGamepadKeyNames::MotionController_Right_Trigger;
		Buttons[ (int32)EControllerHand::Right ][ ESteamVRControllerButton::Grip ] = FGamepadKeyNames::MotionController_Right_Grip1;
		Buttons[ (int32)EControllerHand::Right ][ ESteamVRControllerButton::TouchPadUp ] = FGamepadKeyNames::MotionController_Right_FaceButton1;
		Buttons[ (int32)EControllerHand::Right ][ ESteamVRControllerButton::TouchPadDown ] = FGamepadKeyNames::MotionController_Right_FaceButton3;
		Buttons[ (int32)EControllerHand::Right ][ ESteamVRControllerButton::TouchPadLeft ] = FGamepadKeyNames::MotionController_Right_FaceButton4;
		Buttons[ (int32)EControllerHand::Right ][ ESteamVRControllerButton::TouchPadRight ] = FGamepadKeyNames::MotionController_Right_FaceButton2;

		IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);

		//@todo:  fix this.  construction of the controller happens after InitializeMotionControllers(), so we manually insert into the array here.
		GEngine->MotionControllerDevices.AddUnique(this);
	}

	virtual ~FSteamVRController()
	{
		GEngine->MotionControllerDevices.Remove(this);
	}

	virtual void Tick( float DeltaTime ) override
	{
	}

	virtual void SendControllerEvents() override
	{
		vr::VRControllerState_t VRControllerState;

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
				if (DeviceToControllerMap[DeviceIndex] == INDEX_NONE )
				{
					// don't map too many controllers
					if (NumControllersMapped >= MaxControllers)
					{
						continue;
					}

					DeviceToControllerMap[DeviceIndex] = FMath::FloorToInt(NumControllersMapped / CONTROLLERS_PER_PLAYER);
					ControllerToDeviceMap[NumControllersMapped] = DeviceIndex;
					ControllerStates[DeviceIndex].Hand = (EControllerHand)(NumControllersMapped % CONTROLLERS_PER_PLAYER);
					++NumControllersMapped;

					// update the SteamVR plugin with the new mapping
					{
						int32 UnrealControllerIdAndHandToDeviceIdMap[MaxUnrealControllers][CONTROLLERS_PER_PLAYER];
						for( int32 UnrealControllerIndex = 0; UnrealControllerIndex < MaxUnrealControllers; ++UnrealControllerIndex )
						{
							for (int32 HandIndex = 0; HandIndex < CONTROLLERS_PER_PLAYER; ++HandIndex)
							{
								UnrealControllerIdAndHandToDeviceIdMap[ UnrealControllerIndex ][ HandIndex ] = ControllerToDeviceMap[ UnrealControllerIdToControllerIndex( UnrealControllerIndex, (EControllerHand)HandIndex ) ];
							}
						}
						SteamVRPlugin->SetUnrealControllerIdAndHandToDeviceIdMap( UnrealControllerIdAndHandToDeviceIdMap );
					}
				}

				// get the controller index for this device
				int32 ControllerIndex = DeviceToControllerMap[DeviceIndex];
				FControllerState& ControllerState = ControllerStates[ DeviceIndex ];
				EControllerHand HandToUse = ControllerState.Hand;

				// check to see if we need to swap input hands for debugging
				static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.SwapMotionControllerInput"));
				bool bSwapHandInput= (CVar->GetValueOnGameThread() != 0) ? true : false;
				if(bSwapHandInput)
				{
					HandToUse = (HandToUse == EControllerHand::Left) ? EControllerHand::Right : EControllerHand::Left; 
				}

				if (VRSystem->GetControllerState(DeviceIndex, &VRControllerState))
				{
					if (VRControllerState.unPacketNum != ControllerState.PacketNum )
					{
						bool CurrentStates[ ESteamVRControllerButton::TotalButtonCount ] = {0};

						// Get the current state of all buttons
						CurrentStates[ ESteamVRControllerButton::System ] = !!(VRControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_System));
						CurrentStates[ ESteamVRControllerButton::ApplicationMenu ] = !!(VRControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu));
						CurrentStates[ ESteamVRControllerButton::TouchPadPress ] = !!(VRControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad));
						CurrentStates[ ESteamVRControllerButton::TouchPadTouch ] = !!( VRControllerState.ulButtonTouched & vr::ButtonMaskFromId( vr::k_EButton_SteamVR_Touchpad ) );
						CurrentStates[ ESteamVRControllerButton::TriggerPress ] = !!(VRControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger));
						CurrentStates[ ESteamVRControllerButton::Grip ] = !!(VRControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip));

						// If the touchpad isn't currently pressed or touched, zero put both of the axes
						if (!CurrentStates[ ESteamVRControllerButton::TouchPadTouch ])
						{
 							VRControllerState.rAxis[TOUCHPAD_AXIS].y = 0.0f;
 							VRControllerState.rAxis[TOUCHPAD_AXIS].x = 0.0f;
						}

						// D-pad emulation
						const FVector2D TouchDir = FVector2D(VRControllerState.rAxis[TOUCHPAD_AXIS].x, VRControllerState.rAxis[TOUCHPAD_AXIS].y).GetSafeNormal();
						const FVector2D UpDir(0.f, 1.f);
						const FVector2D RightDir(1.f, 0.f);

						const float VerticalDot = TouchDir | UpDir;
						const float RightDot = TouchDir | RightDir;

						const bool bPressed = !TouchDir.IsNearlyZero() && CurrentStates[ ESteamVRControllerButton::TouchPadPress ];
						
						CurrentStates[ ESteamVRControllerButton::TouchPadUp ]		= bPressed && (VerticalDot >= DOT_45DEG);
						CurrentStates[ ESteamVRControllerButton::TouchPadDown ]		= bPressed && (VerticalDot <= -DOT_45DEG);
						CurrentStates[ ESteamVRControllerButton::TouchPadLeft ]		= bPressed && (RightDot <= -DOT_45DEG);
						CurrentStates[ ESteamVRControllerButton::TouchPadRight ]	= bPressed && (RightDot >= DOT_45DEG);

						if ( ControllerState.TouchPadXAnalog != VRControllerState.rAxis[TOUCHPAD_AXIS].x)
						{
							const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_Thumbstick_X : FGamepadKeyNames::MotionController_Right_Thumbstick_X;
							MessageHandler->OnControllerAnalog(AxisButton, ControllerIndex, VRControllerState.rAxis[TOUCHPAD_AXIS].x);
							ControllerState.TouchPadXAnalog = VRControllerState.rAxis[TOUCHPAD_AXIS].x;
						}

						if ( ControllerState.TouchPadYAnalog != VRControllerState.rAxis[TOUCHPAD_AXIS].y)
						{
							const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_Thumbstick_Y : FGamepadKeyNames::MotionController_Right_Thumbstick_Y;
							MessageHandler->OnControllerAnalog(AxisButton, ControllerIndex, VRControllerState.rAxis[TOUCHPAD_AXIS].y);
							ControllerState.TouchPadYAnalog = VRControllerState.rAxis[TOUCHPAD_AXIS].y;
						}

						if ( ControllerState.TriggerAnalog != VRControllerState.rAxis[TRIGGER_AXIS].x)
						{
							const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_TriggerAxis : FGamepadKeyNames::MotionController_Right_TriggerAxis;
							MessageHandler->OnControllerAnalog(AxisButton, ControllerIndex, VRControllerState.rAxis[TRIGGER_AXIS].x);
							ControllerState.TriggerAnalog = VRControllerState.rAxis[TRIGGER_AXIS].x;
						}

						// For each button check against the previous state and send the correct message if any
						for (int32 ButtonIndex = 0; ButtonIndex < ESteamVRControllerButton::TotalButtonCount; ++ButtonIndex)
						{
							if (CurrentStates[ButtonIndex] != ControllerState.ButtonStates[ButtonIndex])
							{
								if (CurrentStates[ButtonIndex])
								{
									MessageHandler->OnControllerButtonPressed( Buttons[ (int32)HandToUse ][ ButtonIndex ], ControllerIndex, false );
								}
								else
								{
									MessageHandler->OnControllerButtonReleased( Buttons[ (int32)HandToUse ][ ButtonIndex ], ControllerIndex, false );
								}

								if (CurrentStates[ButtonIndex] != 0)
								{
									// this button was pressed - set the button's NextRepeatTime to the InitialButtonRepeatDelay
									ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
								}
							}

							// Update the state for next time
							ControllerState.ButtonStates[ButtonIndex] = CurrentStates[ButtonIndex];
						}

						ControllerState.PacketNum = VRControllerState.unPacketNum;
					}
				}

				for (int32 ButtonIndex = 0; ButtonIndex < ESteamVRControllerButton::TotalButtonCount; ++ButtonIndex)
				{
					if ( ControllerState.ButtonStates[ButtonIndex] != 0 && ControllerState.NextRepeatTime[ButtonIndex] <= CurrentTime)
					{
						MessageHandler->OnControllerButtonPressed( Buttons[ (int32)HandToUse ][ ButtonIndex ], ControllerIndex, true );

						// set the button's NextRepeatTime to the ButtonRepeatDelay
						ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
					}
				}
			}
		}
	}


	int32 UnrealControllerIdToControllerIndex( const int32 UnrealControllerId, const EControllerHand Hand ) const
	{
		return UnrealControllerId * CONTROLLERS_PER_PLAYER + (int32)Hand;
	}

	
	void SetChannelValue(int32 UnrealControllerId, FForceFeedbackChannelType ChannelType, float Value) override
	{
		// Skip unless this is the left or right large channel, which we consider to be the only SteamVRController feedback channel
		if( ChannelType != FForceFeedbackChannelType::LEFT_LARGE && ChannelType != FForceFeedbackChannelType::RIGHT_LARGE )
		{
			return;
		}

		const EControllerHand Hand = ( ChannelType == FForceFeedbackChannelType::LEFT_LARGE ) ? EControllerHand::Left : EControllerHand::Right;
		const int32 ControllerIndex = UnrealControllerIdToControllerIndex( UnrealControllerId, Hand );

		if ((ControllerIndex >= 0) && ( ControllerIndex < MaxControllers))
		{
			FControllerState& ControllerState = ControllerStates[ ControllerIndex ];

			ControllerState.ForceFeedbackValue = Value;

			UpdateVibration( ControllerIndex );
		}
	}

	void SetChannelValues(int32 UnrealControllerId, const FForceFeedbackValues& Values) override
	{
		const int32 LeftControllerIndex = UnrealControllerIdToControllerIndex( UnrealControllerId, EControllerHand::Left );
		if ((LeftControllerIndex >= 0) && ( LeftControllerIndex < MaxControllers))
		{
			FControllerState& ControllerState = ControllerStates[ LeftControllerIndex ];
			ControllerState.ForceFeedbackValue = Values.LeftLarge;

			UpdateVibration( LeftControllerIndex );
		}

		const int32 RightControllerIndex = UnrealControllerIdToControllerIndex( UnrealControllerId, EControllerHand::Right );
		if( ( RightControllerIndex >= 0 ) && ( RightControllerIndex < MaxControllers ) )
		{
			FControllerState& ControllerState = ControllerStates[ RightControllerIndex ];
			ControllerState.ForceFeedbackValue = Values.RightLarge;

			UpdateVibration( RightControllerIndex );
		}
	}

	void UpdateVibration( const int32 ControllerIndex )
	{
		// make sure there is a valid device for this controller
		int32 DeviceIndex = ControllerToDeviceMap[ ControllerIndex ];
		if (DeviceIndex < 0)
		{
			return;
		}

		const FControllerState& ControllerState = ControllerStates[ ControllerIndex ];
		vr::IVRSystem* VRSystem = GetVRSystem();

		if (VRSystem == nullptr)
		{
			return;
		}

		// Map the float values from [0,1] to be more reasonable values for the SteamController.  The docs say that [100,2000] are reasonable values
 		const float LeftIntensity = FMath::Clamp(ControllerState.ForceFeedbackValue * 2000.f, 0.f, 2000.f);
		if (LeftIntensity > 0.f)
		{
			VRSystem->TriggerHapticPulse(DeviceIndex, TOUCHPAD_AXIS, LeftIntensity);
		}
	}

	virtual void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override
	{
		MessageHandler = InMessageHandler;
	}

	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition) const
	{
		bool RetVal = false;

 		FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
 		if (SteamVRHMD)
 		{
 			FQuat DeviceOrientation = FQuat::Identity;
 			RetVal = SteamVRHMD->GetControllerHandPositionAndOrientation(ControllerIndex, DeviceHand, OutPosition, DeviceOrientation);
 			OutOrientation = DeviceOrientation.Rotator();
 		}

		return RetVal;
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
		/** Which hand this controller is representing */
		EControllerHand Hand;

		/** If packet num matches that on your prior call, then the controller state hasn't been changed since 
		  * your last call and there is no need to process it. */
		uint32 PacketNum;

		/** touchpad analog values */
		float TouchPadXAnalog;
		float TouchPadYAnalog;

		/** trigger analog value */
		float TriggerAnalog;

		/** Last frame's button states, so we only send events on edges */
		bool ButtonStates[ ESteamVRControllerButton::TotalButtonCount ];

		/** Next time a repeat event should be generated for each button */
		double NextRepeatTime[ ESteamVRControllerButton::TotalButtonCount ];

		/** Value for force feedback on this controller hand */
		float ForceFeedbackValue;
	};

	/** Mappings between tracked devices and 0 indexed controllers */
	int32 NumControllersMapped;
	int32 ControllerToDeviceMap[ MaxControllers ];
	int32 DeviceToControllerMap[vr::k_unMaxTrackedDeviceCount];

	/** Controller states */
	FControllerState ControllerStates[ MaxControllers ];

	/** Delay before sending a repeat message after a button was first pressed */
	float InitialButtonRepeatDelay;

	/** Delay before sending a repeat message after a button has been pressed for a while */
	float ButtonRepeatDelay;

	/** Mapping of controller buttons */
	FGamepadKeyNames::Type Buttons[ CONTROLLERS_PER_PLAYER ][ ESteamVRControllerButton::TotalButtonCount ];

	/** handler to send all messages to */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	/** the SteamVR plugin module */
	ISteamVRPlugin* SteamVRPlugin;

	/** weak pointer to the IVRSystem owned by the HMD module */
	TWeakPtr<vr::IVRSystem> HMDVRSystem;
};


class FSteamVRControllerPlugin : public ISteamVRControllerPlugin
{
	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override
	{
		return TSharedPtr< class IInputDevice >(new FSteamVRController(InMessageHandler));

	}
};

IMPLEMENT_MODULE( FSteamVRControllerPlugin, SteamVRController)
