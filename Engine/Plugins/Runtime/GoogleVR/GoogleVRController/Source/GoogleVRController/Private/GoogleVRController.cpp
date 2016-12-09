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
#include "GoogleVRControllerPrivate.h"
#include "CoreDelegates.h"
#include "IHeadMountedDisplay.h"
#include "Classes/GoogleVRControllerFunctionLibrary.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"

extern gvr_context* GVRAPI;
extern gvr_user_prefs* GVRUserPrefs;
#endif // GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS

#if GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
#define CONTROLLER_EVENT_FORWARDED_PORT 7003 //Change this port number if it is already taken.
#define ADB_FORWARD_RETRY_TIME 5.0 //5 seconds
static int EmulatorHandednessPreference = 0; // set to right handed by default;
static bool bKeepConnectingControllerEmulator = false;
static double LastTimeTryAdbForward = 0.0;
static bool bIsLastTickInPlayMode = false;
static FRotator BaseEmulatorOrientation= FRotator::ZeroRotator;
static bool SetupAdbForward();
static bool ExecuteAdbCommand( const FString& CommandLine, FString* OutStdOut, FString* OutStdErr);
static void GetAdbPath(FString& OutADBPath);
static bool IsPlayInEditor();
#endif

class FGoogleVRControllerPlugin : public IGoogleVRControllerPlugin
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	gvr::ControllerApi* CreateAndInitGoogleVRControllerAPI()
	{
		// Get controller API
#if GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS
		gvr::ControllerApi* pController = new gvr::ControllerApi();
#elif GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
		gvr::ControllerApi* pController = new gvr::ControllerEmulatorApi();
#endif
		check(pController);

		int32_t options = gvr::ControllerApi::DefaultOptions();

		// by default we turn on everything
		options |= GVR_CONTROLLER_ENABLE_GESTURES;
		options |= GVR_CONTROLLER_ENABLE_ACCEL;
		options |= GVR_CONTROLLER_ENABLE_GYRO;
		options |= GVR_CONTROLLER_ENABLE_TOUCH;
		options |= GVR_CONTROLLER_ENABLE_ORIENTATION;

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
#elif GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
		success = static_cast<gvr::ControllerEmulatorApi*>(pController)->InitEmulator(options, CONTROLLER_EVENT_FORWARDED_PORT);
#endif
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
		UE_LOG(LogGoogleVRController, Log, TEXT("Creating Input Device: GoogleVRController -- Supported"));
		gvr::ControllerApi* pControllerAPI = CreateAndInitGoogleVRControllerAPI();
		if(pControllerAPI)
		{
			return TSharedPtr< class IInputDevice >(new FGoogleVRController(pControllerAPI, InMessageHandler));
		}
		else
		{
			return nullptr;
		}
#else
		UE_LOG(LogGoogleVRController, Warning, TEXT("Creating Input Device: GoogleVRController -- Not Supported"));
		return nullptr;
#endif
	}
};

IMPLEMENT_MODULE( FGoogleVRControllerPlugin, GoogleVRController)

