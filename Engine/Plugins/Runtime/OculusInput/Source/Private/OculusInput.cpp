// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusInput.h"
#include "Features/IModularFeatures.h"
#include "IOculusRiftPlugin.h"

#if USE_OVR_MOTION_SDK

#define OVR_TESTING 0
#define OVR_DEBUG_LOGGING 0

#define LOCTEXT_NAMESPACE "OculusTouch"

const FKey FOculusTouchCapacitiveKey::OculusTouch_Left_Thumbstick("OculusTouch_Left_Thumbstick");
const FKey FOculusTouchCapacitiveKey::OculusTouch_Left_Trigger("OculusTouch_Left_Trigger");
const FKey FOculusTouchCapacitiveKey::OculusTouch_Left_FaceButton1("OculusTouch_Left_FaceButton1");
const FKey FOculusTouchCapacitiveKey::OculusTouch_Left_FaceButton2("OculusTouch_Left_FaceButton2");
const FKey FOculusTouchCapacitiveKey::OculusTouch_Left_IndexPointing("OculusTouch_Left_IndexPointing");
const FKey FOculusTouchCapacitiveKey::OculusTouch_Left_ThumbUp("OculusTouch_Left_ThumbUp");

const FKey FOculusTouchCapacitiveKey::OculusTouch_Right_Thumbstick("OculusTouch_Right_Thumbstick");
const FKey FOculusTouchCapacitiveKey::OculusTouch_Right_Trigger("OculusTouch_Right_Trigger");
const FKey FOculusTouchCapacitiveKey::OculusTouch_Right_FaceButton1("OculusTouch_Right_FaceButton1");
const FKey FOculusTouchCapacitiveKey::OculusTouch_Right_FaceButton2("OculusTouch_Right_FaceButton2");
const FKey FOculusTouchCapacitiveKey::OculusTouch_Right_IndexPointing("OculusTouch_Right_IndexPointing");
const FKey FOculusTouchCapacitiveKey::OculusTouch_Right_ThumbUp("OculusTouch_Right_ThumbUp");

const FOculusTouchCapacitiveKeyNames::Type FOculusTouchCapacitiveKeyNames::OculusTouch_Left_Thumbstick("OculusTouch_Left_Thumbstick");
const FOculusTouchCapacitiveKeyNames::Type FOculusTouchCapacitiveKeyNames::OculusTouch_Left_Trigger("OculusTouch_Left_Trigger");
const FOculusTouchCapacitiveKeyNames::Type FOculusTouchCapacitiveKeyNames::OculusTouch_Left_FaceButton1("OculusTouch_Left_FaceButton1");
const FOculusTouchCapacitiveKeyNames::Type FOculusTouchCapacitiveKeyNames::OculusTouch_Left_FaceButton2("OculusTouch_Left_FaceButton2");
const FOculusTouchCapacitiveKeyNames::Type FOculusTouchCapacitiveKeyNames::OculusTouch_Left_IndexPointing("OculusTouch_Left_IndexPointing");
const FOculusTouchCapacitiveKeyNames::Type FOculusTouchCapacitiveKeyNames::OculusTouch_Left_ThumbUp("OculusTouch_Left_ThumbUp");

const FOculusTouchCapacitiveKeyNames::Type FOculusTouchCapacitiveKeyNames::OculusTouch_Right_Thumbstick("OculusTouch_Right_Thumbstick");
const FOculusTouchCapacitiveKeyNames::Type FOculusTouchCapacitiveKeyNames::OculusTouch_Right_Trigger("OculusTouch_Right_Trigger");
const FOculusTouchCapacitiveKeyNames::Type FOculusTouchCapacitiveKeyNames::OculusTouch_Right_FaceButton1("OculusTouch_Right_FaceButton1");
const FOculusTouchCapacitiveKeyNames::Type FOculusTouchCapacitiveKeyNames::OculusTouch_Right_FaceButton2("OculusTouch_Right_FaceButton2");
const FOculusTouchCapacitiveKeyNames::Type FOculusTouchCapacitiveKeyNames::OculusTouch_Right_IndexPointing("OculusTouch_Right_IndexPointing");
const FOculusTouchCapacitiveKeyNames::Type FOculusTouchCapacitiveKeyNames::OculusTouch_Right_ThumbUp("OculusTouch_Right_ThumbUp");

