// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HMDPrivatePCH.h"
#include "GearVR.h"
#include "RHIStaticStates.h"

#if GEARVR_SUPPORTED_PLATFORMS

#include "OpenGLDrvPrivate.h"
#include "OpenGLResources.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidEGL.h"

#define NUM_BUFFERS 3

#if !UE_BUILD_SHIPPING
#define GL_CHECK_ERROR \
do \
{ \
int err; \
	while ((err = glGetError()) != GL_NO_ERROR) \
	{ \
		UE_LOG(LogHMD, Warning, TEXT("%s:%d GL error (#%x)\n"), ANSI_TO_TCHAR(__FILE__), __LINE__, err); \
	} \
} while (0) 

#else // #if !UE_BUILD_SHIPPING
#define GL_CHECK_ERROR (void)0
#endif // #if !UE_BUILD_SHIPPING

namespace GearVR
{

class FOpenGLTexture2DSet : public FOpenGLTexture2D
{
public:
	FOpenGLTexture2DSet(
	class FOpenGLDynamicRHI* InGLRHI,
		GLuint InResource,
		GLenum InTarget,
		GLenum InAttachment,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		uint32 InNumSamples,
		uint32 InArraySize,
		EPixelFormat InFormat,
		bool bInCubemap,
		bool bInAllocatedStorage,
		uint32 InFlags,
		uint8* InTextureRange
		)
		: FOpenGLTexture2D(
		InGLRHI,
		InResource,
		InTarget,
		InAttachment,
		InSizeX,
		InSizeY,
		InSizeZ,
		InNumMips,
		InNumSamples,
		InArraySize,
		InFormat,
		bInCubemap,
		bInAllocatedStorage,
		InFlags,
		InTextureRange,
		FClearValueBinding::Black
		)
	{
		ColorTextureSet = nullptr;
		CurrentIndex = TextureCount = 0;
	}
	~FOpenGLTexture2DSet()
	{
		vrapi_DestroyTextureSwapChain(ColorTextureSet);
		Resource = 0;
	}

	void SwitchToNextElement();

	static FOpenGLTexture2DSet* CreateTexture2DSet(
		FOpenGLDynamicRHI* InGLRHI,
		uint32 SizeX, uint32 SizeY,
		uint32 InNumSamples,
		EPixelFormat InFormat,
		uint32 InFlags);

	ovrTextureSwapChain*	GetColorTextureSet() const { return ColorTextureSet; }
	uint32					GetCurrentIndex() const { return CurrentIndex;  }
	uint32					GetTextureCount() const { return TextureCount; }
protected:
	void InitWithCurrentElement();

	uint32					CurrentIndex;
	uint32					TextureCount;
	ovrTextureSwapChain*	ColorTextureSet;
};

} // namespace GearVR

void FOpenGLTexture2DSet::SwitchToNextElement()
{
	CurrentIndex = (CurrentIndex + 1) % TextureCount;
	InitWithCurrentElement();
}

void FOpenGLTexture2DSet::InitWithCurrentElement()
{
	Resource = vrapi_GetTextureSwapChainHandle(ColorTextureSet, CurrentIndex);
}