#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
FGoogleVRController::FGoogleVRController(gvr::ControllerApi* pControllerAPI, const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
	: pController(pControllerAPI)
	, MessageHandler(InMessageHandler)
#else
FGoogleVRController::FGoogleVRController(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
	: MessageHandler(InMessageHandler)
#endif
{
	UE_LOG(LogGoogleVRController, Log, TEXT("GoogleVR Controller Created"));

#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	// Register motion controller!
	IModularFeatures::Get().RegisterModularFeature( GetModularFeatureName(), this );

#if GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
	static FAutoConsoleCommand ConnectGVRControllerEmulator(
		TEXT("GVRController.Connect"),
		TEXT("Connect GoogleVR Controller Emulatation in Editor"),
		FConsoleCommandDelegate::CreateRaw(this, &FGoogleVRController::ApplicationResumeDelegate)
	);

	static FAutoConsoleCommand DisconnectGVRControllerEmulator(
		TEXT("GVRController.Disconnect"),
		TEXT("Disconnect GoogleVR Controller Emulatation in Editor"),
		FConsoleCommandDelegate::CreateRaw(this, &FGoogleVRController::ApplicationPauseDelegate)
	);

	static FAutoConsoleCommand SetEmulatorToRightHanded(
		TEXT("GVRController.SetToRightHanded"),
		TEXT("Set the controllre emulator handedness to right handed"),
		FConsoleCommandDelegate::CreateLambda([]() { EmulatorHandednessPreference = 0;})
	);

	static FAutoConsoleCommand SetEmulatorToLeftHanded(
		TEXT("GVRController.SetToLeftHanded"),
		TEXT("Set the controllre emulator handedness to left handed"),
		FConsoleCommandDelegate::CreateLambda([]() { EmulatorHandednessPreference = 1;})
	);

#endif
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
#if GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS
	// Go ahead and resume to be safe
	ApplicationResumeDelegate();
#endif
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
#if GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
	bKeepConnectingControllerEmulator = false;
#endif
	pController->Pause();
#endif
}

void FGoogleVRController::ApplicationResumeDelegate()
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
#if GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
	bKeepConnectingControllerEmulator = true;
#endif
	pController->Resume();
#endif
}

void FGoogleVRController::PollController()
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
#if GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
	int32_t PreviousConnectionState = CachedControllerState.GetConnectionState();
	// If controller connection is requested but it is not connected, try resetup adb forward.
	if(bKeepConnectingControllerEmulator
	   && PreviousConnectionState != gvr::ControllerConnectionState::GVR_CONTROLLER_CONNECTED)
	{
		double CurrentTime = FPlatformTime::Seconds();
		if(CurrentTime - LastTimeTryAdbForward > ADB_FORWARD_RETRY_TIME)
		{
			UE_LOG(LogGoogleVRController, Log, TEXT("Trying to connect to GoogleVR Controller"));
			SetupAdbForward();
			LastTimeTryAdbForward = CurrentTime;
		}
	}

	CachedControllerState.Update(*pController);

	if(PreviousConnectionState != gvr::ControllerConnectionState::GVR_CONTROLLER_CONNECTED
	   && CachedControllerState.GetConnectionState() == gvr::ControllerConnectionState::GVR_CONTROLLER_CONNECTED)
	{
		UE_LOG(LogGoogleVRController, Log, TEXT("GoogleVR Controller Connected"));
	}

	if(PreviousConnectionState == gvr::ControllerConnectionState::GVR_CONTROLLER_CONNECTED
	   && CachedControllerState.GetConnectionState() != gvr::ControllerConnectionState::GVR_CONTROLLER_CONNECTED)
	{
		UE_LOG(LogGoogleVRController, Log, TEXT("GoogleVR Controller Disconnected"));
	}
#elif GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS
	CachedControllerState.Update(*pController);
#endif
#endif
}