FOculusInput::FOculusInput( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
	: MessageHandler( InMessageHandler )
	, ControllerPairs()
	, TriggerThreshold(0.8f)
{
	// Initializes LibOVR. 
	ovrInitParams initParams;
	FMemory::Memset(initParams, 0);
	initParams.Flags = ovrInit_RequestVersion;
	initParams.RequestedMinorVersion = OVR_MINOR_VERSION;
#if !UE_BUILD_SHIPPING
//	initParams.LogCallback = OvrLogCallback;
#endif

	ovrResult initStatus = ovr_Initialize(&initParams);
	if (!OVR_SUCCESS(initStatus) && initStatus == ovrError_LibLoad)
	{
		// fatal errors: can't load library
 		UE_LOG(LogOcInput, Log, TEXT("Can't find Oculus library %s: is proper Runtime installed? Version: %s"),
 			TEXT(OVR_FILE_DESCRIPTION_STRING), TEXT(OVR_VERSION_STRING));
		return;
	}

	FOculusTouchControllerPair& ControllerPair = *new(ControllerPairs) FOculusTouchControllerPair();

	// @todo: Unreal controller index should be assigned to us by the engine to ensure we don't contest with other devices
	ControllerPair.UnrealControllerIndex = 0; //???? NextUnrealControllerIndex++;

	// Load the config, even if we failed to initialize a controller
	LoadConfig();

	IModularFeatures::Get().RegisterModularFeature( GetModularFeatureName(), this );
	GEngine->MotionControllerDevices.AddUnique(this);

	// Register the FKeys
	EKeys::AddKey(FKeyDetails(FOculusTouchCapacitiveKey::OculusTouch_Left_Thumbstick, LOCTEXT("OculusTouch_Left_Thumbstick", "Oculus Touch (L) Thumbstick CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FOculusTouchCapacitiveKey::OculusTouch_Left_FaceButton1, LOCTEXT("OculusTouch_Left_FaceButton1", "Oculus Touch (L) X Button CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FOculusTouchCapacitiveKey::OculusTouch_Left_Trigger, LOCTEXT("OculusTouch_Left_Trigger", "Oculus Touch (L) Trigger CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FOculusTouchCapacitiveKey::OculusTouch_Left_FaceButton2, LOCTEXT("OculusTouch_Left_FaceButton2", "Oculus Touch (L) Y Button CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FOculusTouchCapacitiveKey::OculusTouch_Left_IndexPointing, LOCTEXT("OculusTouch_Left_IndexPointing", "Oculus Touch (L) Pointing CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FOculusTouchCapacitiveKey::OculusTouch_Left_ThumbUp, LOCTEXT("OculusTouch_Left_ThumbUp", "Oculus Touch (L) Thumb Up CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	EKeys::AddKey(FKeyDetails(FOculusTouchCapacitiveKey::OculusTouch_Right_Thumbstick, LOCTEXT("OculusTouch_Right_Thumbstick", "Oculus Touch (R) Thumbstick CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FOculusTouchCapacitiveKey::OculusTouch_Right_FaceButton1, LOCTEXT("OculusTouch_Right_FaceButton1", "Oculus Touch (R) A Button CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FOculusTouchCapacitiveKey::OculusTouch_Right_Trigger, LOCTEXT("OculusTouch_Right_Trigger", "Oculus Touch (R) Trigger CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FOculusTouchCapacitiveKey::OculusTouch_Right_FaceButton2, LOCTEXT("OculusTouch_Right_FaceButton2", "Oculus Touch (R) B Button CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FOculusTouchCapacitiveKey::OculusTouch_Right_IndexPointing, LOCTEXT("OculusTouch_Right_IndexPointing", "Oculus Touch (R) Pointing CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FOculusTouchCapacitiveKey::OculusTouch_Right_ThumbUp, LOCTEXT("OculusTouch_Right_ThumbUp", "Oculus Touch (R) Thumb Up CapTouch"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	UE_LOG(LogOcInput, Log, TEXT("OculusInput is initialized. Init status %d. Runtime version: %s"), int(initStatus), *FString(ANSI_TO_TCHAR(ovr_GetVersionString())));
}


FOculusInput::~FOculusInput()
{
	IModularFeatures::Get().UnregisterModularFeature( GetModularFeatureName(), this );
	ovr_Shutdown();
}


void FOculusInput::LoadConfig()
{
	const TCHAR* OculusTouchSettings = TEXT("OculusTouch.Settings");
	float ConfigThreshold = TriggerThreshold;

	if (GConfig->GetFloat(OculusTouchSettings, TEXT("TriggerThreshold"), ConfigThreshold, GEngineIni))
	{
		TriggerThreshold = ConfigThreshold;
	}
}

void FOculusInput::Tick( float DeltaTime )
{
	// Nothing to do when ticking, for now.  SendControllerEvents() handles everything.
}


void FOculusInput::SendControllerEvents()
{
	const double CurrentTime = FPlatformTime::Seconds();

	// @todo: Should be made configurable and unified with other controllers handling of repeat
	const float InitialButtonRepeatDelay = 0.2f;
	const float ButtonRepeatDelay = 0.1f;
	const float AnalogButtonPressThreshold = TriggerThreshold;

	check(IOculusRiftPlugin::IsAvailable());
	IOculusRiftPlugin& OculusRiftPlugin = IOculusRiftPlugin::Get();
	ovrSession OvrSession = OculusRiftPlugin.GetSession();
	UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: OvrSession = %p"), OvrSession);
	if (OvrSession)
	{
		ovrInputState OvrInput;
		ovrTrackingState OvrTrackingState;

		const ovrResult OvrRes = ovr_GetInputState(OvrSession, ovrControllerType_Touch, &OvrInput);
		const bool bOvrGCTRes = OculusRiftPlugin.GetCurrentTrackingState(&OvrTrackingState);

		UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: ovr_GetInputState() ret = %d, GetCurrentTrackingState ret = %d"), int(OvrRes), int(bOvrGCTRes));

		if (OVR_SUCCESS(OvrRes) && bOvrGCTRes)
		{
			UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: ConnectedControllerTypes 0x%X"), OvrInput.ConnectedControllerTypes);
			UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: ButtonState = 0x%X"), OvrInput.Buttons);
			UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: Touches = 0x%X"), OvrInput.Touches);

			for (FOculusTouchControllerPair& ControllerPair : ControllerPairs)
			{
				for( int32 HandIndex = 0; HandIndex < ARRAY_COUNT( ControllerPair.ControllerStates ); ++HandIndex )
				{
					FOculusTouchControllerState& State = ControllerPair.ControllerStates[ HandIndex ];

					const bool bIsLeft = (HandIndex == (int32)EControllerHand::Left);
					bool bIsCurrentlyTracked = (bIsLeft ? (OvrInput.ConnectedControllerTypes & ovrControllerType_LTouch) != 0 : (OvrInput.ConnectedControllerTypes & ovrControllerType_RTouch) != 0);

#if OVR_TESTING
					bIsCurrentlyTracked = true;
					static float _angle = 0;
					OvrTrackingState.HandPoses[HandIndex].ThePose.Orientation = OVR::Quatf(OVR::Vector3f(0, 0, 1), _angle);
					_angle += 0.1f;

					OvrTrackingState.HandPoses[HandIndex].ThePose = OvrTrackingState.HeadPose.ThePose;
					UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Error, TEXT("SendControllerEvents: OVR_TESTING is enabled!"));
#endif

					if (bIsCurrentlyTracked)
					{
						State.bIsCurrentlyTracked = true;

						const float OvrTriggerAxis = OvrInput.IndexTrigger[HandIndex];
						const float OvrGripAxis = OvrInput.HandTrigger[HandIndex];

						UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: IndexTrigger[%d] = %f"), int(HandIndex), OvrTriggerAxis);
						UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: HandTrigger[%d] = %f"), int(HandIndex), OvrGripAxis);
						UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: ThumbStick[%d] = { %f, %f }"), int(HandIndex), OvrInput.Thumbstick[HandIndex].x, OvrInput.Thumbstick[HandIndex].y );

						if (OvrTriggerAxis != State.TriggerAxis)
						{
							State.TriggerAxis = OvrTriggerAxis;
							MessageHandler->OnControllerAnalog(bIsLeft ? FGamepadKeyNames::MotionController_Left_TriggerAxis : FGamepadKeyNames::MotionController_Right_TriggerAxis, ControllerPair.UnrealControllerIndex, State.TriggerAxis);
						}

						if (OvrGripAxis != State.GripAxis)
						{
							State.GripAxis = OvrGripAxis;
							MessageHandler->OnControllerAnalog(bIsLeft ? FGamepadKeyNames::MotionController_Left_Grip1Axis : FGamepadKeyNames::MotionController_Right_Grip1Axis, ControllerPair.UnrealControllerIndex, State.GripAxis);
						}

						if (OvrInput.Thumbstick[HandIndex].x != State.ThumbstickAxes.X)
						{
							State.ThumbstickAxes.X = OvrInput.Thumbstick[HandIndex].x;
							MessageHandler->OnControllerAnalog(bIsLeft ? FGamepadKeyNames::MotionController_Left_Thumbstick_X : FGamepadKeyNames::MotionController_Right_Thumbstick_X, ControllerPair.UnrealControllerIndex, State.ThumbstickAxes.X);
						}

						if (OvrInput.Thumbstick[HandIndex].y != State.ThumbstickAxes.Y)
						{
							State.ThumbstickAxes.Y = OvrInput.Thumbstick[HandIndex].y;
							// we need to negate Y value to match XBox controllers
							MessageHandler->OnControllerAnalog(bIsLeft ? FGamepadKeyNames::MotionController_Left_Thumbstick_Y : FGamepadKeyNames::MotionController_Right_Thumbstick_Y, ControllerPair.UnrealControllerIndex, -State.ThumbstickAxes.Y);
						}

						for (int32 ButtonIndex = 0; ButtonIndex < (int32)EOculusTouchControllerButton::TotalButtonCount; ++ButtonIndex)
						{
							FOculusButtonState& ButtonState = State.Buttons[ButtonIndex];
							check(!ButtonState.Key.IsNone()); // is button's name initialized?

							// Determine if the button is pressed down
							bool bButtonPressed = false;
							switch ((EOculusTouchControllerButton)ButtonIndex)
							{
							case EOculusTouchControllerButton::Trigger:
								bButtonPressed = State.TriggerAxis >= AnalogButtonPressThreshold;
								break;

							case EOculusTouchControllerButton::Grip:
								bButtonPressed = State.GripAxis >= AnalogButtonPressThreshold;
								break;

							case EOculusTouchControllerButton::XA:
								bButtonPressed = bIsLeft ? (OvrInput.Buttons & ovrButton_X) != 0 : (OvrInput.Buttons & ovrButton_A) != 0;
								break;

							case EOculusTouchControllerButton::YB:
								bButtonPressed = bIsLeft ? (OvrInput.Buttons & ovrButton_Y) != 0 : (OvrInput.Buttons & ovrButton_B) != 0;
								break;

							case EOculusTouchControllerButton::Thumbstick:
								bButtonPressed = bIsLeft ? (OvrInput.Buttons & ovrButton_LThumb) != 0 : (OvrInput.Buttons & ovrButton_RThumb) != 0;
								break;

							default:
								check(0);
								break;
							}

							// Update button state
							if (bButtonPressed != ButtonState.bIsPressed)
							{
								const bool bIsRepeat = false;

								ButtonState.bIsPressed = bButtonPressed;
								if (ButtonState.bIsPressed)
								{
									MessageHandler->OnControllerButtonPressed(ButtonState.Key, ControllerPair.UnrealControllerIndex, bIsRepeat);

									// Set the timer for the first repeat
									ButtonState.NextRepeatTime = CurrentTime + ButtonRepeatDelay;
								}
								else
								{
									MessageHandler->OnControllerButtonReleased(ButtonState.Key, ControllerPair.UnrealControllerIndex, bIsRepeat);
								}
							}

							// Apply key repeat, if its time for that
							if (ButtonState.bIsPressed && ButtonState.NextRepeatTime <= CurrentTime)
							{
								const bool bIsRepeat = true;
								MessageHandler->OnControllerButtonPressed(ButtonState.Key, ControllerPair.UnrealControllerIndex, bIsRepeat);

								// Set the timer for the next repeat
								ButtonState.NextRepeatTime = CurrentTime + ButtonRepeatDelay;
							}
						}

						// Handle Capacitive States
						for (int32 CapTouchIndex = 0; CapTouchIndex < (int32)EOculusTouchCapacitiveAxes::TotalAxisCount; ++CapTouchIndex)
						{
							FOculusTouchCapacitiveState& CapState = State.CapacitiveAxes[CapTouchIndex];

							float CurrentAxisVal = 0.f;
							switch ((EOculusTouchCapacitiveAxes)CapTouchIndex)
							{
							case EOculusTouchCapacitiveAxes::XA:
							{
								const uint32 mask = (bIsLeft) ? ovrTouch_X : ovrTouch_A;
								CurrentAxisVal = (OvrInput.Touches & mask) != 0 ? 1.f : 0.f;
								break;
							}
							case EOculusTouchCapacitiveAxes::YB:
							{
								const uint32 mask = (bIsLeft) ? ovrTouch_Y : ovrTouch_B;
								CurrentAxisVal = (OvrInput.Touches & mask) != 0 ? 1.f : 0.f;
								break;
							}
							case EOculusTouchCapacitiveAxes::Thumbstick:
							{
								const uint32 mask = (bIsLeft) ? ovrTouch_LThumb : ovrTouch_RThumb;
								CurrentAxisVal = (OvrInput.Touches & mask) != 0 ? 1.f : 0.f;
								break;
							}
							case EOculusTouchCapacitiveAxes::Trigger:
							{
								const uint32 mask = (bIsLeft) ? ovrTouch_LIndexTrigger : ovrTouch_RIndexTrigger;
								CurrentAxisVal = (OvrInput.Touches & mask) != 0 ? 1.f : 0.f;
								break;
							}
							case EOculusTouchCapacitiveAxes::IndexPointing:
							{
								const uint32 mask = (bIsLeft) ? ovrTouch_LIndexPointing : ovrTouch_RIndexPointing;
								CurrentAxisVal = (OvrInput.Touches & mask) != 0 ? 1.f : 0.f;
								break;
							}
							case EOculusTouchCapacitiveAxes::ThumbUp:
							{
								const uint32 mask = (bIsLeft) ? ovrTouch_LThumbUp : ovrTouch_RThumbUp;
								CurrentAxisVal = (OvrInput.Touches & mask) != 0 ? 1.f : 0.f;
								break;
							}
							default:
								check(0);
							}
							
							if (CurrentAxisVal != CapState.State)
							{
								MessageHandler->OnControllerAnalog(CapState.Axis, ControllerPair.UnrealControllerIndex, CurrentAxisVal);

								CapState.State = CurrentAxisVal;
							}
						}

						const ovrPosef& OvrHandPose = OvrTrackingState.HandPoses[HandIndex].ThePose;
						FVector NewLocation;
						FQuat NewOrientation;
						if (OculusRiftPlugin.PoseToOrientationAndPosition(OvrHandPose, /* Out */ NewOrientation, /* Out */ NewLocation))
						{
							// OK, we have up to date positional data!
							State.Orientation = NewOrientation;
							State.Location = NewLocation;

							UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: HandPOSE[%d]: Pos %.3f %.3f %.3f"), HandIndex, NewLocation.X, NewLocation.Y, NewLocation.Y);
							UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: HandPOSE[%d]: Yaw %.3f Pitch %.3f Roll %.3f"), HandIndex, NewOrientation.Rotator().Yaw, NewOrientation.Rotator().Pitch, NewOrientation.Rotator().Roll);
						}
						else
						{
							// HMD wasn't ready.  This can currently happen if we try to grab motion data before we've rendered at least one frame
							UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: PoseToOrientationAndPosition returned false"));
						}
					}
					else
					{
						// Controller isn't available right now.  Zero out input state, so that if it comes back it will send fresh event deltas
						State = FOculusTouchControllerState((EControllerHand)HandIndex);
						UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT("SendControllerEvents: Controller for the hand %d is not tracked"), int(HandIndex));
					}
				}
			}
		}
	}
	UE_CLOG(OVR_DEBUG_LOGGING, LogOcInput, Log, TEXT(""));
}


void FOculusInput::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;
}


bool FOculusInput::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	// No exec commands supported, for now.
	return false;
}

void FOculusInput::SetChannelValue( int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value )
{
	const EControllerHand Hand = ( ChannelType == FForceFeedbackChannelType::LEFT_LARGE || ChannelType == FForceFeedbackChannelType::LEFT_SMALL ) ? EControllerHand::Left : EControllerHand::Right;

	for( FOculusTouchControllerPair& ControllerPair : ControllerPairs )
	{
		if( ControllerPair.UnrealControllerIndex == ControllerId )
		{
			FOculusTouchControllerState& ControllerState = ControllerPair.ControllerStates[ (int32)Hand ];

			if (ControllerState.bPlayingHapticEffect)
			{
				continue;
			}

			// @todo: The SMALL channel controls frequency, the LARGE channel controls amplitude.  This is a bit of a weird fit.
			if( ChannelType == FForceFeedbackChannelType::LEFT_SMALL || ChannelType == FForceFeedbackChannelType::RIGHT_SMALL )
			{
				ControllerState.HapticFrequency = Value;
			}
			else
			{
				ControllerState.HapticAmplitude = Value;
			}

			UpdateForceFeedback( ControllerPair, Hand );

			break;
		}
	}
}

void FOculusInput::SetChannelValues( int32 ControllerId, const FForceFeedbackValues& Values )
{
	for( FOculusTouchControllerPair& ControllerPair : ControllerPairs )
	{
		if( ControllerPair.UnrealControllerIndex == ControllerId )
		{
			// @todo: The SMALL channel controls frequency, the LARGE channel controls amplitude.  This is a bit of a weird fit.
			FOculusTouchControllerState& LeftControllerState = ControllerPair.ControllerStates[ (int32)EControllerHand::Left ];
			if (!LeftControllerState.bPlayingHapticEffect)
			{
				LeftControllerState.HapticFrequency = Values.LeftSmall;
				LeftControllerState.HapticAmplitude = Values.LeftLarge;
				UpdateForceFeedback(ControllerPair, EControllerHand::Left);
			}

			FOculusTouchControllerState& RightControllerState = ControllerPair.ControllerStates[(int32)EControllerHand::Right];
			if (!RightControllerState.bPlayingHapticEffect)
			{
				RightControllerState.HapticFrequency = Values.RightSmall;
				RightControllerState.HapticAmplitude = Values.RightLarge;
				UpdateForceFeedback(ControllerPair, EControllerHand::Right);
			}
		}
	}
}

void FOculusInput::UpdateForceFeedback( const FOculusTouchControllerPair& ControllerPair, const EControllerHand Hand )
{
	const FOculusTouchControllerState& ControllerState = ControllerPair.ControllerStates[ (int32)Hand ];

	check(IOculusRiftPlugin::IsAvailable());
	ovrSession OvrSession = IOculusRiftPlugin::Get().GetSession();

	if( ControllerState.bIsCurrentlyTracked && !ControllerState.bPlayingHapticEffect && OvrSession)
	{
		float FreqMin, FreqMax = 0.f;
		GetHapticFrequencyRange(FreqMin, FreqMax);

		// Map the [0.0 - 1.0] range to a useful range of frequencies for the Oculus controllers
		const float ActualFrequency = FMath::Lerp(FreqMin, FreqMax, FMath::Clamp(ControllerState.HapticFrequency, 0.0f, 1.0f));

		// Oculus SDK wants amplitude values between 0.0 and 1.0
		const float ActualAmplitude = ControllerState.HapticAmplitude * GetHapticAmplitudeScale();

		const ovrControllerType OvrController = ( Hand == EControllerHand::Left ) ? ovrControllerType_LTouch : ovrControllerType_RTouch;
		ovr_SetControllerVibration( OvrSession, OvrController, ActualFrequency, ActualAmplitude );
	}
}

bool FOculusInput::GetControllerOrientationAndPosition( const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition ) const
{
	bool bHaveControllerData = false;

	for( const FOculusTouchControllerPair& ControllerPair : ControllerPairs )
	{
		if( ControllerPair.UnrealControllerIndex == ControllerIndex )
		{
			if( (int32)DeviceHand >= 0 && (int32)DeviceHand < 2 )
			{
				const FOculusTouchControllerState& ControllerState = ControllerPair.ControllerStates[ (int32)DeviceHand ];

				if( ControllerState.bIsCurrentlyTracked )
				{
					OutOrientation = ControllerState.Orientation.Rotator();
					OutPosition = ControllerState.Location;

					bHaveControllerData = true;
				}
			}

			break;
		}
	}

	return bHaveControllerData;
}

ETrackingStatus FOculusInput::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	ETrackingStatus TrackingStatus = ETrackingStatus::NotTracked;

	if ((int32) DeviceHand < 0 || (int32)DeviceHand >= 2)
	{
		return TrackingStatus;
	}

	for( const FOculusTouchControllerPair& ControllerPair : ControllerPairs )
	{
		if( ControllerPair.UnrealControllerIndex == ControllerIndex )
		{
			const FOculusTouchControllerState& ControllerState = ControllerPair.ControllerStates[ (int32)DeviceHand ];

			if( ControllerState.bIsCurrentlyTracked )
			{
				TrackingStatus = ETrackingStatus::Tracked;
			}

			break;
		}
	}

	return TrackingStatus;
}

void FOculusInput::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	for (FOculusTouchControllerPair& ControllerPair : ControllerPairs)
	{
		if (ControllerPair.UnrealControllerIndex == ControllerId)
		{
			FOculusTouchControllerState& ControllerState = ControllerPair.ControllerStates[Hand];
			if (ControllerState.bIsCurrentlyTracked)
			{
				check(IOculusRiftPlugin::IsAvailable());
				ovrSession OvrSession = IOculusRiftPlugin::Get().GetSession();
				if (OvrSession)
				{
					float FreqMin, FreqMax = 0.f;
					GetHapticFrequencyRange(FreqMin, FreqMax);

					const float Frequency = FMath::Lerp(FreqMin, FreqMax, FMath::Clamp(Values.Frequency, 0.f, 1.f));
					const float Amplitude = Values.Amplitude * GetHapticAmplitudeScale();

					if ((ControllerState.HapticAmplitude != Amplitude) || (ControllerState.HapticFrequency != Frequency))
					{
						ControllerState.HapticAmplitude = Amplitude;
						ControllerState.HapticFrequency = Frequency;

						const ovrControllerType OvrController = (EControllerHand(Hand) == EControllerHand::Left) ? ovrControllerType_LTouch : ovrControllerType_RTouch;
						ovr_SetControllerVibration(OvrSession, OvrController, Frequency, Amplitude);

						UE_CLOG(0, LogOcInput, Log, TEXT("SetHapticFeedbackValues: Hand %d, freq %f, amp %f"), int(Hand), Frequency, Amplitude);

						ControllerState.bPlayingHapticEffect = (Amplitude != 0.f) && (Frequency != 0.f);
					}
				}
			}

			break;
		}
	}
}

void FOculusInput::GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const
{
	MinFrequency = 0.f;
	MaxFrequency = 1.f;
}

float FOculusInput::GetHapticAmplitudeScale() const
{
	return 1.f;
}

#undef LOCTEXT_NAMESPACE
#endif	 // USE_OVR_MOTION_SDK