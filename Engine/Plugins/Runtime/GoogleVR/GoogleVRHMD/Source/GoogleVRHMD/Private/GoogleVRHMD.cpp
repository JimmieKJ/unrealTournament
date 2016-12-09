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

#include "GoogleVRHMD.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "Misc/EngineVersion.h"
#include "Misc/CoreDelegates.h"
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#endif // GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
#include "IOS/IOSApplication.h"
#include "IOS/IOSWindow.h"
#include "IOSAppDelegate.h"
#include "IOSView.h"
#endif
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

///////////////////////////////////////////
// Begin GoogleVR Api Console Variables //
///////////////////////////////////////////

static TAutoConsoleVariable<int32> CVarViewerPreview(
	TEXT("googlevr.ViewerPreview"),
	3,
	TEXT("Change which viewer data is used for VR previewing.\n")
	TEXT(" 0: No viewer or distortion\n")
	TEXT(" 1: Google Cardboard 1.0\n")
	TEXT(" 2: Google Cardboard 2.0\n")
	TEXT(" 3: ViewMaster (default)\n")
	TEXT(" 4: SnailVR\n")
	TEXT(" 5: RiTech 2.0\n")
	TEXT(" 6: Go4D C1-Glass")
);

static TAutoConsoleVariable<float> CVarPreviewSensitivity(
	TEXT("googlevr.PreviewSensitivity"),
	1.0f,
	TEXT("Change preview sensitivity of Yaw and Pitch multiplier.\n")
	TEXT("Values are clamped between 0.1 and 10.0\n")
);

///////////////////////////////////
// Begin GoogleVR Api Reference //
///////////////////////////////////

// GoogleVR Api Reference
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
gvr_context* GVRAPI = nullptr;
const gvr_user_prefs* GVRUserPrefs = nullptr;
static const int64_t kPredictionTime = 50 * 1000000; //50 millisecond
static const float kDefaultRenderTargetScaleFactor = 1.0f;
#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS

// Only one HMD can be active at a time, so using file static to track this for transferring to game thread
static bool bBackDetected = false;
static bool bTriggerDetected = false;
static double BackbuttonPressTime;
static const double BACK_BUTTON_SHORT_PRESS_TIME = 1.0f;

//static variable for debugging;
static bool bDebugShowGVRSplash = false;

////////////////////////////////////////////////
// Begin Misc Helper Functions Implementation //
////////////////////////////////////////////////

void OnTriggerEvent(void* UserParam)
{
	UE_LOG(LogHMD, Log, TEXT("Trigger event detected"));

	bTriggerDetected = true;
}

////////////////////////////////////////
// Begin Android JNI Helper Functions //
////////////////////////////////////////

#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

// Note: Should probably be moved into AndroidJNI class
int64 CallLongMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...)
{
	if (Method == NULL || Object == NULL)
	{
		return false;
	}

	va_list Args;
	va_start(Args, Method);
	jlong Return = Env->CallLongMethodV(Object, Method, Args);
	va_end(Args);

	return (int64)Return;
}

extern "C" void Java_com_epicgames_ue4_GameActivity_nativeOnUiLayerBack(JNIEnv* jenv, jobject thiz)
{
	// Need to be on game thread to dispatch handler
	bBackDetected = true;
}

void AndroidThunkCpp_UiLayer_SetEnabled(bool bEnable)
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		static jmethodID UiLayerMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_UiLayer_SetEnabled", "(Z)V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, UiLayerMethod, bEnable);
 	}
}
void AndroidThunkCpp_UiLayer_SetViewerName(const FString& ViewerName)
{
	if(ViewerName.Len() == 0)
	{
		return;
	}

 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		static jmethodID UiLayerMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_UiLayer_SetViewerName", "(Ljava/lang/String;)V", false);
		jstring NameJava = Env->NewStringUTF(TCHAR_TO_UTF8(*ViewerName));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, UiLayerMethod, NameJava);
 	}
}

gvr_context* AndroidThunkCpp_GetNativeGVRApi()
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GetNativeGVRApi", "()J", false);
		return reinterpret_cast<gvr_context*>(CallLongMethod(Env, FJavaWrapper::GameActivityThis, Method));
 	}

	return nullptr;
}

void AndroidThunkCpp_GvrLayout_SetFixedPresentationSurfaceSizeToCurrent()
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GvrLayout_SetFixedPresentationSurfaceSizeToCurrent", "()V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
 	}
}

bool AndroidThunkCpp_ProjectWantsCardboardOnlyMode()
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_ProjectWantsCardboardOnlyMode", "()Z", false);
		return FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, Method);
 	}

	return false;
}

bool AndroidThunkCpp_IsVrLaunch()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_IsVrLaunch", "()Z", false);
		return FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}

	return true;
}

void AndroidThunkCpp_QuitDaydreamApplication()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_QuitDaydreamApplication", "()V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
}

FString AndroidThunkCpp_GetDataString()
{
	FString Result = FString("");
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GetDataString", "()Z", false);
		jstring JavaString = (jstring)FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, Method);
		if (JavaString != NULL)
		{
			const char* JavaChars = Env->GetStringUTFChars(JavaString, 0);
			Result = FString(UTF8_TO_TCHAR(JavaChars));
			Env->ReleaseStringUTFChars(JavaString, JavaChars);
			Env->DeleteLocalRef(JavaString);
		}
	}

	return Result;
}

#endif // GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

/////////////////////////////////////
// Begin IOS Class Implementations //
/////////////////////////////////////

#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS

// Helped function to get global access
FGoogleVRHMD* GetGoogleVRHMD()
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->GetVersionString().Contains(TEXT("GoogleVR")) )
	{
		return static_cast<FGoogleVRHMD*>(GEngine->HMDDevice.Get());
	}

	return nullptr;
}

@implementation FOverlayViewDelegate

- (void)didChangeViewerProfile
{
	FGoogleVRHMD* HMD = GetGoogleVRHMD();
	if(HMD)
	{
		HMD->RefreshViewerProfile();
	}
}

- (void)didTapBackButton
{
	bBackDetected = true;
}

@end

#endif

/////////////////////////////////////////////////
// Begin FGoogleVRHMDPlugin Implementation //
/////////////////////////////////////////////////

class FGoogleVRHMDPlugin : public IGoogleVRHMDPlugin
{
public:

	/** Returns the key into the HMDPluginPriority section of the config file for this module */
	virtual FString GetModuleKeyName() const override
	{
		return TEXT("GoogleVRHMD");
	}

	/**
	 * Attempts to create a new head tracking device interface
	 *
	 * @return	Interface to the new head tracking device, if we were able to successfully create one
	 */
	virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay() override;

	/**
	 * Always return true for GoogleVR, when enabled, to allow HMD Priority to sort it out
	 */
	virtual bool IsHMDConnected() { return true; }
};

IMPLEMENT_MODULE( FGoogleVRHMDPlugin, GoogleVRHMD );

TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > FGoogleVRHMDPlugin::CreateHeadMountedDisplay()
{
	TSharedPtr< FGoogleVRHMD, ESPMode::ThreadSafe > HMD(new FGoogleVRHMD());
	if (HMD->IsInitialized())
	{
		return HMD;
	}

	return NULL;
}

/////////////////////////////////////
// Begin FGoogleVRHMD Self API //
/////////////////////////////////////

FGoogleVRHMD::FGoogleVRHMD()
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	: CustomPresent(nullptr)
	, bStereoEnabled(false)
#else
	: bStereoEnabled(false)
#endif
	, bHMDEnabled(false)
	, bDistortionCorrectionEnabled(true)
	, bUseGVRApiDistortionCorrection(false)
	, bUseOffscreenFramebuffers(false)
	, bIsInDaydreamMode(false)
	, bForceStopPresentScene(false)
	, NeckModelScale(1.0f)
	, CurHmdOrientation(FQuat::Identity)
	, CurHmdPosition(FVector::ZeroVector)
	, DeltaControlRotation(FRotator::ZeroRotator)
	, DeltaControlOrientation(FQuat::Identity)
	, BaseOrientation(FQuat::Identity)
	, RendererModule(nullptr)
	, DistortionMeshIndices(nullptr)
	, DistortionMeshVerticesLeftEye(nullptr)
	, DistortionMeshVerticesRightEye(nullptr)
