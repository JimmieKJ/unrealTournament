// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HMDPrivatePCH.h"
#include "GearVR.h"
#include "RHIStaticStates.h"

#if GEARVR_SUPPORTED_PLATFORMS

#include "OpenGLDrvPrivate.h"
#include "OpenGLResources.h"
#include "VrApi_Helpers.h"
#include "VrApi_LocalPrefs.h"
#include "Android/AndroidJNI.h"

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
		//auto RenderFrame = static_cast<FGameFrame*>(GetRenderFrame());
		pGearVRBridge->DoEnterVRMode();
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(EnterVRMode,
		FGearVRCustomPresent*, pGearVRBridge, pGearVRBridge,
		{
			pGearVRBridge->DoEnterVRMode();
		});
		FlushRenderingCommands();
	}
}

void FGearVR::LeaveVRMode()
{
	if (IsInRenderingThread())
	{
		pGearVRBridge->DoLeaveVRMode();
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(LeaveVRMode,
		FGearVRCustomPresent*, pGearVRBridge, pGearVRBridge,
		{
			pGearVRBridge->DoLeaveVRMode();
		});
		FlushRenderingCommands();
	}
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

//////////////////////////////////////////////////////////////////////////
FGearVRCustomPresent::FGearVRCustomPresent(jobject InActivityObject, int InMinimumVsyncs) :
	FRHICustomPresent(nullptr),
	bInitialized(false),
	MinimumVsyncs(InMinimumVsyncs),
	OvrMobile(nullptr),
	ActivityObject(InActivityObject)
{
	Init();
}

void FGearVRCustomPresent::Shutdown()
{
	UE_LOG(LogHMD, Log, TEXT("FGearVRCustomPresent::Shutdown() is called"));
	check(IsInRenderingThread());
	Reset(); 

	FScopeLock lock(&OvrMobileLock);
	if (OvrMobile)
	{
		DoLeaveVRMode();
	}

	auto GLRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
	GLRHI->SetCustomPresent(nullptr);
}

void FGearVRCustomPresent::DicedBlit(uint32 SourceX, uint32 SourceY, uint32 DestX, uint32 DestY, uint32 Width, uint32 Height, uint32 NumXSteps, uint32 NumYSteps)
{
	check((NumXSteps > 0) && (NumYSteps > 0))
	uint32 StepX = Width / NumXSteps;
	uint32 StepY = Height / NumYSteps;

	uint32 MaxX = SourceX + Width;
	uint32 MaxY = SourceY + Height;
	
	for (; SourceY < MaxY; SourceY += StepY, DestY += StepY)
	{
		uint32 CurHeight = FMath::Min(StepY, MaxY - SourceY);

		for (uint32 CurSourceX = SourceX, CurDestX = DestX; CurSourceX < MaxX; CurSourceX += StepX, CurDestX += StepX)
		{
			uint32 CurWidth = FMath::Min(StepX, MaxX - CurSourceX);

			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, CurDestX, DestY, CurSourceX, SourceY, CurWidth, CurHeight);
			GL_CHECK_ERROR;
		}
	}
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
	FrameParms.WarpProgram = VRAPI_FRAME_PROGRAM_MIDDLE_CLAMP;

	const ovrRectf LeftEyeRect  = { 0.0f, 0.0f, 0.5f, 1.0f };
	const ovrRectf RightEyeRect = { 0.5f, 0.0f, 0.5f, 1.0f };
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
			
			FrameParms.PerformanceParms = DefaultPerfParms;
			FrameParms.PerformanceParms.CpuLevel = RenderContext->GetFrameSetting()->CpuLevel;
			FrameParms.PerformanceParms.GpuLevel = RenderContext->GetFrameSetting()->GpuLevel;
			FrameParms.PerformanceParms.MainThreadTid = RenderContext->GetRenderFrame()->GameThreadId;
			FrameParms.PerformanceParms.RenderThreadTid = gettid();
			vrapi_SubmitFrame(OvrMobile, &FrameParms);

			TextureSet->SwitchToNextElement();
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

void FGearVRCustomPresent::DoEnterVRMode()
{
	check(IsInRenderingThread());
	check(!IsInGameThread())

	FScopeLock lock(&OvrMobileLock);
	if (!OvrMobile)
	{
		JavaRT.Vm = GJavaVM;
		JavaRT.ActivityObject = ActivityObject;
		GJavaVM->AttachCurrentThread(&JavaRT.Env, nullptr);

		FrameParms = vrapi_DefaultFrameParms(&JavaRT, VRAPI_FRAME_INIT_DEFAULT, 0);
		FrameParms.MinimumVsyncs = MinimumVsyncs;
		FrameParms.ExtraLatencyMode = EXTRA_LATENCY_MODE_ALWAYS;

		RenderThreadId = gettid();
		ovrModeParms parms = vrapi_DefaultModeParms(&JavaRT);
		parms.ResetWindowFullscreen = false; // Reset may cause weird issues
		parms.AllowPowerSave = false;		 // If power saving is on then perf may suffer

		UE_LOG(LogHMD, Log, TEXT("        eglGetCurrentSurface( EGL_DRAW ) = %p, fr = %d"), eglGetCurrentSurface(EGL_DRAW), GFrameNumber);

		OvrMobile = vrapi_EnterVrMode(&parms);

		UE_LOG(LogHMD, Log, TEXT("        vrapi_EnterVrMode()"));
		UE_LOG(LogHMD, Log, TEXT("        eglGetCurrentSurface( EGL_DRAW ) = %p"), eglGetCurrentSurface(EGL_DRAW));
	}
}

void FGearVRCustomPresent::DoLeaveVRMode()
{
	check(IsInRenderingThread());

	FScopeLock lock(&OvrMobileLock);
	if (OvrMobile)
	{
		UE_LOG(LogHMD, Log, TEXT("        eglGetCurrentSurface( EGL_DRAW ) = %p"), eglGetCurrentSurface(EGL_DRAW));

		check(PlatformOpenGLContextValid());
		vrapi_LeaveVrMode(OvrMobile);
		OvrMobile = NULL;
		check(PlatformOpenGLContextValid());

		UE_LOG(LogHMD, Log, TEXT("        vrapi_LeaveVrMode()"));
		UE_LOG(LogHMD, Log, TEXT("        eglGetCurrentSurface( EGL_DRAW ) = %p"), eglGetCurrentSurface(EGL_DRAW));

		RenderThreadId = 0;
	}
}

void FGearVRCustomPresent::OnAcquireThreadOwnership()
{
	UE_LOG(LogHMD, Log, TEXT("!!! Rendering thread is acquired! tid = 0x%X"), gettid());

	JavaRT.Vm = GJavaVM;
	JavaRT.ActivityObject = ActivityObject;
	GJavaVM->AttachCurrentThread(&JavaRT.Env, nullptr);
}

void FGearVRCustomPresent::OnReleaseThreadOwnership()
{
	UE_LOG(LogHMD, Log, TEXT("!!! Rendering thread is released! tid = 0x%X"), gettid());

	check(RenderThreadId == 0 || RenderThreadId == gettid());
	DoLeaveVRMode();

	if (JavaRT.Env)
	{
		JavaRT.Vm->DetachCurrentThread();
		JavaRT.Vm = nullptr;
		JavaRT.Env = nullptr;
	}
}

#endif //GEARVR_SUPPORTED_PLATFORMS