void FGoogleVRController::ProcessControllerButtons()
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	// Capture our current button states
	bool CurrentButtonStates[EGoogleVRControllerButton::TotalButtonCount] = {0};

	// Process our known set of buttons
	if (CachedControllerState.GetButtonState(gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK))
	{
		CurrentButtonStates[EGoogleVRControllerButton::TouchPadPress] = true;
	}
	else if (CachedControllerState.GetButtonUp(gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK))
	{
		CurrentButtonStates[EGoogleVRControllerButton::TouchPadPress] = false;
	}

	if (CachedControllerState.GetButtonDown(gvr::ControllerButton::GVR_CONTROLLER_BUTTON_HOME))
	{
		// Ignore Home press as its reserved
	}
	else if (CachedControllerState.GetButtonUp(gvr::ControllerButton::GVR_CONTROLLER_BUTTON_HOME))
	{
		// Ignore Home release as its reserved
	}

	// Note: VolumeUp and VolumeDown Controller states are also ignored as they are reserved

	if (CachedControllerState.GetButtonState(gvr::ControllerButton::GVR_CONTROLLER_BUTTON_APP))
	{
		CurrentButtonStates[EGoogleVRControllerButton::ApplicationMenu] = true;
	}
	else if (CachedControllerState.GetButtonUp(gvr::ControllerButton::GVR_CONTROLLER_BUTTON_APP))
	{
		CurrentButtonStates[EGoogleVRControllerButton::ApplicationMenu] = false;
	}

	// Note: There is no Grip or Trigger button information from the CachedControllerState; so do nothing
	// EGoogleVRControllerButton::TriggerPress - unhandled
	// EGoogleVRControllerButton::Grip - unhandled

	// Process touches and analog information
	// OnDown
	CurrentButtonStates[EGoogleVRControllerButton::TouchPadTouch] = CachedControllerState.IsTouching();

	// The controller's touch positions are in [0,1]^2 coordinate space, we want to be in [-1,1]^2, so translate the touch positions.
	FVector2D TranslatedLocation = FVector2D((CachedControllerState.GetTouchPos().x * 2) - 1, (CachedControllerState.GetTouchPos().y * 2) - 1);

	// OnHold
	if( CachedControllerState.IsTouching() || CachedControllerState.GetTouchUp() )
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

	else if(!CachedControllerState.IsTouching())
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

void FGoogleVRController::ProcessControllerEvents()
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	if (CachedControllerState.GetRecentered())
	{
#if GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
		// Perform recenter when using in editor controller emulation
		if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->GetVersionString().Contains(TEXT("GoogleVR")) )
		{
			GEngine->HMDDevice.Get()->ResetOrientation();
		}
		BaseEmulatorOrientation.Yaw += LastOrientation.Yaw;
#endif
		UGoogleVRControllerFunctionLibrary::GetGoogleVRControllerEventManager()->OnControllerRecenteredDelegate.Broadcast();
	}
#endif
}

bool FGoogleVRController::IsAvailable() const
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	if( CachedControllerState.GetApiStatus() == gvr::ControllerApiStatus::GVR_CONTROLLER_API_OK &&
		CachedControllerState.GetConnectionState() == gvr::ControllerConnectionState::GVR_CONTROLLER_CONNECTED )
	{
		return true;
	}
#endif
	return false;
}

int FGoogleVRController::GetGVRControllerHandedness() const
{
#if GOOGLEVRCONTROLLER_SUPPORTED_ANDROID_PLATFORMS
	if (GVRUserPrefs)
	{
		return static_cast<int>(gvr_user_prefs_get_controller_handedness(GVRUserPrefs));
	}
	else
	{
		return -1;
	}
#elif GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
	return EmulatorHandednessPreference;
#else
	return -1;
#endif
}

void FGoogleVRController::Tick(float DeltaTime)
{
#if GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
	bool bIsInPlayMode = IsPlayInEditor();
	if(bIsInPlayMode && !bIsLastTickInPlayMode)
	{
		ApplicationResumeDelegate();
	}
	else if(!bIsInPlayMode && bIsLastTickInPlayMode)
	{
		ApplicationPauseDelegate();
	}
	bIsLastTickInPlayMode = bIsInPlayMode;
#endif

	PollController();
}

void FGoogleVRController::SendControllerEvents()
{
	ProcessControllerButtons();
	ProcessControllerEvents();
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
		gvr_quatf ControllerOrientation = CachedControllerState.GetOrientation();
		OutOrientation = FQuat(ControllerOrientation.qz, -ControllerOrientation.qx, -ControllerOrientation.qy, ControllerOrientation.qw).Rotator();
#if GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
		OutOrientation.Yaw -= BaseEmulatorOrientation.Yaw;
#endif
#endif

		LastOrientation = OutOrientation;

		return true;
	}

	return false;
}

ETrackingStatus FGoogleVRController::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
	if(IsAvailable())
	{
		return ETrackingStatus::Tracked;
	}
#endif

	return ETrackingStatus::NotTracked;
}