#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
	, OverlayView(nil)
#endif
	, LastUpdatedCacheFrame(0)
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	, DistortedBufferViewportList(nullptr)
	, NonDistortedBufferViewportList(nullptr)
	, ActiveViewportList(nullptr)
	, ScratchViewport(nullptr)
#endif
	, PosePitch(0.0f)
	, PoseYaw(0.0f)
	, DistortionPointsX(40)
	, DistortionPointsY(40)
	, NumVerts(0)
	, NumTris(0)
	, NumIndices(0)
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("Initializing FGoogleVRHMD"));

#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

	// Get GVRAPI from java
	GVRAPI = AndroidThunkCpp_GetNativeGVRApi();

#elif GOOGLEVRHMD_SUPPORTED_PLATFORMS

	GVRAPI = gvr_create();

#endif

	if(IsInitialized())
	{
		UE_LOG(LogHMD, Log, TEXT("GoogleVR API created"));

		// Get renderer module
		static const FName RendererModuleName("Renderer");
		RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		check(RendererModule);

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

		// Initialize OpenGL resources for API
		gvr_initialize_gl(GVRAPI);

		// Log the current viewer
		UE_LOG(LogHMD, Log, TEXT("The current viewer is %s"), UTF8_TO_TCHAR(gvr_get_viewer_model(GVRAPI)));

		// Get gvr user prefs
		GVRUserPrefs = gvr_get_user_prefs(GVRAPI);

#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
		//Set the flag when async reprojection is enabled
		bUseOffscreenFramebuffers = gvr_get_async_reprojection_enabled(GVRAPI);
		//We are in Daydream Mode when async reprojection is enabled.
		bIsInDaydreamMode = bUseOffscreenFramebuffers;

		// Only use gvr api distortion when async reprojection is enabled
		// And by default we use Unreal's PostProcessing Distortion for Cardboard
		bUseGVRApiDistortionCorrection = bUseOffscreenFramebuffers;
		//bUseGVRApiDistortionCorrection = true;  //Uncomment this line is you want to use GVR distortion when async reprojection is not enabled.

		if(bUseOffscreenFramebuffers)
		{
			// Create custom present class
			CustomPresent = new FGoogleVRHMDCustomPresent(this);
			GVRSplash = MakeShareable(new FGoogleVRSplash(this));
			GVRSplash->Init();
		}

		// Show ui on Android
		AndroidThunkCpp_UiLayer_SetEnabled(true);
		AndroidThunkCpp_UiLayer_SetViewerName(UTF8_TO_TCHAR(gvr_get_viewer_model(GVRAPI)));

		// Set Hardware Scaling in GvrLayout
		AndroidThunkCpp_GvrLayout_SetFixedPresentationSurfaceSizeToCurrent();

#elif GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS

		// We will use Unreal's PostProcessing Distortion for iOS
		bUseGVRApiDistortionCorrection = false;
		bIsInDaydreamMode = false;

		// Setup and show ui on iOS
		dispatch_async(dispatch_get_main_queue(), ^
		{
			OverlayViewDelegate = [[FOverlayViewDelegate alloc] init];
			OverlayView = [[GVROverlayView alloc] initWithFrame:[IOSAppDelegate GetDelegate].IOSView.bounds];
			OverlayView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
			OverlayView.delegate = OverlayViewDelegate;
			[[IOSAppDelegate GetDelegate].IOSView addSubview:OverlayView];
		});
#endif // GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

		// By default, go ahead and disable the screen saver. The user can still change this freely
		FPlatformMisc::ControlScreensaver(FGenericPlatformMisc::Disable);

		// TODO: Get trigger event handler working again
		// Setup GVRAPI delegates
		//gvr_set_trigger_event_handler(GVRAPI, OnTriggerEvent, nullptr);

		// Refresh delegate
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FGoogleVRHMD::ApplicationResumeDelegate);

#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS

		// Set the default rendertarget size to the default size in UE4
		SetRenderTargetSizeToDefault();

		// Enabled by default
		EnableHMD(true);
		EnableStereo(true);

		// Initialize distortion mesh and indices
		SetNumOfDistortionPoints(DistortionPointsX, DistortionPointsY);

		// Register LoadMap Delegate
		FCoreUObjectDelegates::PreLoadMap.AddRaw(this, &FGoogleVRHMD::OnPreLoadMap);
	}
	else
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("GoogleVR context failed to create successfully."));
	}
}

FGoogleVRHMD::~FGoogleVRHMD()
{
	delete[] DistortionMeshIndices;
	DistortionMeshIndices = nullptr;
	delete[] DistortionMeshVerticesLeftEye;
	DistortionMeshVerticesLeftEye = nullptr;
	delete[] DistortionMeshVerticesRightEye;
	DistortionMeshVerticesRightEye = nullptr;

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if (DistortedBufferViewportList)
	{
		// gvr destroy function will set the pointer to nullptr
		gvr_buffer_viewport_list_destroy(&DistortedBufferViewportList);
	}
	if (NonDistortedBufferViewportList)
	{
		gvr_buffer_viewport_list_destroy(&NonDistortedBufferViewportList);
	}
	if (ScratchViewport)
	{
		gvr_buffer_viewport_destroy(&ScratchViewport);
	}

	CustomPresent->Shutdown();
    CustomPresent = nullptr;
#endif

	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
}

bool FGoogleVRHMD::IsInitialized() const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	return GVRAPI != nullptr;

#endif

	// Non-supported platform will be PC editor which will always succeed
	return true;
}

void FGoogleVRHMD::UpdateGVRViewportList() const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// Allocate if necessary!
	if (!DistortedBufferViewportList)
	{
		DistortedBufferViewportList = gvr_buffer_viewport_list_create(GVRAPI);
	}
	if (!NonDistortedBufferViewportList)
	{
		NonDistortedBufferViewportList = gvr_buffer_viewport_list_create(GVRAPI);
	}
	if (!ScratchViewport)
	{
		ScratchViewport = gvr_buffer_viewport_create(GVRAPI);
	}

	gvr_get_recommended_buffer_viewports(GVRAPI, DistortedBufferViewportList);
	gvr_get_screen_buffer_viewports(GVRAPI, NonDistortedBufferViewportList);

	ActiveViewportList = bDistortionCorrectionEnabled ? DistortedBufferViewportList : NonDistortedBufferViewportList;

	// Pass the viewport list used for rendering to CustomPresent for async reprojection
	if(CustomPresent)
	{
		CustomPresent->UpdateRenderingViewportList(ActiveViewportList);
	}

#endif
}

void FGoogleVRHMD::GetCurrentPose(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// Update camera pose using cached head pose which we updated at the beginning of a frame.
	CurrentOrientation = BaseOrientation * CachedFinalHeadRotation;
	CurrentPosition = BaseOrientation.RotateVector(CachedFinalHeadPosition);
#else
	// Simulating head rotation using mouse in Editor.
	ULocalPlayer* Player = GEngine->GetDebugLocalPlayer();
	float PreviewSensitivity = FMath::Clamp(CVarPreviewSensitivity.GetValueOnAnyThread(), 0.1f, 10.0f);

	if (Player != NULL && Player->PlayerController != NULL && GWorld)
	{
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		Player->PlayerController->GetInputMouseDelta(MouseX, MouseY);

		double CurrentTime = FApp::GetCurrentTime();
		double DeltaTime = GWorld->GetDeltaSeconds();

		PoseYaw += (FMath::RadiansToDegrees(MouseX * DeltaTime * 4.0f) * PreviewSensitivity);
		PosePitch += (FMath::RadiansToDegrees(MouseY * DeltaTime * 4.0f) * PreviewSensitivity);
		PosePitch = FMath::Clamp(PosePitch, -90.0f + KINDA_SMALL_NUMBER, 90.0f - KINDA_SMALL_NUMBER);

		CurrentOrientation = BaseOrientation * FQuat(FRotator(PosePitch, PoseYaw, 0.0f));
	}
	else
	{
		CurrentOrientation = FQuat(FRotator(0.0f, 0.0f, 0.0f));
	}

	// TODO: add neck model to the editor emulatation.
	CurrentPosition = FVector::ZeroVector;
#endif
}