FOpenGLTexture2DSet* FOpenGLTexture2DSet::CreateTexture2DSet(
	FOpenGLDynamicRHI* InGLRHI,
	uint32 SizeX, uint32 SizeY,
	uint32 InNumSamples,
	EPixelFormat InFormat,
	uint32 InFlags
	)
{
	GLenum Target = (InNumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
	GLenum Attachment = GL_NONE;// GL_COLOR_ATTACHMENT0;
	bool bAllocatedStorage = false;
	uint32 NumMips = 1;
	uint8* TextureRange = nullptr;

	FOpenGLTexture2DSet* NewTextureSet = new FOpenGLTexture2DSet(
		InGLRHI, 0, Target, Attachment, SizeX, SizeY, 0, NumMips, InNumSamples, 1, InFormat, false, bAllocatedStorage, InFlags, TextureRange);

	UE_LOG(LogHMD, Log, TEXT("Allocated textureSet %p (%d x %d), fr = %d"), NewTextureSet, SizeX, SizeY, GFrameNumber);

	NewTextureSet->ColorTextureSet = vrapi_CreateTextureSwapChain(VRAPI_TEXTURE_TYPE_2D, VRAPI_TEXTURE_FORMAT_8888,	SizeX, SizeY, 1, true);
	if (!NewTextureSet->ColorTextureSet)
	{
		// hmmm... can't allocate a texture set for some reasons.
		UE_LOG(LogHMD, Log, TEXT("Can't allocate texture swap chain."));
		delete NewTextureSet;
		return nullptr;
	}
	NewTextureSet->TextureCount = vrapi_GetTextureSwapChainLength(NewTextureSet->ColorTextureSet);

	NewTextureSet->InitWithCurrentElement();
	return NewTextureSet;
}

bool FGearVR::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(Index == 0);
#if !OVR_DEBUG_DRAW
	UE_LOG(LogHMD, Log, TEXT("Allocating Render Target textures"));
	pGearVRBridge->AllocateRenderTargetTexture(SizeX, SizeY, Format, NumMips, Flags, TargetableTextureFlags, OutTargetableTexture, OutShaderResourceTexture, NumSamples);
	return true;
#else
	return false;
#endif
}

bool FGearVRCustomPresent::AllocateRenderTargetTexture(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(SizeX != 0 && SizeY != 0);

	Flags |= TargetableTextureFlags;

	UE_LOG(LogHMD, Log, TEXT("Allocated a new swap texture set (size %d x %d)"), SizeX, SizeY);

	auto GLRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
	TextureSet = FOpenGLTexture2DSet::CreateTexture2DSet(
		GLRHI,
		SizeX, SizeY,
		1,
		EPixelFormat(Format),
		TexCreate_RenderTargetable | TexCreate_ShaderResource
		);

	OutTargetableTexture = TextureSet->GetTexture2D();
	OutShaderResourceTexture = TextureSet->GetTexture2D();

	check(IsInGameThread() && IsInRenderingThread()); // checking if rendering thread is suspended

	return true;
}

FGearVR* FViewExtension::GetGearVR() const
{ 
	return static_cast<FGearVR*>(Delegate); 
}

