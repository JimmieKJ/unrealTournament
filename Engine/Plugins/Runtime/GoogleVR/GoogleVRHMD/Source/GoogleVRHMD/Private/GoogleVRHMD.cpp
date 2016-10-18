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

#include "GoogleVRHMDPCH.h"
#include "GoogleVRHMD.h"
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
static uint32 OffscreenBufferMSAASetting = 2;
#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS

// Only one HMD can be active at a time, so using file static to track this for transferring to game thread
static bool bBackDetected = false;
static bool bTriggerDetected = false;

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

bool AndroidThunkCpp_IsScanlineRacingEnabled()
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_IsScanlineRacingEnabled", "()Z", false);
		return FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, Method);
 	}

	return false;
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
	virtual FString GetModulePriorityKeyName() const override
	{
		return TEXT("GoogleVRHMD");
	}

	/**
	 * Attempts to create a new head tracking device interface
	 *
	 * @return	Interface to the new head tracking device, if we were able to successfully create one
	 */
	virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay() override;
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
	: bStereoEnabled(false)
	, bHMDEnabled(false)
	, bDistortionCorrectionEnabled(true)
	, bUseGVRApiDistortionCorrection(true)
	, bUseOffscreenFramebuffers(true)
	, CurHmdOrientation(FQuat::Identity)
	, LastHmdOrientation(FQuat::Identity)
	, DeltaControlRotation(FRotator::ZeroRotator)
	, DeltaControlOrientation(FQuat::Identity)
	, BaseOrientation(FQuat::Identity)
	, RendererModule(nullptr)
	, DistortionMeshIndices(nullptr)
	, DistortionMeshVerticesLeftEye(nullptr)
	, DistortionMeshVerticesRightEye(nullptr)
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	, CustomPresent(nullptr)
#endif
#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
	, OverlayView(nil)
#endif
	, LastUpdatedCacheFrame(0)
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	, CachedDistortedRenderTextureParams(nullptr)
	, CachedNonDistortedRenderTextureParams(nullptr)
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

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

		// Initialize OpenGL resources for API
		gvr_initialize_gl(GVRAPI);

		// Set default framebuffer
		gvr_set_default_framebuffer_active(GVRAPI);

		// Log the current viewer
		UE_LOG(LogHMD, Log, TEXT("The current viewer is %s"), UTF8_TO_TCHAR(gvr_get_viewer_model(GVRAPI)));

#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

		// Fix some flags based on deployment mode for Android
		// Also disable GVR Distortion if scanline racing is not enabled.
		if(AndroidThunkCpp_ProjectWantsCardboardOnlyMode() || !AndroidThunkCpp_IsScanlineRacingEnabled())
		{
			bUseOffscreenFramebuffers = false;
			bUseGVRApiDistortionCorrection = false;
		}

		// Show ui on Android
		AndroidThunkCpp_UiLayer_SetEnabled(true);
		AndroidThunkCpp_UiLayer_SetViewerName(UTF8_TO_TCHAR(gvr_get_viewer_model(GVRAPI)));

		// Set Hardware Scaling in GvrLayout
		AndroidThunkCpp_GvrLayout_SetFixedPresentationSurfaceSizeToCurrent();

#elif GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS

		// No support for these
		bUseOffscreenFramebuffers = false;
		bUseGVRApiDistortionCorrection = false;

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

		// Create custom present class
		CustomPresent = new FGoogleVRHMDCustomPresent(this);

		// Get initial render params!
		ConditionalUpdateCache();

#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS

		// Enabled by default
		EnableHMD(true);
		EnableStereo(true);

		// Get renderer module
		static const FName RendererModuleName("Renderer");
		RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		check(RendererModule);

		// Initialize distortion mesh and indices
		SetNumOfDistortionPoints(DistortionPointsX, DistortionPointsY);
	}
	else
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("GoogleVR context failed to create successfully."));
	}
}

FGoogleVRHMD::~FGoogleVRHMD()
{
	delete [] DistortionMeshIndices;
	DistortionMeshIndices = nullptr;
	delete [] DistortionMeshVerticesLeftEye;
	DistortionMeshVerticesLeftEye = nullptr;
	delete [] DistortionMeshVerticesRightEye;
	DistortionMeshVerticesRightEye = nullptr;

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if(CachedDistortedRenderTextureParams)
	{
		gvr_destroy_render_params_list(&CachedDistortedRenderTextureParams);
		CachedDistortedRenderTextureParams = nullptr;
	}
	if(CachedNonDistortedRenderTextureParams)
	{
		gvr_destroy_render_params_list(&CachedNonDistortedRenderTextureParams);
		CachedNonDistortedRenderTextureParams = nullptr;
	}

	delete CustomPresent;
#endif
}

bool FGoogleVRHMD::IsInitialized() const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	return GVRAPI != nullptr;

#endif

	// Non-supported platform will be PC editor which will always succeed
	return true;
}