IRendererModule* FGoogleVRHMD::GetRendererModule()
{
	return RendererModule;
}

void FGoogleVRHMD::RefreshViewerProfile()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr_refresh_viewer_profile(GVRAPI);
#endif

	// Re-Initialize distortion meshes, as the viewer may have changed
	SetNumOfDistortionPoints(DistortionPointsX, DistortionPointsY);
}

FIntPoint FGoogleVRHMD::GetUnrealMobileContentSize()
{
	FIntPoint Size = FIntPoint::ZeroValue;
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	FPlatformRect Rect = FAndroidWindow::GetScreenRect();
	Size.X = Rect.Right;
	Size.Y = Rect.Bottom;
#elif GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
	FPlatformRect Rect = FIOSWindow::GetScreenRect();
	Size.X = Rect.Right;
	Size.Y = Rect.Bottom;
#endif
	return Size;
}

FIntPoint FGoogleVRHMD::GetGVRHMDRenderTargetSize()
{
	return GVRRenderTargetSize;
}

FIntPoint FGoogleVRHMD::GetGVRMaxRenderTargetSize()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr_sizei MaxSize = gvr_get_maximum_effective_render_target_size(GVRAPI);
	UE_LOG(LogHMD, Log, TEXT("GVR Recommended RenderTargetSize: %d x %d"), MaxSize.width, MaxSize.height);
	return FIntPoint{ static_cast<int>(MaxSize.width), static_cast<int>(MaxSize.height) };
#else
	return FIntPoint{ 0, 0 };
#endif
}

FIntPoint FGoogleVRHMD::SetRenderTargetSizeToDefault()
{
	GVRRenderTargetSize = FIntPoint::ZeroValue;
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	if (bUseGVRApiDistortionCorrection)
	{
		FIntPoint RenderTargetSize = FIntPoint::ZeroValue;
		SetGVRHMDRenderTargetSize(kDefaultRenderTargetScaleFactor, RenderTargetSize);
	}
	else
	{
		FPlatformRect Rect = FAndroidWindow::GetScreenRect();
		GVRRenderTargetSize.X = Rect.Right;
		GVRRenderTargetSize.Y = Rect.Bottom;
	}
#elif GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
	FPlatformRect Rect = FIOSWindow::GetScreenRect();
	GVRRenderTargetSize.X = Rect.Right;
	GVRRenderTargetSize.Y = Rect.Bottom;
#endif
	return GVRRenderTargetSize;
}

bool FGoogleVRHMD::SetGVRHMDRenderTargetSize(float ScaleFactor, FIntPoint& OutRenderTargetSize)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if (ScaleFactor < 0.1 || ScaleFactor > 1)
	{
        	ScaleFactor = FMath::Clamp(ScaleFactor, 0.1f, 1.0f);
		UE_LOG(LogHMD, Warning, TEXT("Invalid RenderTexture Scale Factor. The valid value should be within [0.1, 1.0]. Clamping the value to %f"), ScaleFactor);
	}

	// For now, only allow change the render texture size when using gvr distortion.
	// Since when use PPHMD, the render texture size need to match the surface size.
	if (!bUseGVRApiDistortionCorrection)
	{
		return false;
	}
	UE_LOG(LogHMD, Log, TEXT("Setting render target size using scale factor: %f"), ScaleFactor);
	FIntPoint DesiredRenderTargetSize = GetGVRMaxRenderTargetSize();
	DesiredRenderTargetSize.X *= ScaleFactor;
	DesiredRenderTargetSize.Y *= ScaleFactor;
	return SetGVRHMDRenderTargetSize(DesiredRenderTargetSize.X, DesiredRenderTargetSize.Y, OutRenderTargetSize);
#else
	return false;
#endif
}

bool FGoogleVRHMD::SetGVRHMDRenderTargetSize(int DesiredWidth, int DesiredHeight, FIntPoint& OutRenderTargetSize)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// For now, only allow change the render texture size when using gvr distortion.
	// Since when use PPHMD, the render texture size need to match the surface size.
	if (!bUseGVRApiDistortionCorrection)
	{
		return false;
	}

	// Ensure sizes are dividable by DividableBy to get post processing effects with lower resolution working well
	const uint32 DividableBy = 4;

	const uint32 Mask = ~(DividableBy - 1);
	GVRRenderTargetSize.X = (DesiredWidth + DividableBy - 1) & Mask;
	GVRRenderTargetSize.Y = (DesiredHeight + DividableBy - 1) & Mask;

	OutRenderTargetSize = GVRRenderTargetSize;
	UE_LOG(LogHMD, Log, TEXT("Set Render Target Size to %d x %d, the deired size is %d x %d"), GVRRenderTargetSize.X, GVRRenderTargetSize.Y, DesiredWidth, DesiredHeight);
	return true;
#else
	return false;
#endif
}

void FGoogleVRHMD::OnPreLoadMap(const FString &)
{
	// Force not to present the scene when start loading a map.
	bForceStopPresentScene = true;
}

void FGoogleVRHMD::ApplicationResumeDelegate()
{
	RefreshViewerProfile();
}

void FGoogleVRHMD::HandleGVRBackEvent()
{
	if( GEngine &&
		GEngine->GameViewport &&
		GEngine->GameViewport->Viewport &&
		GEngine->GameViewport->Viewport->GetClient() )
	{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
		GEngine->GameViewport->Viewport->GetClient()->InputKey(GEngine->GameViewport->Viewport, 0, EKeys::Android_Back, EInputEvent::IE_Pressed);
		GEngine->GameViewport->Viewport->GetClient()->InputKey(GEngine->GameViewport->Viewport, 0, EKeys::Android_Back, EInputEvent::IE_Released);
#elif GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
		// TODO: iOS doesn't have hardware back buttons, so what should be fired?
		GEngine->GameViewport->Viewport->GetClient()->InputKey(GEngine->GameViewport->Viewport, 0, EKeys::Global_Back, EInputEvent::IE_Pressed);
		GEngine->GameViewport->Viewport->GetClient()->InputKey(GEngine->GameViewport->Viewport, 0, EKeys::Global_Back, EInputEvent::IE_Released);
#endif
	}
}

void FGoogleVRHMD::SetDistortionCorrectionEnabled(bool bEnable)
{
	// Can't turn off distortion correction if using async reprojection;
	if(bUseOffscreenFramebuffers)
	{
		bDistortionCorrectionEnabled = true;
	}
	else
	{
		bDistortionCorrectionEnabled = bEnable;
	}
}

void FGoogleVRHMD::SetDistortionCorrectionMethod(bool bInUseGVRApiDistortionCorrection)
{
	//Cannot change distortion method when use async reprojection
	if(bUseOffscreenFramebuffers)
	{
		bUseGVRApiDistortionCorrection = true;
	}
	else
	{
		bUseGVRApiDistortionCorrection = bInUseGVRApiDistortionCorrection;
	}
}

bool FGoogleVRHMD::SetDefaultViewerProfile(const FString& ViewerProfileURL)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if(gvr_set_default_viewer_profile(GVRAPI, TCHAR_TO_ANSI(*ViewerProfileURL)))
	{
		gvr_refresh_viewer_profile(GVRAPI);

		// Re-Initialize distortion meshes, as the viewer may have changed
		SetNumOfDistortionPoints(DistortionPointsX, DistortionPointsY);

		return true;
	}