void FViewExtension::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View)
{
	check(IsInRenderingThread());

//	FViewExtension& RenderContext = *this; 
	const FGameFrame* CurrentFrame = GetRenderFrame();

	if (!bFrameBegun || !ShowFlags.Rendering || !CurrentFrame || !CurrentFrame->Settings->IsStereoEnabled())
	{
		return;
	}

	const FSettings* FrameSettings = CurrentFrame->GetSettings();

	const unsigned eyeIdx = (View.StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
	pPresentBridge->FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[eyeIdx].HeadPose = NewTracking.HeadPose;
	pPresentBridge->LoadingIconParms.Layers[VRAPI_FRAME_LAYER_TYPE_OVERLAY].Textures[eyeIdx].HeadPose = NewTracking.HeadPose;

	ovrPosef CurEyeRenderPose;

	// Take new EyeRenderPose is bUpdateOnRT.
	// if !bOrientationChanged && !bPositionChanged then we still need to use new eye pose (for timewarp)
	if (FrameSettings->Flags.bUpdateOnRT ||
		(!CurrentFrame->Flags.bOrientationChanged && !CurrentFrame->Flags.bPositionChanged))
	{
		CurHeadPose = NewTracking.HeadPose;
		CurEyeRenderPose = NewEyeRenderPose[eyeIdx];
	}
	else
	{
		CurEyeRenderPose = CurrentFrame->EyeRenderPose[eyeIdx];
		// use previous EyeRenderPose for proper timewarp when !bUpdateOnRt
		pPresentBridge->FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[eyeIdx].HeadPose = CurrentFrame->HeadPose;
	}
	//const auto RTTexId = *(GLuint*)View.Family->RenderTarget->GetRenderTargetTexture()->GetNativeResource();
	pPresentBridge->FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[eyeIdx].ColorTextureSwapChain = pPresentBridge->TextureSet->GetColorTextureSet();
	pPresentBridge->FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[eyeIdx].TextureSwapChainIndex = pPresentBridge->TextureSet->GetCurrentIndex();

	if (ShowFlags.Rendering && CurrentFrame->Settings->Flags.bUpdateOnRT)
	{
		FQuat	CurrentEyeOrientation;
		FVector	CurrentEyePosition;
		CurrentFrame->PoseToOrientationAndPosition(CurEyeRenderPose, CurrentEyeOrientation, CurrentEyePosition);

		FQuat ViewOrientation = View.ViewRotation.Quaternion();

		// recalculate delta control orientation; it should match the one we used in CalculateStereoViewOffset on a game thread.
		FVector GameEyePosition;
		FQuat GameEyeOrient;

		CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->EyeRenderPose[eyeIdx], GameEyeOrient, GameEyePosition);
		const FQuat DeltaControlOrientation =  ViewOrientation * GameEyeOrient.Inverse();
		// make sure we use the same viewrotation as we had on a game thread
		check(View.ViewRotation == CurrentFrame->CachedViewRotation[eyeIdx]);

		if (CurrentFrame->Flags.bOrientationChanged)
		{
			// Apply updated orientation to corresponding View at recalc matrices.
			// The updated position will be applied from inside of the UpdateViewMatrix() call.
			const FQuat DeltaOrient = View.BaseHmdOrientation.Inverse() * CurrentEyeOrientation;
			View.ViewRotation = FRotator(ViewOrientation * DeltaOrient);

			//UE_LOG(LogHMD, Log, TEXT("VIEWDLT: Yaw %.3f Pitch %.3f Roll %.3f"), DeltaOrient.Rotator().Yaw, DeltaOrient.Rotator().Pitch, DeltaOrient.Rotator().Roll);
		}

		if (!CurrentFrame->Flags.bPositionChanged)
		{
			// if no positional change applied then we still need to calculate proper stereo disparity.
			// use the current head pose for this calculation instead of the one that was saved on a game thread.
			FQuat HeadOrientation;
			CurrentFrame->PoseToOrientationAndPosition(CurHeadPose.Pose, HeadOrientation, View.BaseHmdLocation);
		}

		// The HMDPosition already has HMD orientation applied.
		// Apply rotational difference between HMD orientation and ViewRotation
		// to HMDPosition vector. 
		// PositionOffset should be already applied to View.ViewLocation on GT in PlayerCameraUpdate.
		const FVector DeltaPosition = CurrentEyePosition - View.BaseHmdLocation;
		const FVector vEyePosition = DeltaControlOrientation.RotateVector(DeltaPosition);
		View.ViewLocation += vEyePosition;

		//UE_LOG(LogHMD, Log, TEXT("VDLTPOS: %.3f %.3f %.3f"), vEyePosition.X, vEyePosition.Y, vEyePosition.Z);

		if (CurrentFrame->Flags.bOrientationChanged || CurrentFrame->Flags.bPositionChanged)
		{
			View.UpdateViewMatrix();
		}
	}
}

void FGearVR::EnterVRMode()
{
	check(pGearVRBridge);

	if (IsInRenderingThread())
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("+++++++ EnterVRMode ++++++, On RT! tid = %d"), gettid());
		pGearVRBridge->EnterVRMode_RenderThread();
	}
	else
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("+++++++ EnterVRMode ++++++, tid = %d"), gettid());
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(EnterVRMode,
		FGearVRCustomPresent*, pGearVRBridge, pGearVRBridge,
		{
			pGearVRBridge->EnterVRMode_RenderThread();
		});
		FlushRenderingCommands();
	}

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("------- EnterVRMode -------, tid = %d"), gettid());
}

void FGearVR::LeaveVRMode()
{
	if (IsInRenderingThread())
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("+++++++ LeaveVRMode ++++++, On RT! tid = %d"), gettid());
		pGearVRBridge->LeaveVRMode_RenderThread();
	}
	else
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("+++++++ LeaveVRMode ++++++, tid = %d"), gettid());
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(LeaveVRMode,
		FGearVRCustomPresent*, pGearVRBridge, pGearVRBridge,
		{
			pGearVRBridge->LeaveVRMode_RenderThread();
		});
		FlushRenderingCommands();
	}

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("------- LeaveVRMode -------, tid = %d"), gettid());
}

void FViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());

	//FViewExtension& RenderContext = *this;
	FGameFrame* CurrentFrame = static_cast<FGameFrame*>(RenderFrame.Get());

	auto pGearVRPlugin = GetGearVR();

	if (!pPresentBridge || bFrameBegun || !CurrentFrame || !CurrentFrame->Settings->IsStereoEnabled())
	{
		return;
	}
	else
	{
		if (!pGearVRPlugin->GetMobileSynced().IsValid())
		{
			return;
		}
	}

	FSettings* FrameSettings = CurrentFrame->GetSettings();
	ShowFlags = ViewFamily.EngineShowFlags;

	check(ViewFamily.RenderTarget->GetRenderTargetTexture());
	uint32 RenderTargetWidth = ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeX();
	uint32 RenderTargetHeight= ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeY();
	CurrentFrame->GetSettings()->SetEyeRenderViewport(RenderTargetWidth/2, RenderTargetHeight);
	pPresentBridge->BeginRendering(*this, ViewFamily.RenderTarget->GetRenderTargetTexture());

	bFrameBegun = true;

	FQuat OldOrientation;
	FVector OldPosition;
	CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->CurSensorState.HeadPose.Pose, OldOrientation, OldPosition);
	const FTransform OldRelativeTransform(OldOrientation, OldPosition);

	if (ShowFlags.Rendering)
	{
		check(pPresentBridge->GetRenderThreadId() == gettid());
		// get latest orientation/position and cache it
		if (!pGearVRPlugin->GetEyePoses(*CurrentFrame, NewEyeRenderPose, NewTracking))
		{
			UE_LOG(LogHMD, Error, TEXT("GetEyePoses from RT failed"));
			return;
		}
	}

	FQuat NewOrientation;
	FVector NewPosition;
	CurrentFrame->PoseToOrientationAndPosition(NewTracking.HeadPose.Pose, NewOrientation, NewPosition);
	const FTransform NewRelativeTransform(NewOrientation, NewPosition);

	Delegate->ApplyLateUpdate(ViewFamily.Scene, OldRelativeTransform, NewRelativeTransform);
}

void FGearVR::CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread());

	if (!Settings->IsStereoEnabled())
	{
		return;
	}

	// We must be sure the rendertargetsize is calculated already
	if (Flags.bNeedUpdateStereoRenderingParams)
	{
		UpdateStereoRenderingParams();
	}

	InOutSizeX = GetFrame()->GetSettings()->RenderTargetSize.X;
	InOutSizeY = GetFrame()->GetSettings()->RenderTargetSize.Y;
}

bool FGearVR::NeedReAllocateViewportRenderTarget(const FViewport& Viewport)
{
	check(IsInGameThread());

	if (IsStereoEnabled())
	{
		const uint32 InSizeX = Viewport.GetSizeXY().X;
		const uint32 InSizeY = Viewport.GetSizeXY().Y;
		const FIntPoint RenderTargetSize = Viewport.GetRenderTargetTextureSizeXY();

		uint32 NewSizeX = InSizeX, NewSizeY = InSizeY;
		CalculateRenderTargetSize(Viewport, NewSizeX, NewSizeY);
		if (NewSizeX != RenderTargetSize.X || NewSizeY != RenderTargetSize.Y)
		{
			return true;
		}
	}

	return false;
}

void FGearVR::ShutdownRendering()
{
	check(IsInRenderingThread());
	
	if (pGearVRBridge)
	{
		pGearVRBridge->Shutdown();
		pGearVRBridge = nullptr;
	}
}

void FGearVR::SetLoadingIconTexture(FTextureRHIRef InTexture)
{
	if (pGearVRBridge)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(EnterVRMode,
		FGearVRCustomPresent*, pGearVRBridge, pGearVRBridge,
		FTextureRHIRef, InTexture, InTexture,
		{
			pGearVRBridge->SetLoadingIconTexture_RenderThread(InTexture);
		});
	}
}

void FGearVR::SetLoadingIconMode(bool bActiveLoadingIcon)
{
	if (pGearVRBridge)
	{
		pGearVRBridge->SetLoadingIconMode(bActiveLoadingIcon);
	}
}