void FGoogleVRHMD::GetCurrentPose(FQuat& CurrentOrientation)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	// Get head pose
	const gvr_quatf& HeadRotation = CachedPose.rotation;

	// Set output orientation
	CurrentOrientation = FQuat(HeadRotation.qx, HeadRotation.qy, HeadRotation.qz, HeadRotation.qw);

	// Fix coordinate system for Google -> UE4
	CurrentOrientation = BaseOrientation * FQuat(-CurrentOrientation.Z, CurrentOrientation.X, CurrentOrientation.Y, -CurrentOrientation.W);

#else

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

		CurrentOrientation = FQuat(FRotator(PosePitch, PoseYaw, 0.0f));
	}
	else
	{
		CurrentOrientation = FQuat(FRotator(0.0f, 0.0f, 0.0f));
	}

#endif
}

void FGoogleVRHMD::ConditionalUpdateCache() const
{
	// Google VR doesn't want to check some values more than once per frame.
	if( LastUpdatedCacheFrame == GFrameNumber &&
		LastUpdatedCacheFrame != 0 )
	{
		return;
	}
	
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	// Allocate if necessary!
	if(CachedDistortedRenderTextureParams == nullptr)
	{
		CachedDistortedRenderTextureParams = gvr_create_render_params_list(GVRAPI);
	}
	if(CachedNonDistortedRenderTextureParams == nullptr)
	{
		CachedNonDistortedRenderTextureParams = gvr_create_render_params_list(GVRAPI);
	}

	// Update cache
	CachedFuturePoseTime = gvr_get_time_point_now();
	CachedFuturePoseTime.monotonic_system_time_nanos += 50 * 1000000;
	CachedPose = gvr_get_head_pose_in_start_space(GVRAPI, CachedFuturePoseTime);
	gvr_get_recommended_render_params_list(GVRAPI, CachedDistortedRenderTextureParams);
	gvr_get_screen_render_params_list(GVRAPI, CachedNonDistortedRenderTextureParams);

#endif

	// Update bookkeeping
	LastUpdatedCacheFrame = GFrameNumber;
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
	// Can't turn off distortion correction if using offscreen framebuffers
	if(!bUseOffscreenFramebuffers || !GOOGLEVRHMD_SUPPORTED_PLATFORMS)
	{
		bDistortionCorrectionEnabled = bEnable;
	}
}

void FGoogleVRHMD::SetDistortionCorrectionMethod(bool bInUseGVRApiDistortionCorrection)
{
	bUseGVRApiDistortionCorrection = bInUseGVRApiDistortionCorrection;
}

void FGoogleVRHMD::SetChromaticAberrationCorrectionEnabled(bool bEnable)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr_set_bool_parameter(GVRAPI, GVR_CHROMATIC_ABERRATION_CORRECTION_ENABLED, bEnable);
#endif
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
	// Force non supported platform distortion mesh be 2 x 2.
	// TODO:  Drive this from the SDK when it's ready
#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS
	XPoints = 2;
	YPoints = 2;
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

bool FGoogleVRHMD::GetDistortionCorrectionEnabled() const
{
	return bDistortionCorrectionEnabled;
}

bool FGoogleVRHMD::IsUsingGVRApiDistortionCorrection() const
{
	// Offscreen framebuffers don't support PPHMD
	return bUseOffscreenFramebuffers || bUseGVRApiDistortionCorrection;
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

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
uint32 FGoogleVRHMD::GetOffscreenBufferMSAASetting()
{
	return OffscreenBufferMSAASetting;
}
#endif


//////////////////////////////////////////////////////
// Begin ISceneViewExtension Pure-Virtual Interface //
//////////////////////////////////////////////////////

void FGoogleVRHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	InViewFamily.EngineShowFlags.MotionBlur = 0;
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	InViewFamily.EngineShowFlags.HMDDistortion = bDistortionCorrectionEnabled && !IsUsingGVRApiDistortionCorrection();
#else
	InViewFamily.EngineShowFlags.HMDDistortion = bDistortionCorrectionEnabled && (GetPreviewViewerType() != EVP_None);
#endif
	InViewFamily.EngineShowFlags.ScreenPercentage = false;
	InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();
}

void FGoogleVRHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	InView.BaseHmdOrientation = FQuat(FRotator(0.0f,0.0f,0.0f));
	InView.BaseHmdLocation = FVector(0.f);
//	WorldToMetersScale = InView.WorldToMetersScale;
	InViewFamily.bUseSeparateRenderTarget = ShouldUseSeparateRenderTarget();
}

void FGoogleVRHMD::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FGoogleVRHMD::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
}

void FGoogleVRHMD::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	// Cache frame information now
	ConditionalUpdateCache();

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if(CustomPresent && bUseOffscreenFramebuffers)
	{
		CustomPresent->BeginRendering(RHICmdList, InViewFamily);
	}