#endif

	return false;
}

void FGoogleVRHMD::SetNumOfDistortionPoints(int32 XPoints, int32 YPoints)
{
	// Force non supported platform distortion mesh be 40 x 40
#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS
	XPoints = 40;
	YPoints = 40;
#endif

	// clamp values
	if (XPoints < 2)
	{
		XPoints = 2;
	}
	else if (XPoints > 200)
	{
		XPoints = 200;
	}

	if (YPoints < 2)
	{
		YPoints = 2;
	}
	else if (YPoints > 200)
	{
		YPoints = 200;
	}

	// calculate our values
	DistortionPointsX = XPoints;
	DistortionPointsY = YPoints;
	NumVerts = DistortionPointsX * DistortionPointsY;
	NumTris = (DistortionPointsX - 1) * (DistortionPointsY - 1) * 2;
	NumIndices = NumTris * 3;

	// generate the distortion mesh
	GenerateDistortionCorrectionIndexBuffer();
	GenerateDistortionCorrectionVertexBuffer(eSSP_LEFT_EYE);
	GenerateDistortionCorrectionVertexBuffer(eSSP_RIGHT_EYE);
}

void FGoogleVRHMD::SetDistortionMeshSize(EDistortionMeshSizeEnum MeshSize)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	switch(MeshSize)
	{
	case EDistortionMeshSizeEnum::DMS_VERYSMALL:
		SetNumOfDistortionPoints(20, 20);
		break;
	case EDistortionMeshSizeEnum::DMS_SMALL:
		SetNumOfDistortionPoints(40, 40);
		break;
	case EDistortionMeshSizeEnum::DMS_MEDIUM:
		SetNumOfDistortionPoints(60, 60);
		break;
	case EDistortionMeshSizeEnum::DMS_LARGE:
		SetNumOfDistortionPoints(80, 80);
		break;
	case EDistortionMeshSizeEnum::DMS_VERYLARGE:
		SetNumOfDistortionPoints(100, 100);
		break;
	}
#endif
}

void FGoogleVRHMD::SetNeckModelScale(float ScaleFactor)
{
	NeckModelScale = FMath::Clamp(ScaleFactor, 0.0f, 1.0f);
}

bool FGoogleVRHMD::GetDistortionCorrectionEnabled() const
{
	return bDistortionCorrectionEnabled;
}

bool FGoogleVRHMD::IsUsingGVRApiDistortionCorrection() const
{
	return bUseGVRApiDistortionCorrection;
}

float FGoogleVRHMD::GetNeckModelScale() const
{
	return NeckModelScale;
}

float FGoogleVRHMD::GetWorldToMetersScale() const
{
	if (GWorld != nullptr)
	{
		return GWorld->GetWorldSettings()->WorldToMeters;
	}

	// Default value, assume Unreal units are in centimeters
	return 100.0f;
}

bool FGoogleVRHMD::IsVrLaunch() const
{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	return AndroidThunkCpp_IsVrLaunch();
#endif
	return false;
}

FString FGoogleVRHMD::GetIntentData() const
{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	return AndroidThunkCpp_GetDataString();
#endif
	return TEXT("");
}

FString FGoogleVRHMD::GetViewerModel()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	return UTF8_TO_TCHAR(gvr_get_viewer_model(GVRAPI));
#endif

	return TEXT("");
}

FString FGoogleVRHMD::GetViewerVendor()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	return UTF8_TO_TCHAR(gvr_get_viewer_vendor(GVRAPI));
#endif

	return TEXT("");
}

FGoogleVRHMD::EViewerPreview FGoogleVRHMD::GetPreviewViewerType()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	return EVP_None;
#else
	int32 Val = FMath::Clamp(CVarViewerPreview.GetValueOnAnyThread(), 0, 6);
	return EViewerPreview(Val);
#endif
}

float FGoogleVRHMD::GetPreviewViewerInterpupillaryDistance()
{
	switch(GetPreviewViewerType())
	{
#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS

	case EVP_GoogleCardboard1:
		return GoogleCardboardViewerPreviews::GoogleCardboard1::InterpupillaryDistance;
	case EVP_GoogleCardboard2:
		return GoogleCardboardViewerPreviews::GoogleCardboard2::InterpupillaryDistance;
	case EVP_ViewMaster:
		return GoogleCardboardViewerPreviews::ViewMaster::InterpupillaryDistance;
	case EVP_SnailVR:
		return GoogleCardboardViewerPreviews::SnailVR::InterpupillaryDistance;
	case EVP_RiTech2:
		return GoogleCardboardViewerPreviews::RiTech2::InterpupillaryDistance;
	case EVP_Go4DC1Glass:
		return GoogleCardboardViewerPreviews::Go4DC1Glass::InterpupillaryDistance;

#endif

	case EVP_None:
	default:
		return 0.064f;
	}
}

FMatrix FGoogleVRHMD::GetPreviewViewerStereoProjectionMatrix(enum EStereoscopicPass StereoPass)
{
	switch(GetPreviewViewerType())
	{
#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS

	case EVP_GoogleCardboard1:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::GoogleCardboard1::LeftStereoProjectionMatrix : GoogleCardboardViewerPreviews::GoogleCardboard1::RightStereoProjectionMatrix;
	case EVP_GoogleCardboard2:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::GoogleCardboard2::LeftStereoProjectionMatrix : GoogleCardboardViewerPreviews::GoogleCardboard2::RightStereoProjectionMatrix;
	case EVP_ViewMaster:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::ViewMaster::LeftStereoProjectionMatrix : GoogleCardboardViewerPreviews::ViewMaster::RightStereoProjectionMatrix;
	case EVP_SnailVR:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::SnailVR::LeftStereoProjectionMatrix : GoogleCardboardViewerPreviews::SnailVR::RightStereoProjectionMatrix;
	case EVP_RiTech2:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::RiTech2::LeftStereoProjectionMatrix : GoogleCardboardViewerPreviews::RiTech2::RightStereoProjectionMatrix;
	case EVP_Go4DC1Glass:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::Go4DC1Glass::LeftStereoProjectionMatrix : GoogleCardboardViewerPreviews::Go4DC1Glass::RightStereoProjectionMatrix;

#endif

	case EVP_None:
	default:
		check(0);
		return FMatrix();
	}
}

uint32 FGoogleVRHMD::GetPreviewViewerNumVertices(enum EStereoscopicPass StereoPass)
{
	switch(GetPreviewViewerType())
	{
#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS

	case EVP_GoogleCardboard1:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::GoogleCardboard1::NumLeftVertices : GoogleCardboardViewerPreviews::GoogleCardboard1::NumRightVertices;
	case EVP_GoogleCardboard2:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::GoogleCardboard2::NumLeftVertices : GoogleCardboardViewerPreviews::GoogleCardboard2::NumRightVertices;
	case EVP_ViewMaster:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::ViewMaster::NumLeftVertices : GoogleCardboardViewerPreviews::ViewMaster::NumRightVertices;
	case EVP_SnailVR:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::SnailVR::NumLeftVertices : GoogleCardboardViewerPreviews::SnailVR::NumRightVertices;
	case EVP_RiTech2:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::RiTech2::NumLeftVertices : GoogleCardboardViewerPreviews::RiTech2::NumRightVertices;
	case EVP_Go4DC1Glass:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::Go4DC1Glass::NumLeftVertices : GoogleCardboardViewerPreviews::Go4DC1Glass::NumRightVertices;

#endif

	case EVP_None:
	default:
		check(0);
		return 0;
	}
}