bool FGearVR::IsInLoadingIconMode() const
{
	if (pGearVRBridge)
	{
		return pGearVRBridge->IsInLoadingIconMode();
	}
	return false;
}

void FGearVR::RenderLoadingIcon_RenderThread()
{
	check(IsInRenderingThread());

	if (pGearVRBridge)
	{
		static uint32 FrameIndex = 0;
		pGearVRBridge->RenderLoadingIcon_RenderThread(FrameIndex++);
	}
}

//////////////////////////////////////////////////////////////////////////
FGearVRCustomPresent::FGearVRCustomPresent(jobject InActivityObject, int InMinimumVsyncs) :
	FRHICustomPresent(nullptr),
	bInitialized(false),
	bLoadingIconIsActive(false),
	bExtraLatencyMode(true),
	MinimumVsyncs(InMinimumVsyncs),
	LoadingIconTextureSet(nullptr),
	OvrMobile(nullptr),
	ActivityObject(InActivityObject)
{
	bHMTWasMounted = false;
	Init();
}

void FGearVRCustomPresent::Shutdown()
{
	UE_LOG(LogHMD, Log, TEXT("FGearVRCustomPresent::Shutdown() is called"));
	check(IsInRenderingThread());
	Reset(); 
	
	SetLoadingIconTexture_RenderThread(nullptr);

	FScopeLock lock(&OvrMobileLock);
	if (OvrMobile)
	{
		LeaveVRMode_RenderThread();
	}

	auto GLRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
	GLRHI->SetCustomPresent(nullptr);
}


void FGearVRCustomPresent::SetRenderContext(FHMDViewExtension* InRenderContext)
{
	if (InRenderContext)
	{
		RenderContext = StaticCastSharedRef<FViewExtension>(InRenderContext->AsShared());
	}
	else
	{
		RenderContext.Reset();
	}
}

void FGearVRCustomPresent::BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT)
{
	check(IsInRenderingThread());

	SetRenderContext(&InRenderContext);

	check(IsValidRef(RT));
	const uint32 RTSizeX = RT->GetSizeX();
	const uint32 RTSizeY = RT->GetSizeY();
	
	FGameFrame* CurrentFrame = GetRenderFrame();
	check(CurrentFrame);

	FrameParms.FrameIndex = CurrentFrame->FrameNumber;
	FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[VRAPI_FRAME_LAYER_EYE_LEFT].TexCoordsFromTanAngles = CurrentFrame->TanAngleMatrix;
	FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[VRAPI_FRAME_LAYER_EYE_RIGHT].TexCoordsFromTanAngles = CurrentFrame->TanAngleMatrix;

	check(VRAPI_FRAME_LAYER_EYE_LEFT == 0);
	check(VRAPI_FRAME_LAYER_EYE_RIGHT == 1);
	// split screen stereo
	for ( int i = 0 ; i < 2 ; i++ )
	{
		for ( int j = 0 ; j < 3 ; j++ )
		{
			FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[i].TexCoordsFromTanAngles.M[0][j] *= ((float)RTSizeY / (float)RTSizeX);
		}
	}
	FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[VRAPI_FRAME_LAYER_EYE_RIGHT].TexCoordsFromTanAngles.M[0][2] -= 1.0 - ((float)RTSizeY / (float)RTSizeX);

	static const ovrRectf LeftEyeRect  = { 0.0f, 0.0f, 0.5f, 1.0f };
	static const ovrRectf RightEyeRect = { 0.5f, 0.0f, 0.5f, 1.0f };
	FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[VRAPI_FRAME_LAYER_EYE_LEFT].TextureRect = LeftEyeRect;
	FrameParms.Layers[VRAPI_FRAME_LAYER_TYPE_WORLD].Textures[VRAPI_FRAME_LAYER_EYE_RIGHT].TextureRect= RightEyeRect;
}