#if GOOGLEVRCONTROLLER_SUPPORTED_EMULATOR_PLATFORMS
static bool SetupAdbForward()
{
	const FString AdbForwardCmd = FString::Printf(TEXT("forward tcp:%d tcp:%d"), CONTROLLER_EVENT_FORWARDED_PORT, 7003);
	FString StdOut;
	if(!ExecuteAdbCommand(*AdbForwardCmd, &StdOut, nullptr))
	{
		return false;
	}
	return true;
}

// Copied from AndroidDeviceDetectionModule.cpp
// TODO: would be nice if Unreal make that function public so we don't need to make a duplicate.
static bool ExecuteAdbCommand( const FString& CommandLine, FString* OutStdOut, FString* OutStdErr )
{
	// execute the command
	int32 ReturnCode;
	FString DefaultError;

	// make sure there's a place for error output to go if the caller specified nullptr
	if (!OutStdErr)
	{
		OutStdErr = &DefaultError;
	}
	FString AdbPath;
	GetAdbPath(AdbPath);

	FPlatformProcess::ExecProcess(*AdbPath, *CommandLine, &ReturnCode, OutStdOut, OutStdErr);

	if (ReturnCode != 0)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("The Android SDK command '%s' failed to run. Return code: %d, Error: %s\n"), *CommandLine, ReturnCode, **OutStdErr);

		return false;
	}

	return true;
}

// Copied from AndroidDeviceDetectionModule.cpp
// TODO: would be nice if Unreal make that function public so we don't need to make a duplicate.
static void GetAdbPath(FString& OutAdbPath)
{
	TCHAR AndroidDirectory[32768] = { 0 };
	FPlatformMisc::GetEnvironmentVariable(TEXT("ANDROID_HOME"), AndroidDirectory, 32768);

	#if PLATFORM_MAC
	if (AndroidDirectory[0] == 0)
	{
		// didn't find ANDROID_HOME, so parse the .bash_profile file on MAC
		FArchive* FileReader = IFileManager::Get().CreateFileReader(*FString([@"~/.bash_profile" stringByExpandingTildeInPath]));
		if (FileReader)
		{
			const int64 FileSize = FileReader->TotalSize();
			ANSICHAR* AnsiContents = (ANSICHAR*)FMemory::Malloc(FileSize + 1);
			FileReader->Serialize(AnsiContents, FileSize);
			FileReader->Close();
			delete FileReader;

			AnsiContents[FileSize] = 0;
			TArray<FString> Lines;
			FString(ANSI_TO_TCHAR(AnsiContents)).ParseIntoArrayLines(Lines);
			FMemory::Free(AnsiContents);

			for (int32 Index = Lines.Num()-1; Index >=0; Index--)
			{
				if (AndroidDirectory[0] == 0 && Lines[Index].StartsWith(TEXT("export ANDROID_HOME=")))
				{
					FString Directory;
					Lines[Index].Split(TEXT("="), NULL, &Directory);
					Directory = Directory.Replace(TEXT("\""), TEXT(""));
					FCString::Strcpy(AndroidDirectory, *Directory);
					setenv("ANDROID_HOME", TCHAR_TO_ANSI(AndroidDirectory), 1);
				}
			}
		}
	}
	#endif

	if (AndroidDirectory[0] != 0)
	{
	#if PLATFORM_WINDOWS
		OutAdbPath = FString::Printf(TEXT("%s\\platform-tools\\adb.exe"), AndroidDirectory);
	#else
		OutAdbPath = FString::Printf(TEXT("%s/platform-tools/adb"), AndroidDirectory);
	#endif

		// if it doesn't exist then just clear the path as we might set it later
		if (!FPaths::FileExists(*OutAdbPath))
		{
			OutAdbPath.Empty();
		}
	}
}

static bool IsPlayInEditor()
{
	for(const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if(Context.World()->IsPlayInEditor())
		{
			return true;
		}
	}
	return false;
}
#endif