const FDistortionVertex* FGoogleVRHMD::GetPreviewViewerVertices(enum EStereoscopicPass StereoPass)
{
	switch(GetPreviewViewerType())
	{
#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS

	case EVP_GoogleCardboard1:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::GoogleCardboard1::LeftVertices : GoogleCardboardViewerPreviews::GoogleCardboard1::RightVertices;
	case EVP_GoogleCardboard2:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::GoogleCardboard2::LeftVertices : GoogleCardboardViewerPreviews::GoogleCardboard2::RightVertices;
	case EVP_ViewMaster:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::ViewMaster::LeftVertices : GoogleCardboardViewerPreviews::ViewMaster::RightVertices;
	case EVP_SnailVR:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::SnailVR::LeftVertices : GoogleCardboardViewerPreviews::SnailVR::RightVertices;
	case EVP_RiTech2:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::RiTech2::LeftVertices : GoogleCardboardViewerPreviews::RiTech2::RightVertices;
	case EVP_Go4DC1Glass:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::Go4DC1Glass::LeftVertices : GoogleCardboardViewerPreviews::Go4DC1Glass::RightVertices;

#endif

	case EVP_None:
	default:
		check(0);
		return nullptr;
	}
}

//////////////////////////////////////////////////////
// Begin ISceneViewExtension Pure-Virtual Interface //
//////////////////////////////////////////////////////

// ------  Called on Game Thread ------
void FGoogleVRHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	InViewFamily.EngineShowFlags.MotionBlur = 0;
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// Enbale Unreal PostProcessing Distortion when not using GVR Distortion.
	InViewFamily.EngineShowFlags.HMDDistortion = bDistortionCorrectionEnabled && !IsUsingGVRApiDistortionCorrection();
#else
	InViewFamily.EngineShowFlags.HMDDistortion = bDistortionCorrectionEnabled && (GetPreviewViewerType() != EVP_None);
#endif
	InViewFamily.EngineShowFlags.ScreenPercentage = false;
	InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();
}

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
FIntRect FGoogleVRHMD::CalculateGVRViewportRect(int RenderTargetSizeX, int RenderTargetSizeY, EStereoscopicPass StereoPassType)
{
	switch(StereoPassType)
	{
		case EStereoscopicPass::eSSP_LEFT_EYE:
			gvr_buffer_viewport_list_get_item(ActiveViewportList, 0, ScratchViewport);
			break;
		case EStereoscopicPass::eSSP_RIGHT_EYE:
			gvr_buffer_viewport_list_get_item(ActiveViewportList, 1, ScratchViewport);
			break;
		default:
			// We shouldn't got here.
			check(false);
			break;
	}
	gvr_rectf GVRRect = gvr_buffer_viewport_get_source_uv(ScratchViewport);
	int Left = static_cast<int>(GVRRect.left * RenderTargetSizeX);
	int Bottom = static_cast<int>(GVRRect.bottom * RenderTargetSizeY);
	int Right = static_cast<int>(GVRRect.right * RenderTargetSizeX);
	int Top = static_cast<int>(GVRRect.top * RenderTargetSizeY);

	//UE_LOG(LogTemp, Log, TEXT("Set Viewport Rect to %d, %d, %d, %d for render target size %d x %d"), Left, Bottom, Right, Top, RenderTargetSizeX, RenderTargetSizeY);
	return FIntRect(Left, Bottom, Right, Top);
}
#endif

void FGoogleVRHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	InView.BaseHmdOrientation = CurHmdOrientation;
	InView.BaseHmdLocation = CurHmdPosition;

	InViewFamily.bUseSeparateRenderTarget = ShouldUseSeparateRenderTarget();

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// We should have a valid GVRRenderTargetSize here.
	check(GVRRenderTargetSize.X != 0 && GVRRenderTargetSize.Y != 0);
	InView.ViewRect = CalculateGVRViewportRect(GVRRenderTargetSize.X, GVRRenderTargetSize.Y, InView.StereoPass);
#endif
}

void FGoogleVRHMD::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// Note that we force not enqueue the CachedHeadPose when start loading a map until a new game frame started.
	// This is for solving the one frame flickering issue when load another level due to that there is one frame
	// the scene is rendered before the camera is updated.
	// TODO: We need to investigate a better solution here.
	if(CustomPresent && !bForceStopPresentScene && !bDebugShowGVRSplash)
	{
		CustomPresent->UpdateRenderingPose(CachedHeadPose);
	}
#endif
}

// ------  Called on Render Thread ------

void FGoogleVRHMD::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if(CustomPresent)
	{
		CustomPresent->BeginRendering();
	}
#endif
}

void FGoogleVRHMD::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
}

////////////////////////////////////////////////////////////////////
// Begin FGoogleVRHMD IStereoRendering Pure-Virtual Interface //
////////////////////////////////////////////////////////////////////

bool FGoogleVRHMD::IsStereoEnabled() const
{
	return bStereoEnabled && bHMDEnabled;
}

bool FGoogleVRHMD::EnableStereo(bool stereo)
{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	// We will not allow stereo rendering to be disabled when using async reprojection
	if(bUseOffscreenFramebuffers && !stereo)
	{
		UE_LOG(LogHMD, Warning, TEXT("Attemp to disable stereo rendering when using async reprojection. This is not support so the operation will be ignored!"));
		return true;
	}
	AndroidThunkCpp_UiLayer_SetEnabled(stereo);
#endif

	bStereoEnabled = stereo;
	GEngine->bForceDisableFrameRateSmoothing = bStereoEnabled;
	return bStereoEnabled;
}

void FGoogleVRHMD::AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	SizeX = SizeX / 2;
	if( StereoPass == eSSP_RIGHT_EYE )
	{
		X += SizeX;
	}
}

void FGoogleVRHMD::CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, const FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if( StereoPassType != eSSP_FULL)
	{
		const float EyeOffset = (GetInterpupillaryDistance() * 0.5f) * WorldToMeters;
		const float PassOffset = (StereoPassType == eSSP_LEFT_EYE) ? -EyeOffset : EyeOffset;
		ViewLocation += ViewRotation.Quaternion().RotateVector(FVector(0,PassOffset,0));
	}
#else
	if( StereoPassType != eSSP_FULL)
	{
		if(GetPreviewViewerType() != EVP_None)
		{
			const float EyeOffset = (GetPreviewViewerInterpupillaryDistance() * 0.5f) * WorldToMeters;
			const float PassOffset = (StereoPassType == eSSP_LEFT_EYE) ? -EyeOffset : EyeOffset;
			ViewLocation += ViewRotation.Quaternion().RotateVector(FVector(0,PassOffset,0));
		}
		else
		{
			// Copied from SimpleHMD
			float EyeOffset = 3.20000005f;
			const float PassOffset = (StereoPassType == eSSP_LEFT_EYE) ? EyeOffset : -EyeOffset;
			ViewLocation += ViewRotation.Quaternion().RotateVector(FVector(0,PassOffset,0));
		}
	}
#endif
}

FMatrix FGoogleVRHMD::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType, const float FOV) const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	switch(StereoPassType)
	{
		case EStereoscopicPass::eSSP_LEFT_EYE:
			gvr_buffer_viewport_list_get_item(ActiveViewportList, 0, ScratchViewport);
			break;
		case EStereoscopicPass::eSSP_RIGHT_EYE:
			gvr_buffer_viewport_list_get_item(ActiveViewportList, 1, ScratchViewport);
			break;
		default:
			// We shouldn't got here.
			check(false);
			break;
	}

	gvr_rectf EyeFov = gvr_buffer_viewport_get_source_fov(ScratchViewport);

	// Have to flip left/right and top/bottom to match UE4 expectations
	float Right = FPlatformMath::Tan(FMath::DegreesToRadians(EyeFov.left));
	float Left = -FPlatformMath::Tan(FMath::DegreesToRadians(EyeFov.right));
	float Bottom = -FPlatformMath::Tan(FMath::DegreesToRadians(EyeFov.top));
	float Top = FPlatformMath::Tan(FMath::DegreesToRadians(EyeFov.bottom));

	float ZNear = GNearClippingPlane;

	float SumRL = (Right + Left);
	float SumTB = (Top + Bottom);
	float InvRL = (1.0f / (Right - Left));
	float InvTB = (1.0f / (Top - Bottom));