void FGearVRCustomPresent::FinishRendering()
{
	check(IsInRenderingThread());

	if (RenderContext.IsValid() && RenderContext->bFrameBegun && TextureSet)
	{
		FScopeLock lock(&OvrMobileLock);
 		// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
		if (OvrMobile)
		{
			check(RenderThreadId == gettid());

			if (IsInLoadingIconMode())
			{
				FGameFrame* CurrentFrame = GetRenderFrame();
				LoadingIconParms.FrameIndex = CurrentFrame->FrameNumber;
				DoRenderLoadingIcon_RenderThread(RenderContext->GetFrameSetting()->CpuLevel, RenderContext->GetFrameSetting()->GpuLevel, RenderContext->GetRenderFrame()->GameThreadId);
			}
			else
			{
				FrameParms.PerformanceParms = DefaultPerfParms;
				FrameParms.PerformanceParms.CpuLevel = RenderContext->GetFrameSetting()->CpuLevel;
				FrameParms.PerformanceParms.GpuLevel = RenderContext->GetFrameSetting()->GpuLevel;
				FrameParms.PerformanceParms.MainThreadTid = RenderContext->GetRenderFrame()->GameThreadId;
				FrameParms.PerformanceParms.RenderThreadTid = gettid();
				FrameParms.Java = JavaRT;
				SystemActivities_Update_RenderThread();

				vrapi_SubmitFrame(OvrMobile, &FrameParms);

				TextureSet->SwitchToNextElement();
			}
		}
		else
		{
			UE_LOG(LogHMD, Warning, TEXT("Skipping frame: No active Ovr_Mobile object"));
		}
	}
	else
	{
		if (RenderContext.IsValid() && !RenderContext->bFrameBegun)
		{
			UE_LOG(LogHMD, Warning, TEXT("Skipping frame: FinishRendering called with no corresponding BeginRendering (was BackBuffer re-allocated?)"));
		}
		else if (!TextureSet)
		{
			UE_LOG(LogHMD, Warning, TEXT("Skipping frame: TextureSet is null"));
		}
		else
		{
			UE_LOG(LogHMD, Warning, TEXT("Skipping frame: No RenderContext set"));
		}
	}
	SetRenderContext(nullptr);
}

void FGearVRCustomPresent::Init()
{
	bInitialized = true;

	DefaultPerfParms = vrapi_DefaultPerformanceParms();

	JavaRT.Vm = nullptr;
	JavaRT.Env = nullptr;
	RenderThreadId = 0;

	auto GLRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
	GLRHI->SetCustomPresent(this);
}

void FGearVRCustomPresent::Reset()
{
	check(IsInRenderingThread());

	if (RenderContext.IsValid())
	{

		RenderContext->bFrameBegun = false;
		RenderContext.Reset();
	}
	bInitialized = false;
}

void FGearVRCustomPresent::OnBackBufferResize()
{
	// if we are in the middle of rendering: prevent from calling EndFrame
	if (RenderContext.IsValid())
	{
		RenderContext->bFrameBegun = false;
	}
}

void FGearVRCustomPresent::UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI)
{
	check(IsInGameThread());
	check(ViewportRHI);

	this->ViewportRHI = ViewportRHI;
	ViewportRHI->SetCustomPresent(this);
}



bool FGearVRCustomPresent::Present(int& SyncInterval)
{
	check(IsInRenderingThread());

	FinishRendering();

	return false; // indicates that we are presenting here, UE shouldn't do Present.
}

void FGearVRCustomPresent::EnterVRMode_RenderThread()
{
	check(IsInRenderingThread());

	FScopeLock lock(&OvrMobileLock);
	if (!OvrMobile)
	{
		ovrJava JavaVM;
		JavaVM.Vm = GJavaVM;
		JavaVM.ActivityObject = ActivityObject;
		GJavaVM->AttachCurrentThread(&JavaVM.Env, nullptr);

		LoadingIconParms = vrapi_DefaultFrameParms(&JavaVM, VRAPI_FRAME_INIT_LOADING_ICON, vrapi_GetTimeInSeconds(), nullptr);
		LoadingIconParms.MinimumVsyncs = MinimumVsyncs;

		FrameParms = vrapi_DefaultFrameParms(&JavaVM, VRAPI_FRAME_INIT_DEFAULT, vrapi_GetTimeInSeconds(), nullptr);
		FrameParms.MinimumVsyncs = MinimumVsyncs;
		FrameParms.ExtraLatencyMode = (bExtraLatencyMode) ? VRAPI_EXTRA_LATENCY_MODE_ON : VRAPI_EXTRA_LATENCY_MODE_OFF;

		ovrModeParms parms = vrapi_DefaultModeParms(&JavaVM);
		// Reset may cause weird issues
		// If power saving is on then perf may suffer
		parms.Flags &= ~(VRAPI_MODE_FLAG_ALLOW_POWER_SAVE | VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN);

		parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
		parms.Display = (size_t)AndroidEGL::GetInstance()->GetDisplay();
		parms.WindowSurface = (size_t)AndroidEGL::GetInstance()->GetNativeWindow();
		parms.ShareContext = (size_t)AndroidEGL::GetInstance()->GetRenderingContext()->eglContext;
		UE_LOG(LogHMD, Log, TEXT("EnterVRMode: Display 0x%llX, Window 0x%llX, ShareCtx %llX"), 
			(unsigned long long)parms.Display, (unsigned long long)parms.WindowSurface, (unsigned long long)parms.ShareContext);
		OvrMobile = vrapi_EnterVrMode(&parms);
	}
}