#endif
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
	// We will not allow stereo rendering to be disabled when using Scanline Racing
	if(AndroidThunkCpp_IsScanlineRacingEnabled() && !stereo)
	{
		UE_LOG(LogHMD, Warning, TEXT("Attemp to disable stereo rendering when using scanline racing. This is not support so the operation will be ignored!"));
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

	// Get information for the proper eye
	gvr_render_params EyeParams{};
	const gvr_render_params_list* ActiveParams = bDistortionCorrectionEnabled ? CachedDistortedRenderTextureParams : CachedNonDistortedRenderTextureParams;
	bool bValidParamsFound = false;
	for(size_t i = 0; i < gvr_get_render_params_list_size(ActiveParams); ++i)
	{
		EyeParams = gvr_get_render_params(ActiveParams, i);

		if( (EyeParams.eye_type == GVR_LEFT_EYE && StereoPassType == eSSP_LEFT_EYE) ||
			(EyeParams.eye_type == GVR_RIGHT_EYE && StereoPassType == eSSP_RIGHT_EYE) )
		{
			bValidParamsFound = true;
			break;
		}
	}

	// If bValidParamsFound == false, there was an error, either because this function was called with eSSP_FULL or the API didn't have eye data
	check(bValidParamsFound);

	// Have to flip left/right and top/bottom to match UE4 expectations
	float Right = FPlatformMath::Tan(FMath::DegreesToRadians(EyeParams.eye_fov.left));
	float Left = -FPlatformMath::Tan(FMath::DegreesToRadians(EyeParams.eye_fov.right));
	float Bottom = -FPlatformMath::Tan(FMath::DegreesToRadians(EyeParams.eye_fov.top));
	float Top = FPlatformMath::Tan(FMath::DegreesToRadians(EyeParams.eye_fov.bottom));

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

#else

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

	MonitorInfo Info;
	if(GetHMDMonitorInfo(Info))
	{
		InOutSizeX = Info.ResolutionX;
		InOutSizeY = Info.ResolutionY;
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
	return IsStereoEnabled() && (bDistortionCorrectionEnabled || bUseOffscreenFramebuffers);
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
	// We will not allow stereo rendering to be disabled when using Scanline Racing
	if(AndroidThunkCpp_IsScanlineRacingEnabled() && !bEnable)
	{
		UE_LOG(LogHMD, Warning, TEXT("Attemp to disable HMD when using scanline racing. This is not support so the operation will be ignored!"));
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
	return EHMDDeviceType::DT_OculusRift; // Workaround needed for non-es2 post processing to call PostProcessHMD
#endif
}

bool	FGoogleVRHMD::GetHMDMonitorInfo(MonitorInfo& OutMonitorInfo)
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

void	FGoogleVRHMD::GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const
{
	InOutHFOVInDegrees = 0.0f;
	InOutVFOVInDegrees = 0.0f;
}

bool	FGoogleVRHMD::DoesSupportPositionalTracking() const
{
	// Does not support position tracking, only pose
	return false;
}

bool	FGoogleVRHMD::HasValidTrackingPosition()
{
	// Does not support position tracking, only pose
	return false;
}

void	FGoogleVRHMD::GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const
{
	// Does not support position tracking, only pose
}

void	FGoogleVRHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
	// Nothing
}

float	FGoogleVRHMD::GetInterpupillaryDistance() const
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

#else

	return GetPreviewViewerInterpupillaryDistance();

#endif
}

void FGoogleVRHMD::GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	CurrentPosition = FVector(0.0f, 0.0f, 0.0f);

	GetCurrentPose(CurrentOrientation);
	CurHmdOrientation = LastHmdOrientation = CurrentOrientation;
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

	GetCurrentPose(CurHmdOrientation);
	LastHmdOrientation = CurHmdOrientation;

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
	GetCurrentPose(CurHmdOrientation);
	LastHmdOrientation = CurHmdOrientation;

	CurrentOrientation = CurHmdOrientation;
	CurrentPosition = FVector(0.0f, 0.0f, 0.0f);

	return true;
}

bool FGoogleVRHMD::IsChromaAbCorrectionEnabled() const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	return gvr_get_bool_parameter(GVRAPI, GVR_CHROMATIC_ABERRATION_CORRECTION_ENABLED);

#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS

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
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	else if(FParse::Command(&Cmd, TEXT("CABC"))) // ChromaABCorrection
	{
		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			SetChromaticAberrationCorrectionEnabled(true);
			return true;
		}
		else if(FParse::Command(&Cmd, TEXT("OFF")))
		{
			SetChromaticAberrationCorrectionEnabled(false);
			return true;
		}
	}
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
	else if(FParse::Command(&Cmd, TEXT("GVRMSAA"))) // ChromaABCorrection
	{
		if (FParse::Command(&Cmd, TEXT("1")))
		{
			OffscreenBufferMSAASetting = 1;
			return true;
		}
		else if(FParse::Command(&Cmd, TEXT("2")))
		{
			OffscreenBufferMSAASetting = 2;
			return true;
		}
		else if(FParse::Command(&Cmd, TEXT("4")))
		{
			OffscreenBufferMSAASetting = 4;
			return true;
		}
		else if(FParse::Command(&Cmd, TEXT("8")))
		{
			OffscreenBufferMSAASetting = 8;
			return true;
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
	SetBaseOrientation(FRotator(0.0f, Yaw, 0.0f).Quaternion());

#endif
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

bool FGoogleVRHMD::HandleInputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex)
{
	return false;
}

bool FGoogleVRHMD::OnStartGameFrame( FWorldContext& WorldContext )
{
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