#if LOG_VIEWER_DATA_FOR_GENERATION

	FPlane Plane0 = FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f);
	FPlane Plane1 = FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f);
	FPlane Plane2 = FPlane((SumRL * InvRL), (SumTB * InvTB), 0.0f, 1.0f);
	FPlane Plane3 = FPlane(0.0f, 0.0f, ZNear, 0.0f);

	const TCHAR* EyeString = StereoPassType == eSSP_LEFT_EYE ? TEXT("Left") : TEXT("Right");
	UE_LOG(LogHMD, Log, TEXT("===== Begin Projection Matrix Eye %s"), EyeString);
	UE_LOG(LogHMD, Log, TEXT("const FMatrix %sStereoProjectionMatrix = FMatrix("), EyeString);
	UE_LOG(LogHMD, Log, TEXT("FPlane(%ff,  0.0f, 0.0f, 0.0f),"), Plane0.X);
	UE_LOG(LogHMD, Log, TEXT("FPlane(0.0f, %ff,  0.0f, 0.0f),"), Plane1.Y);
	UE_LOG(LogHMD, Log, TEXT("FPlane(%ff,  %ff,  0.0f, 1.0f),"), Plane2.X, Plane2.Y);
	UE_LOG(LogHMD, Log, TEXT("FPlane(0.0f, 0.0f, %ff,  0.0f)"), Plane3.Z);
	UE_LOG(LogHMD, Log, TEXT(");"));
	UE_LOG(LogHMD, Log, TEXT("===== End Projection Matrix Eye %s"));

	return FMatrix(
		Plane0,
		Plane1,
		Plane2,
		Plane3
		);

#else

	return FMatrix(
		FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f),
		FPlane((SumRL * InvRL), (SumTB * InvTB), 0.0f, 1.0f),
		FPlane(0.0f, 0.0f, ZNear, 0.0f)
		);

#endif // LOG_VIEWER_DATA_FOR_GENERATION

#else //!GOOGLEVRHMD_SUPPORTED_PLATFORMS

	if(GetPreviewViewerType() == EVP_None)
	{
		// Test data copied from SimpleHMD
		const float ProjectionCenterOffset = 0.151976421f;
		const float PassProjectionOffset = (StereoPassType == eSSP_LEFT_EYE) ? ProjectionCenterOffset : -ProjectionCenterOffset;

		const float HalfFov = 2.19686294f / 2.f;
		const float InWidth = 640.f;
		const float InHeight = 480.f;
		const float XS = 1.0f / tan(HalfFov);
		const float YS = InWidth / tan(HalfFov) / InHeight;

		const float InNearZ = GNearClippingPlane;
		return FMatrix(
			FPlane(XS,                      0.0f,								    0.0f,							0.0f),
			FPlane(0.0f,					YS,	                                    0.0f,							0.0f),
			FPlane(0.0f,	                0.0f,								    0.0f,							1.0f),
			FPlane(0.0f,					0.0f,								    InNearZ,						0.0f))

			* FTranslationMatrix(FVector(PassProjectionOffset,0,0));
	}
	else
	{
		return GetPreviewViewerStereoProjectionMatrix(StereoPassType);
	}

#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS
}

void FGoogleVRHMD::InitCanvasFromView(class FSceneView* InView, class UCanvas* Canvas)
{
}

///////////////////////////////////////////////////////////////
// Begin FGoogleVRHMD IStereoRendering Virtual Interface //
///////////////////////////////////////////////////////////////

void FGoogleVRHMD::GetEyeRenderParams_RenderThread(const struct FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	if (Context.View.StereoPass == eSSP_LEFT_EYE)
	{
		EyeToSrcUVOffsetValue.X = 0.0f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
	else
	{
		EyeToSrcUVOffsetValue.X = 0.5f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
}

void FGoogleVRHMD::UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& InViewport, class SViewport* ViewportWidget)
{
	check(IsInGameThread());

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	FRHIViewport* const ViewportRHI = InViewport.GetViewportRHI().GetReference();

	if (!IsStereoEnabled() || !CustomPresent)
	{
		ViewportRHI->SetCustomPresent(nullptr);
		return;
	}

	check(CustomPresent);

	CustomPresent->UpdateViewport(InViewport, ViewportRHI);

#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS
}

void FGoogleVRHMD::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread());

	// Change the render target size when it is valid
	if (GVRRenderTargetSize.X != 0 && GVRRenderTargetSize.Y != 0)
	{
		InOutSizeX = GVRRenderTargetSize.X;
		InOutSizeY = GVRRenderTargetSize.Y;
	}
}

bool FGoogleVRHMD::NeedReAllocateViewportRenderTarget(const class FViewport& Viewport)
{
	check(IsInGameThread());

	if (IsStereoEnabled())
	{
		const uint32 InSizeX = Viewport.GetSizeXY().X;
		const uint32 InSizeY = Viewport.GetSizeXY().Y;
		FIntPoint RenderTargetSize;
		RenderTargetSize.X = Viewport.GetRenderTargetTexture()->GetSizeX();
		RenderTargetSize.Y = Viewport.GetRenderTargetTexture()->GetSizeY();

		uint32 NewSizeX = InSizeX, NewSizeY = InSizeY;
		CalculateRenderTargetSize(Viewport, NewSizeX, NewSizeY);
		if (NewSizeX != RenderTargetSize.X || NewSizeY != RenderTargetSize.Y)
		{
			UE_LOG(LogHMD, Log, TEXT("NeedReAllocateViewportRenderTarget() Needs realloc to new size: (%d, %d)"), NewSizeX, NewSizeY);
			return true;
		}
	}
	return false;
}

bool FGoogleVRHMD::ShouldUseSeparateRenderTarget() const
{
	check(IsInGameThread());
	return IsStereoEnabled() && bUseGVRApiDistortionCorrection;
}

void FGoogleVRHMD::SetClippingPlanes(float NCP, float FCP)
{
}

///////////////////////////////////////////////////////////////////////
// Begin FGoogleVRHMD IHeadMountedDisplay Pure-Virtual Interface //
///////////////////////////////////////////////////////////////////////

bool FGoogleVRHMD::IsHMDConnected()
{
	// Just uses regular screen, so this is always true!
	return true;
}

bool FGoogleVRHMD::IsHMDEnabled() const
{
	return bHMDEnabled;
}

void FGoogleVRHMD::EnableHMD(bool bEnable)
{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	// We will not allow stereo rendering to be disabled when using async reprojection
	if(bUseOffscreenFramebuffers && !bEnable)
	{
		UE_LOG(LogHMD, Warning, TEXT("Attemp to disable HMD when using async reprojection. This is not support so the operation will be ignored!"));
		return;
	}
#endif
	bHMDEnabled = bEnable;
	if(!bHMDEnabled)
	{
		EnableStereo(false);
	}
}

EHMDDeviceType::Type FGoogleVRHMD::GetHMDDeviceType() const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	return EHMDDeviceType::DT_ES2GenericStereoMesh;
#else
	return EHMDDeviceType::DT_GoogleVR; // Workaround needed for non-es2 post processing to call PostProcessHMD
#endif
}

bool FGoogleVRHMD::GetHMDMonitorInfo(MonitorInfo& OutMonitorInfo)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if(!IsStereoEnabled())
	{
		return false;
	}

	OutMonitorInfo.MonitorName = FString::Printf(TEXT("%s - %s"), UTF8_TO_TCHAR(gvr_get_viewer_vendor(GVRAPI)), UTF8_TO_TCHAR(gvr_get_viewer_model(GVRAPI)));
	OutMonitorInfo.MonitorId = 0;
	OutMonitorInfo.DesktopX = OutMonitorInfo.DesktopY = OutMonitorInfo.ResolutionX = OutMonitorInfo.ResolutionY = 0;
	OutMonitorInfo.WindowSizeX = OutMonitorInfo.WindowSizeY = 0;

	// For proper scaling, and since hardware scaling is used, return the calculated size and not the actual device size
	// TODO: We are using the screen resolution to tune the rendering scale. Revisit here if we want to hook up the gvr
	// gvr_get_recommended_render_target_size function.
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	FPlatformRect Rect = FAndroidWindow::GetScreenRect();
	OutMonitorInfo.ResolutionX = Rect.Right - Rect.Left;
	OutMonitorInfo.ResolutionY = Rect.Bottom - Rect.Top;