void FGearVRCustomPresent::LeaveVRMode_RenderThread()
{
	check(IsInRenderingThread());

	FScopeLock lock(&OvrMobileLock);
	if (OvrMobile)
	{
		check(PlatformOpenGLContextValid());
		vrapi_LeaveVrMode(OvrMobile);
		OvrMobile = NULL;
		check(PlatformOpenGLContextValid());
		RenderThreadId = 0;

		if (JavaRT.Env)
		{
			check(JavaRT.Vm);
			JavaRT.Vm->DetachCurrentThread();
			JavaRT.Vm = nullptr;
			JavaRT.Env = nullptr;
		}
	}
}

void FGearVRCustomPresent::OnAcquireThreadOwnership()
{
	UE_LOG(LogHMD, Log, TEXT("!!! Rendering thread is acquired! tid = %d"), gettid());

	JavaRT.Vm = GJavaVM;
	JavaRT.ActivityObject = ActivityObject;
	GJavaVM->AttachCurrentThread(&JavaRT.Env, nullptr);
	RenderThreadId = gettid();
}

void FGearVRCustomPresent::OnReleaseThreadOwnership()
{
	UE_LOG(LogHMD, Log, TEXT("!!! Rendering thread is released! tid = %d"), gettid());

	check(RenderThreadId == 0 || RenderThreadId == gettid());
	LeaveVRMode_RenderThread();

	if (JavaRT.Env)
	{
		check(JavaRT.Vm);
		JavaRT.Vm->DetachCurrentThread();
		JavaRT.Vm = nullptr;
		JavaRT.Env = nullptr;
	}
}

void FGearVRCustomPresent::SetLoadingIconMode(bool bLoadingIconActive)
{
	bLoadingIconIsActive = bLoadingIconActive;
}

bool FGearVRCustomPresent::IsInLoadingIconMode() const
{
	return bLoadingIconIsActive;
}

void FGearVRCustomPresent::SetLoadingIconTexture_RenderThread(FTextureRHIRef Texture)
{
	check(IsInRenderingThread());
	SrcLoadingIconTexture = Texture;

	if (LoadingIconTextureSet)
	{
		vrapi_DestroyTextureSwapChain(LoadingIconTextureSet);
		LoadingIconTextureSet = nullptr;
	}

	// Reset LoadingIconParms
	LoadingIconParms = vrapi_DefaultFrameParms(&JavaRT, VRAPI_FRAME_INIT_LOADING_ICON, vrapi_GetTimeInSeconds(), nullptr);
	LoadingIconParms.MinimumVsyncs = MinimumVsyncs;

	if (Texture && Texture->GetTexture2D())
	{
		const uint32 SizeX = Texture->GetTexture2D()->GetSizeX();
		const uint32 SizeY = Texture->GetTexture2D()->GetSizeY();

		const ovrTextureFormat VrApiFormat = VRAPI_TEXTURE_FORMAT_8888;

		LoadingIconTextureSet = vrapi_CreateTextureSwapChain(VRAPI_TEXTURE_TYPE_2D, VrApiFormat, SizeX, SizeY, 0, false);
		// set the icon
		GLuint LoadingIconTexID = *(GLuint*)SrcLoadingIconTexture->GetNativeResource();
		
		UE_LOG(LogHMD, Log, TEXT("LOADINGICON TEX ID %d"), LoadingIconTexID);
		vrapi_SetTextureSwapChainHandle(LoadingIconTextureSet, 0, LoadingIconTexID);
	}
}

