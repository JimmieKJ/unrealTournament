/* Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "GoogleVRController.h"
#include "IHeadMountedDisplay.h"

#if GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"

extern gvr_context *GVRAPI;
#endif // GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS

class FGoogleVRControllerPlugin : public IGoogleVRControllerPlugin
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	gvr::ControllerApi* CreateAndInitGoogleVRControllerAPI()
	{
		// Get controller API
		ControllerApiPtr pController = new gvr::ControllerApi();
		check(pController);

		gvr::ControllerApiOptions options;
		gvr::ControllerApi::InitDefaultOptions(&options);

		// by default we turn on everything
		options.enable_gestures = true;
		options.enable_accel = true;
		options.enable_gyro = true;
		options.enable_touch = true;
		options.enable_orientation = true;

		bool success = false;
#if GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS
		// Have to get the application context and class loader for initializing the controller Api
		JNIEnv* jenv = FAndroidApplication::GetJavaEnv();
		static jmethodID Method = FJavaWrapper::FindMethod(jenv, FJavaWrapper::GameActivityClassID, "getApplicationContext", "()Landroid/content/Context;", false);
		static jobject ApplicationContext = FJavaWrapper::CallObjectMethod(jenv, FJavaWrapper::GameActivityThis, Method);
		jclass MainClass = FAndroidApplication::FindJavaClass("com/epicgames/ue4/GameActivity");
		jclass classClass = jenv->FindClass("java/lang/Class");
		jmethodID getClassLoaderMethod = jenv->GetMethodID(classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
		jobject classLoader = jenv->CallObjectMethod(MainClass, getClassLoaderMethod);

		success = pController->Init(jenv, ApplicationContext, classLoader, options, GVRAPI);
#endif //GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS

		if (!success)
		{
			UE_LOG(LogGoogleVRController, Log, TEXT("Failed to initialize GoogleVR Controller."));
			delete pController;
			return nullptr;
		}
		else
		{
			UE_LOG(LogGoogleVRController, Log, TEXT("Successfully initialized GoogleVR Controller."));
			return pController;
		}
	}
#endif //GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS

	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override
	{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
		UE_LOG(LogTemp, Warning, TEXT("Creating Input Device: GoogleVRController -- Supported"));
		ControllerApi* pControllerAPI = CreateAndInitGoogleVRControllerAPI();
		if(pControllerAPI)
		{
			return TSharedPtr< class IInputDevice >(new FGoogleVRController(pControllerAPI, InMessageHandler));
		}
		else
		{
			return nullptr;
		}
#else
		UE_LOG(LogTemp, Warning, TEXT("Creating Input Device: GoogleVRController -- Not Supported"));
		return nullptr;
#endif
	}
};

IMPLEMENT_MODULE( FGoogleVRControllerPlugin, GoogleVRController)

FGoogleVRController::FGoogleVRController(ControllerApiPtr pControllerAPI, const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
	:
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	pController(pControllerAPI),
#endif
	bControllerReadyToPollState(false),
	MessageHandler(InMessageHandler)
{
	UE_LOG(LogTemp, Warning, TEXT("GoogleVR Controller Created"));

#if GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS
	// For android platform, controller should be able to sync controller state once it is created.
	bControllerReadyToPollState = true;
#endif

#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	// Register motion controller!
	IModularFeatures::Get().RegisterModularFeature( GetModularFeatureName(), this );

	// Setup button mappings
	Buttons[(int32)EControllerHand::Left][EGoogleVRControllerButton::ApplicationMenu] = FGamepadKeyNames::MotionController_Left_Shoulder;
	Buttons[(int32)EControllerHand::Right][EGoogleVRControllerButton::ApplicationMenu] = FGamepadKeyNames::MotionController_Right_Shoulder;
	
	Buttons[(int32)EControllerHand::Left][EGoogleVRControllerButton::TouchPadLeft] = FGamepadKeyNames::MotionController_Left_FaceButton4;
	Buttons[(int32)EControllerHand::Right][EGoogleVRControllerButton::TouchPadLeft] = FGamepadKeyNames::MotionController_Right_FaceButton4;
	Buttons[(int32)EControllerHand::Left][EGoogleVRControllerButton::TouchPadUp] = FGamepadKeyNames::MotionController_Left_FaceButton1;
	Buttons[(int32)EControllerHand::Right][EGoogleVRControllerButton::TouchPadUp] = FGamepadKeyNames::MotionController_Right_FaceButton1;
	Buttons[(int32)EControllerHand::Left][EGoogleVRControllerButton::TouchPadRight] = FGamepadKeyNames::MotionController_Left_FaceButton2;
	Buttons[(int32)EControllerHand::Right][EGoogleVRControllerButton::TouchPadRight] = FGamepadKeyNames::MotionController_Right_FaceButton2;
	Buttons[(int32)EControllerHand::Left][EGoogleVRControllerButton::TouchPadDown] = FGamepadKeyNames::MotionController_Left_FaceButton3;
	Buttons[(int32)EControllerHand::Right][EGoogleVRControllerButton::TouchPadDown] = FGamepadKeyNames::MotionController_Right_FaceButton3;

	Buttons[(int32)EControllerHand::Left][EGoogleVRControllerButton::System] = FGamepadKeyNames::FGamepadKeyNames::SpecialLeft;
	Buttons[(int32)EControllerHand::Right][EGoogleVRControllerButton::System] = FGamepadKeyNames::FGamepadKeyNames::SpecialRight;

	Buttons[(int32)EControllerHand::Left][EGoogleVRControllerButton::TriggerPress] = FGamepadKeyNames::MotionController_Left_Trigger;
	Buttons[(int32)EControllerHand::Right][EGoogleVRControllerButton::TriggerPress] = FGamepadKeyNames::MotionController_Right_Trigger;

	Buttons[(int32)EControllerHand::Left][EGoogleVRControllerButton::Grip] = FGamepadKeyNames::MotionController_Left_Grip1;
	Buttons[(int32)EControllerHand::Right][EGoogleVRControllerButton::Grip] = FGamepadKeyNames::MotionController_Right_Grip1;

	Buttons[(int32)EControllerHand::Left][EGoogleVRControllerButton::TouchPadPress] = FGamepadKeyNames::MotionController_Left_Thumbstick;
	Buttons[(int32)EControllerHand::Right][EGoogleVRControllerButton::TouchPadPress] = FGamepadKeyNames::MotionController_Right_Thumbstick;

	Buttons[(int32)EControllerHand::Left][EGoogleVRControllerButton::TouchPadTouch] = GoogleVRControllerKeyNames::Touch0;
	Buttons[(int32)EControllerHand::Right][EGoogleVRControllerButton::TouchPadTouch] = GoogleVRControllerKeyNames::Touch0;
	
	// Register callbacks for pause and resume
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FGoogleVRController::ApplicationPauseDelegate);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FGoogleVRController::ApplicationResumeDelegate);

	if(bControllerReadyToPollState)
	{
		// Go ahead and resume to be safe
		ApplicationResumeDelegate();
	}
#endif
}

FGoogleVRController::~FGoogleVRController()
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
	delete pController;
	pController = nullptr;
#endif
}

void FGoogleVRController::ApplicationPauseDelegate()
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	
	pController->Pause();

#endif
}

void FGoogleVRController::ApplicationResumeDelegate()
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	
	pController->Resume();

#endif
}

void FGoogleVRController::PollController()
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	// read the controller state into our cache
	if(bControllerReadyToPollState)
	{
		pController->ReadState(&CachedControllerState);
	}
#endif

	
}

void FGoogleVRController::ProcessControllerButtons()
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	
	// Capture our current button states
	bool CurrentButtonStates[EGoogleVRControllerButton::TotalButtonCount] = {0};

	// Process our known set of buttons
	if (CachedControllerState.button_state[gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK])
	{
		CurrentButtonStates[EGoogleVRControllerButton::TouchPadPress] = true;
	}
	else if (CachedControllerState.button_up[gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK])
	{
		CurrentButtonStates[EGoogleVRControllerButton::TouchPadPress] = false;
	}

	if (CachedControllerState.button_down[gvr::ControllerButton::GVR_CONTROLLER_BUTTON_HOME])
	{
		// Ignore Home press as its reserved
	}
	else if (CachedControllerState.button_up[gvr::ControllerButton::GVR_CONTROLLER_BUTTON_HOME])
	{
		// Ignore Home release as its reserved
	}

	// Note: VolumeUp and VolumeDown Controller states are also ignored as they are reserved

	if (CachedControllerState.button_state[gvr::ControllerButton::GVR_CONTROLLER_BUTTON_APP])
	{
		CurrentButtonStates[EGoogleVRControllerButton::ApplicationMenu] = true;
	}
	else if (CachedControllerState.button_up[gvr::ControllerButton::GVR_CONTROLLER_BUTTON_APP])
	{
		CurrentButtonStates[EGoogleVRControllerButton::ApplicationMenu] = false;
	}

	// Note: There is no Grip or Trigger button information from the CachedControllerState; so do nothing
	// EGoogleVRControllerButton::TriggerPress - unhandled
	// EGoogleVRControllerButton::Grip - unhandled

	// Process touches and analog information	
	// OnDown
	if(CachedControllerState.is_touching)
	{
		CurrentButtonStates[EGoogleVRControllerButton::TouchPadTouch] = true;
	}

	// OnUp
	else if(CachedControllerState.touch_up)
	{
		CurrentButtonStates[EGoogleVRControllerButton::TouchPadTouch] = false;
	}
	
	// The controller's touch positions are in [0,1]^2 coordinate space, we want to be in [-1,1]^2, so translate the touch positions.
	FVector2D TranslatedLocation = FVector2D((CachedControllerState.touch_pos.x * 2) - 1, (CachedControllerState.touch_pos.y * 2) - 1);

	// OnHold
	if( CachedControllerState.is_touching || CachedControllerState.touch_up )
	{
		const FVector2D TouchDir = TranslatedLocation.GetSafeNormal();
		const FVector2D UpDir(0.f, 1.f);
		const FVector2D RightDir(1.f, 0.f);

		const float VerticalDot = TouchDir | UpDir;
		const float RightDot = TouchDir | RightDir;

		const bool bPressed = !TouchDir.IsNearlyZero() && CurrentButtonStates[EGoogleVRControllerButton::TouchPadPress];

		CurrentButtonStates[EGoogleVRControllerButton::TouchPadUp] = bPressed && (VerticalDot <= -DOT_45DEG);
		CurrentButtonStates[EGoogleVRControllerButton::TouchPadDown] = bPressed && (VerticalDot >= DOT_45DEG);
		CurrentButtonStates[EGoogleVRControllerButton::TouchPadLeft] = bPressed && (RightDot <= -DOT_45DEG);
		CurrentButtonStates[EGoogleVRControllerButton::TouchPadRight] = bPressed && (RightDot >= DOT_45DEG);
	}

	else if(!CachedControllerState.is_touching)
	{
		TranslatedLocation.X = 0.0f;
		TranslatedLocation.Y = 0.0f;
	}
	
	MessageHandler->OnControllerAnalog( FGamepadKeyNames::MotionController_Left_Thumbstick_X, 0, TranslatedLocation.X);
	MessageHandler->OnControllerAnalog( FGamepadKeyNames::MotionController_Right_Thumbstick_X, 0, TranslatedLocation.X);
		
	MessageHandler->OnControllerAnalog( FGamepadKeyNames::MotionController_Left_Thumbstick_Y, 0, TranslatedLocation.Y);
	MessageHandler->OnControllerAnalog( FGamepadKeyNames::MotionController_Right_Thumbstick_Y, 0, TranslatedLocation.Y);
	
	// Process buttons for both hands at the same time
	for(int32 ButtonIndex = 0; ButtonIndex < (int32)EGoogleVRControllerButton::TotalButtonCount; ++ButtonIndex)
	{
		if(CurrentButtonStates[ButtonIndex] != LastButtonStates[ButtonIndex])
		{
			// OnDown
			if(CurrentButtonStates[ButtonIndex])
			{
				MessageHandler->OnControllerButtonPressed( Buttons[(int32)EControllerHand::Left][ButtonIndex], 0, false);
				MessageHandler->OnControllerButtonPressed( Buttons[(int32)EControllerHand::Right][ButtonIndex], 0, false);
			}
			// On Up
			else
			{
				MessageHandler->OnControllerButtonReleased( Buttons[(int32)EControllerHand::Left][ButtonIndex], 0, false);
				MessageHandler->OnControllerButtonReleased( Buttons[(int32)EControllerHand::Right][ButtonIndex], 0, false);
			}
		}

		// update state for next time
		LastButtonStates[ButtonIndex] = CurrentButtonStates[ButtonIndex];
	}

#endif // GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
}

bool FGoogleVRController::IsAvailable() const
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS

	if( CachedControllerState.api_status == gvr::ControllerApiStatus::GVR_CONTROLLER_API_OK &&
		CachedControllerState.connection_state == gvr::ControllerConnectionState::GVR_CONTROLLER_CONNECTED )
	{
		return true;
	}

#endif

	return false;
}

void FGoogleVRController::Tick(float DeltaTime)
{
	PollController();
}

void FGoogleVRController::SendControllerEvents()
{
	ProcessControllerButtons();
}

void FGoogleVRController::SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

bool FGoogleVRController::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{ 
	return false;
}

void FGoogleVRController::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
}

void FGoogleVRController::SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values)
{
}

bool FGoogleVRController::GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition) const
{
	if(IsAvailable())
	{
		OutPosition = FVector::ZeroVector;
		OutOrientation = FRotator::ZeroRotator;

#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS

		OutOrientation = FQuat(CachedControllerState.orientation.qz, -CachedControllerState.orientation.qx, -CachedControllerState.orientation.qy, CachedControllerState.orientation.qw).Rotator();

#endif

		LastOrientation = OutOrientation;

		return true;
	}

	return false;
}

ETrackingStatus FGoogleVRController::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS

	if( IsAvailable() )
	{
		return ETrackingStatus::Tracked;
	}

#endif

	return ETrackingStatus::NotTracked;
}