#elif GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
	FPlatformRect Rect = FIOSWindow::GetScreenRect();
	OutMonitorInfo.ResolutionX = Rect.Right - Rect.Left;
	OutMonitorInfo.ResolutionY = Rect.Bottom - Rect.Top;
#else
	gvr_recti Bounds = gvr_get_window_bounds(GVRAPI);
	OutMonitorInfo.ResolutionX = Bounds.right - Bounds.left;
	OutMonitorInfo.ResolutionY = Bounds.top - Bounds.bottom;
#endif

	return true;
#else
	OutMonitorInfo.MonitorName = "UnsupportedGoogleVRHMDPlatform";
	OutMonitorInfo.MonitorId = 0;
	OutMonitorInfo.DesktopX = OutMonitorInfo.DesktopY = OutMonitorInfo.ResolutionX = OutMonitorInfo.ResolutionY = 0;
	OutMonitorInfo.WindowSizeX = OutMonitorInfo.WindowSizeY = 0;

	return false;
#endif
}

void FGoogleVRHMD::GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const
{
	InOutHFOVInDegrees = 0.0f;
	InOutVFOVInDegrees = 0.0f;
}

bool FGoogleVRHMD::DoesSupportPositionalTracking() const
{
	// Does not support position tracking, only pose
	return false;
}

bool FGoogleVRHMD::HasValidTrackingPosition()
{
	// Does not support position tracking, only pose
	return false;
}

void FGoogleVRHMD::GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const
{
	// Does not support position tracking, only pose
}

void FGoogleVRHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
	// Nothing
}

float FGoogleVRHMD::GetInterpupillaryDistance() const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// For simplicity, the interpupillary distance is the distance to the left eye, doubled.
	gvr_mat4f EyeMat = gvr_get_eye_from_head_matrix(GVRAPI, GVR_LEFT_EYE);
	FVector Offset = FVector(-EyeMat.m[2][3], EyeMat.m[0][3], EyeMat.m[1][3]);
#if LOG_VIEWER_DATA_FOR_GENERATION
	UE_LOG(LogHMD, Log, TEXT("===== Begin Interpupillary Distance"));
	UE_LOG(LogHMD, Log, TEXT("const float InterpupillaryDistance = %ff;"), Offset.Size() * 2.0f);
	UE_LOG(LogHMD, Log, TEXT("===== End Interpupillary Distance"));
#endif

	return Offset.Size() * 2.0f;
#else //!GOOGLEVRHMD_SUPPORTED_PLATFORMS
	return GetPreviewViewerInterpupillaryDistance();
#endif
}

void FGoogleVRHMD::GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	GetCurrentPose(CurrentOrientation, CurrentPosition);
	CurHmdOrientation = CurrentOrientation;
	CurHmdPosition = CurrentPosition;
}

void FGoogleVRHMD::RebaseObjectOrientationAndPosition(FVector& Position, FQuat& Orientation) const
{
}

TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> FGoogleVRHMD::GetViewExtension()
{
	TSharedPtr<FGoogleVRHMD, ESPMode::ThreadSafe> ptr(AsShared());
	return StaticCastSharedPtr<ISceneViewExtension>(ptr);
}

void FGoogleVRHMD::ApplyHmdRotation(class APlayerController* PC, FRotator& ViewRotation)
{
	ViewRotation.Normalize();

	GetCurrentPose(CurHmdOrientation, CurHmdPosition);

	const FRotator DeltaRot = ViewRotation - PC->GetControlRotation();
	DeltaControlRotation = (DeltaControlRotation + DeltaRot).GetNormalized();

	// Pitch from other sources is never good, because there is an absolute up and down that must be respected to avoid motion sickness.
	// Same with roll.
	DeltaControlRotation.Pitch = 0;
	DeltaControlRotation.Roll = 0;
	DeltaControlOrientation = DeltaControlRotation.Quaternion();

	ViewRotation = FRotator(DeltaControlOrientation * CurHmdOrientation);
}

bool FGoogleVRHMD::UpdatePlayerCamera(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	GetCurrentPose(CurrentOrientation, CurrentPosition);
	CurHmdOrientation = CurrentOrientation;
	CurHmdPosition = CurrentPosition;

	return true;
}

bool FGoogleVRHMD::IsChromaAbCorrectionEnabled() const
{
	return false;
}

bool FGoogleVRHMD::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FParse::Command(&Cmd, TEXT("HMD")))
	{
		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			EnableHMD(true);
			return true;
		}
		else if(FParse::Command(&Cmd, TEXT("OFF")))
		{
			EnableHMD(false);
			return true;
		}
	}
	else if (FParse::Command(&Cmd, TEXT("STEREO")))
	{
		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			EnableStereo(true);
			return true;
		}
		else if(FParse::Command(&Cmd, TEXT("OFF")))
		{
			EnableStereo(false);
			return true;
		}
	}
	else if(FParse::Command(&Cmd, TEXT("DISTORT")))
	{
		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			SetDistortionCorrectionEnabled(true);
			return true;
		}
		else if(FParse::Command(&Cmd, TEXT("OFF")))
		{
			SetDistortionCorrectionEnabled(false);
			return true;
		}
		else if(FParse::Command(&Cmd, TEXT("PPHMD")))
		{
			SetDistortionCorrectionMethod(false);
		}
		else if(FParse::Command(&Cmd, TEXT("GVRAPI")))
		{
			SetDistortionCorrectionMethod(true);
		}
	}
	else if (FParse::Command(&Cmd, TEXT("GVRRENDERSIZE")))
	{
		int Width, Height;
		float ScaleFactor;
		FIntPoint ActualSize;
		if (FParse::Value(Cmd, TEXT("W="), Width) && FParse::Value(Cmd, TEXT("H="), Height))
		{
			SetGVRHMDRenderTargetSize(Width, Height, ActualSize);
			return true;
		}
		else if (FParse::Value(Cmd, TEXT("S="), ScaleFactor))
		{
			SetGVRHMDRenderTargetSize(ScaleFactor, ActualSize);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("RESET")))
		{
			SetRenderTargetSizeToDefault();
			return true;
		}

		return false;
	}
	else if (FParse::Command(&Cmd, TEXT("GVRNECKMODELSCALE")))
	{
		float ScaleFactor;
		if (FParse::Value(Cmd, TEXT("Factor="), ScaleFactor))
		{
			SetNeckModelScale(ScaleFactor);
			return true;
		}

		return false;
	}
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// Tune the distortion mesh vert count when use Unreal's PostProcessing Distortion
	else if (FParse::Command(&Cmd, TEXT("DISTORTMESH")))
	{
		if (FParse::Command(&Cmd, TEXT("VERYSMALL")))
		{
			SetDistortionMeshSize(EDistortionMeshSizeEnum::DMS_VERYSMALL);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("SMALL")))
		{
			SetDistortionMeshSize(EDistortionMeshSizeEnum::DMS_SMALL);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("MEDIUM")))
		{
			SetDistortionMeshSize(EDistortionMeshSizeEnum::DMS_MEDIUM);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("LARGE")))
		{
			SetDistortionMeshSize(EDistortionMeshSizeEnum::DMS_LARGE);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("VERYLARGE")))
		{
			SetDistortionMeshSize(EDistortionMeshSizeEnum::DMS_VERYLARGE);
			return true;
		}
	}
	else if (FParse::Command(&Cmd, TEXT("GVRSPLASH")))
	{
		if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			if (GVRSplash.IsValid())
			{
				GVRSplash->Show();
				bDebugShowGVRSplash = true;
			}
		}
		else if (FParse::Command(&Cmd, TEXT("HIDE")))
		{
			if (GVRSplash.IsValid())
			{
				GVRSplash->Hide();
				bDebugShowGVRSplash = false;
			}
		}
	}