void FGearVRCustomPresent::RenderLoadingIcon_RenderThread(uint32 FrameIndex)
{
	check(IsInRenderingThread());
	LoadingIconParms.FrameIndex = FrameIndex;
	DoRenderLoadingIcon_RenderThread(0, 0, 0);
}

void FGearVRCustomPresent::DoRenderLoadingIcon_RenderThread(int CpuLevel, int GpuLevel, pid_t GameTid)
{
	check(IsInRenderingThread());

	if (OvrMobile)
	{
		LoadingIconParms.PerformanceParms = DefaultPerfParms;
		if (CpuLevel)
		{
			LoadingIconParms.PerformanceParms.CpuLevel = CpuLevel;
		}
		if (GpuLevel)
		{
			LoadingIconParms.PerformanceParms.GpuLevel = GpuLevel;
		}
		if (GameTid)
		{
			LoadingIconParms.PerformanceParms.MainThreadTid = GameTid;
		}
		LoadingIconParms.PerformanceParms.RenderThreadTid = gettid();

		if (LoadingIconTextureSet)
		{
			for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
			{
				LoadingIconParms.Layers[VRAPI_FRAME_LAYER_TYPE_OVERLAY].Textures[eye].ColorTextureSwapChain = LoadingIconTextureSet;
			}
		}

		SystemActivities_Update_RenderThread();

		vrapi_SubmitFrame(OvrMobile, &LoadingIconParms);
	}
}

void FGearVRCustomPresent::PushBlackFinal(const FGameFrame& frame)
{
	check(IsInRenderingThread());

	if (OvrMobile)
	{
		UE_LOG(LogHMD, Log, TEXT("PushBlackFinal()"));
		ovrFrameParms frameParms = vrapi_DefaultFrameParms(&JavaRT, VRAPI_FRAME_INIT_BLACK_FINAL, vrapi_GetTimeInSeconds(), NULL );
		FrameParms.PerformanceParms = DefaultPerfParms;
		const FSettings* Settings = frame.GetSettings();
		FrameParms.PerformanceParms.CpuLevel = Settings->CpuLevel;
		FrameParms.PerformanceParms.GpuLevel = Settings->GpuLevel;
		FrameParms.PerformanceParms.MainThreadTid = frame.GameThreadId;
		FrameParms.PerformanceParms.RenderThreadTid = gettid();

		frameParms.FrameIndex = frame.FrameNumber;
		vrapi_SubmitFrame(OvrMobile, &frameParms );
	}
}

void FGearVRCustomPresent::SystemActivities_Update_RenderThread()
{
	check(IsInRenderingThread());

	if (!IsInitialized() || !OvrMobile)
	{
		return;
	}

	SystemActivitiesAppEventList_t	AppEvents;

	// process any SA events
	SystemActivities_Update(OvrMobile, &JavaRT, &AppEvents);

	bool isHMTMounted = (vrapi_GetSystemStatusInt(&JavaRT, VRAPI_SYS_STATUS_MOUNTED) != VRAPI_FALSE);
	if (isHMTMounted && isHMTMounted != bHMTWasMounted)
	{
		UE_LOG(LogHMD, Log, TEXT("Just mounted"));
		// We just mounted so push a reorient event to be handled in SystemActivities_Update.
		// This event will be handled just as if System Activities sent it to the application
		char reorientMessage[1024];
		SystemActivities_CreateSystemActivitiesCommand("", SYSTEM_ACTIVITY_EVENT_REORIENT, "", "",
			reorientMessage, sizeof(reorientMessage));
		SystemActivities_AppendAppEvent(&AppEvents, reorientMessage);
	}
	bHMTWasMounted = isHMTMounted;

	SystemActivities_PostUpdate(OvrMobile, &JavaRT, &AppEvents);
}

#endif //GEARVR_SUPPORTED_PLATFORMS