#endif

	return false;
}

bool FGoogleVRHMD::IsPositionalTrackingEnabled() const
{
	// Does not support position tracking, only pose
	return false;
}

bool FGoogleVRHMD::EnablePositionalTracking(bool enable)
{
	// Does not support position tracking, only pose
	return false;
}

bool FGoogleVRHMD::IsHeadTrackingAllowed() const
{
	return true;
}

bool FGoogleVRHMD::IsInLowPersistenceMode() const
{
	return false;
}

void FGoogleVRHMD::EnableLowPersistenceMode(bool Enable)
{
}

void FGoogleVRHMD::ResetOrientationAndPosition(float Yaw)
{
	ResetOrientation(Yaw);
	ResetPosition();
}

//////////////////////////////////////////////////////////////////
// Begin FGoogleVRHMD IHeadMountedDisplay Virtual Interface //
//////////////////////////////////////////////////////////////////

void FGoogleVRHMD::ResetOrientation(float Yaw)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr_reset_tracking(GVRAPI);
#else
	PoseYaw = 0;
#endif
	SetBaseOrientation(FRotator(0.0f, Yaw, 0.0f).Quaternion());
}

void FGoogleVRHMD::ResetPosition()
{
}

void FGoogleVRHMD::SetBaseRotation(const FRotator& BaseRot)
{
	SetBaseOrientation(FRotator(0.0f, BaseRot.Yaw, 0.0f).Quaternion());
}

FRotator FGoogleVRHMD::GetBaseRotation() const
{
	return GetBaseOrientation().Rotator();
}

void FGoogleVRHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
	BaseOrientation = BaseOrient;
}

FQuat FGoogleVRHMD::GetBaseOrientation() const
{
	return BaseOrientation;
}

bool FGoogleVRHMD::HandleInputKey(UPlayerInput *, const FKey & Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	if (Key == EKeys::Android_Back)
	{
		if (EventType == IE_Pressed)
		{
			BackbuttonPressTime = FPlatformTime::Seconds();
		}
		else if (EventType == IE_Released)
		{
			if (FPlatformTime::Seconds() - BackbuttonPressTime < BACK_BUTTON_SHORT_PRESS_TIME)
			{
				// Add default back button behavior in Daydream Mode
				if (bIsInDaydreamMode)
				{
					AndroidThunkCpp_QuitDaydreamApplication();
				}
			}
		}
		return true;
	}
#endif
	return false;
}

bool FGoogleVRHMD::HandleInputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex)
{
	return false;
}

void FGoogleVRHMD::UpdateHeadPose()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// Update CachedHeadPose
	CachedFuturePoseTime = gvr_get_time_point_now();
	CachedFuturePoseTime.monotonic_system_time_nanos += kPredictionTime;

	CachedHeadPose = gvr_get_head_space_from_start_space_rotation(GVRAPI, CachedFuturePoseTime);

	// Apply the neck model to calculate the final pose
	gvr_mat4f FinalHeadPose = gvr_apply_neck_model(GVRAPI, CachedHeadPose, NeckModelScale);

	// Convert the final pose into Unreal data type
	FMatrix FinalHeadPoseUnreal;
	memcpy(FinalHeadPoseUnreal.M[0], FinalHeadPose.m[0], 4 * 4 * sizeof(float));

	// Inverse the view matrix so we can get the world position of the pose
	FMatrix FinalHeadPoseInverseUnreal = FinalHeadPoseUnreal.Inverse();

	// Number of Unreal Units per meter.
	const float WorldToMetersScale = GetWorldToMetersScale();

	// Gvr is using a openGl Right Handed coordinate system, UE is left handed.
	// The following code is converting the gvr coordinate system to UE coordinates.

	// Gvr: Negative Z is Forward, UE: Positive X is Forward.
	CachedFinalHeadPosition.X = -FinalHeadPoseInverseUnreal.M[2][3] * WorldToMetersScale;

	// Gvr: Positive X is Right, UE: Positive Y is Right.
	CachedFinalHeadPosition.Y = FinalHeadPoseInverseUnreal.M[0][3] * WorldToMetersScale;

	// Gvr: Positive Y is Up, UE: Positive Z is Up
	CachedFinalHeadPosition.Z = FinalHeadPoseInverseUnreal.M[1][3] * WorldToMetersScale;

	// Convert Gvr right handed coordinate system rotation into UE left handed coordinate system.
	CachedFinalHeadRotation = FQuat(FinalHeadPoseUnreal);
	CachedFinalHeadRotation = FQuat(-CachedFinalHeadRotation.Z, CachedFinalHeadRotation.X, CachedFinalHeadRotation.Y, -CachedFinalHeadRotation.W);
#endif
}

bool FGoogleVRHMD::OnStartGameFrame( FWorldContext& WorldContext )
{
	//UE_LOG(LogTemp, Log, TEXT("Start Game Frame!!!"));

	// Handle back coming from viewer magnet clickers or ui layer
	if(bBackDetected)
	{
		HandleGVRBackEvent();
		bBackDetected = false;
	}

	if(bTriggerDetected)
	{
		if( GEngine &&
			GEngine->GameViewport &&
			GEngine->GameViewport->Viewport &&
			GEngine->GameViewport->Viewport->GetClient() )
		{
			GEngine->GameViewport->Viewport->GetClient()->InputTouch(GEngine->GameViewport->Viewport, 0, 0, ETouchType::Began, FVector2D(-1, -1), FDateTime::Now(), 0);
			GEngine->GameViewport->Viewport->GetClient()->InputTouch(GEngine->GameViewport->Viewport, 0, 0, ETouchType::Ended, FVector2D(-1, -1), FDateTime::Now(), 0);
		}
		bTriggerDetected = false;
	}

	//Update the head pose at the begnning of a frame. This headpose will be used for both simulation and rendering.
	UpdateHeadPose();

	// Update ViewportList from GVR API
	UpdateGVRViewportList();

	// Enable scene present after OnStartGameFrame get called.
	bForceStopPresentScene = false;
	return false;
}

bool FGoogleVRHMD::OnEndGameFrame( FWorldContext& WorldContext )
{
	return false;
}

void FGoogleVRHMD::RecordAnalytics()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	if (FEngineAnalytics::IsAvailable())
	{
		// prepare and send analytics data
		TArray<FAnalyticsEventAttribute> EventAttributes;

		IHeadMountedDisplay::MonitorInfo MonitorInfo;
		GetHMDMonitorInfo(MonitorInfo);

		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DeviceName"), TEXT("GoogleVRHMD")));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayDeviceName"), *MonitorInfo.MonitorName));
		//EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayId"), MonitorInfo.MonitorId)); // Doesn't compile on iOS for some reason
		FString MonResolution(FString::Printf(TEXT("(%d, %d)"), MonitorInfo.ResolutionX, MonitorInfo.ResolutionY));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Resolution"), MonResolution));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("InterpupillaryDistance"), GetInterpupillaryDistance()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ChromaAbCorrectionEnabled"), IsChromaAbCorrectionEnabled()));

		FString OutStr(TEXT("Editor.VR.DeviceInitialised"));
		FEngineAnalytics::GetProvider().RecordEvent(OutStr, EventAttributes);
	}

#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS
}

FString FGoogleVRHMD::GetVersionString() const
{
	FString s = FString::Printf(TEXT("GoogleVR - %s, VrLib: %s, built %s, %s"), *FEngineVersion::Current().ToString(), TEXT("GVR"),
		UTF8_TO_TCHAR(__DATE__), UTF8_TO_TCHAR(__TIME__));
	return s;
}