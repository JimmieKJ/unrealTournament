// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GearVRPrivatePCH.h"
#include "GearVR.h"
#include "RHIStaticStates.h"

#if GEARVR_SUPPORTED_PLATFORMS

#include "OpenGLDrvPrivate.h"
#include "OpenGLResources.h"
#include "ScreenRendering.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidEGL.h"

#define OCULUS_STRESS_TESTS_ENABLED	0

#if OCULUS_STRESS_TESTS_ENABLED
#include "OculusStressTests.h"
#endif

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

static uint32 GetNumMipLevels(uint32 w, uint32 h)
{
	uint32 n = 1;
	while (w > 1 || h > 1)
	{
		w >>= 1;
		h >>= 1;
		n++;
	}
	return n;
}


void FOpenGLTexture2DSet::SwitchToNextElement()
{
	if (TextureCount != 0)
	{
		CurrentIndex = (CurrentIndex + 1) % TextureCount;
	}
	else
	{
		CurrentIndex = 0;
	}
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
	uint32 InNumMips,
	EPixelFormat InFormat,
	uint32 InFlags,
	bool bBuffered
	)
{
	GLenum Target = (InNumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
	GLenum Attachment = GL_NONE;// GL_COLOR_ATTACHMENT0;
	bool bAllocatedStorage = false;
	uint8* TextureRange = nullptr;

	FOpenGLTexture2DSet* NewTextureSet = new FOpenGLTexture2DSet(
		InGLRHI, 0, Target, Attachment, SizeX, SizeY, 0, InNumMips, InNumSamples, 1, InFormat, false, bAllocatedStorage, InFlags, TextureRange);

	const int32 NumLevels = (InNumMips == 0) ? VRAPI_TEXTURE_SWAPCHAIN_FULL_MIP_CHAIN : int(InNumMips);
	NewTextureSet->ColorTextureSet = vrapi_CreateTextureSwapChain(VRAPI_TEXTURE_TYPE_2D, VRAPI_TEXTURE_FORMAT_8888, SizeX, SizeY, NumLevels, bBuffered);
	if (!NewTextureSet->ColorTextureSet)
	{
		// hmmm... can't allocate a texture set for some reasons.
		UE_LOG(LogHMD, Log, TEXT("Can't allocate textureSet swap chain."));
		delete NewTextureSet;
		return nullptr;
	}
	UE_LOG(LogHMD, Log, TEXT("Allocated textureSet %p (%d x %d)"), NewTextureSet->ColorTextureSet, SizeX, SizeY);
	NewTextureSet->TextureCount = vrapi_GetTextureSwapChainLength(NewTextureSet->ColorTextureSet);

	NewTextureSet->InitWithCurrentElement();
	return NewTextureSet;
}

FRenderLayer::FRenderLayer(FHMDLayerDesc& InDesc) :
	FHMDRenderLayer(InDesc)
{
	FMemory::Memset(Layer, 0);
	ovrJava JavaVM;

	Layer = vrapi_DefaultFrameParms(&JavaVM, VRAPI_FRAME_INIT_DEFAULT, 0, nullptr).Layers[VRAPI_FRAME_LAYER_TYPE_OVERLAY];
}

FRenderLayer::~FRenderLayer()
{
}

TSharedPtr<FHMDRenderLayer> FRenderLayer::Clone() const
{
	TSharedPtr<FHMDRenderLayer> NewLayer = MakeShareable(new FRenderLayer(*this));
	return NewLayer;
}


FLayerManager::FLayerManager(FCustomPresent* inPresent) :
	pPresentBridge(inPresent)
	, EyeLayerId(0)
	, bInitialized(false)
{
}

FLayerManager::~FLayerManager()
{
}

void FLayerManager::Startup()
{
	if (!bInitialized)
	{
		FHMDLayerManager::Startup();

		auto EyeLayer = AddLayer(FHMDLayerDesc::Eye, INT_MIN, Layer_UnknownOrigin, EyeLayerId);
		check(EyeLayer.IsValid());
		bInitialized = true;
	}
}

void FLayerManager::Shutdown()
{
	if (bInitialized)
	{
		ReleaseTextureSets();

		FHMDLayerManager::Shutdown();
		bInitialized = false;
	}
}

void FLayerManager::ReleaseTextureSets()
{
	if (IsInRenderingThread())
	{
		FScopeLock Lock(&LayersLock);
		ReleaseTextureSets_RenderThread_NoLock();
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(FLayerManager_Shutdown,
			FLayerManager*, LayerMgr, this,
			{
				LayerMgr->ReleaseTextureSets_RenderThread_NoLock();
			});
		FlushRenderingCommands();
	}
}

void FLayerManager::ReleaseTextureSets_RenderThread_NoLock() 
{
	check(IsInRenderingThread());

	FHMDLayerManager::ReleaseTextureSets_RenderThread_NoLock();

	const uint32 NumLayers = LayersToRender.Num();
	for (uint32 i = 0; i < NumLayers; ++i)
	{
		auto RenderLayer = static_cast<FRenderLayer*>(LayersToRender[i].Get());
		RenderLayer->ReleaseResources();
	}
}

void FLayerManager::PreSubmitUpdate_RenderThread(FRHICommandListImmediate& RHICmdList, const FHMDGameFrame* InCurrentFrame, bool ShowFlagsRendering)
{
	const bool bLayersWereChanged = bLayersChanged;

	const FGameFrame* CurrentFrame = static_cast<const FGameFrame*>(InCurrentFrame);

	// Call base method first, it will make sure the LayersToRender is ready
	FHMDLayerManager::PreSubmitUpdate_RenderThread(RHICmdList, CurrentFrame, ShowFlagsRendering);

	const float WorldToMetersScale = CurrentFrame->Settings->WorldToMetersScale;

	const FSettings* FrameSettings = CurrentFrame->GetSettings();

	for (uint32 l = 0; l < LayersToRender.Num() ; ++l)
	{
		FRenderLayer* RenderLayer = static_cast<FRenderLayer*>(LayersToRender[l].Get());
		if (!RenderLayer || !RenderLayer->IsFullySetup())
		{
			continue;
		}
		const FHMDLayerDesc& LayerDesc = RenderLayer->GetLayerDesc();
		FTextureRHIRef Texture = LayerDesc.GetTexture();
		switch (LayerDesc.GetType())
		{
		case FHMDLayerDesc::Eye:
		{
			RenderLayer->Layer.Textures[0].HeadPose = CurrentFrame->CurSensorState.HeadPose;
			RenderLayer->Layer.Textures[1].HeadPose = CurrentFrame->CurSensorState.HeadPose;

			const uint32 RTSizeX = CurrentFrame->ViewportSize.X;
			const uint32 RTSizeY = CurrentFrame->ViewportSize.Y;

			RenderLayer->Layer.Textures[VRAPI_FRAME_LAYER_EYE_LEFT].TexCoordsFromTanAngles = CurrentFrame->TanAngleMatrix;
			RenderLayer->Layer.Textures[VRAPI_FRAME_LAYER_EYE_RIGHT].TexCoordsFromTanAngles = CurrentFrame->TanAngleMatrix;

			check(VRAPI_FRAME_LAYER_EYE_LEFT == 0);
			check(VRAPI_FRAME_LAYER_EYE_RIGHT == 1);
			// split screen stereo
			for ( int i = 0 ; i < 2 ; i++ )
			{
				for ( int j = 0 ; j < 3 ; j++ )
				{
					RenderLayer->Layer.Textures[i].TexCoordsFromTanAngles.M[0][j] *= ((float)RTSizeY / (float)RTSizeX);
				}
			}
			RenderLayer->Layer.Textures[VRAPI_FRAME_LAYER_EYE_RIGHT].TexCoordsFromTanAngles.M[0][2] -= 1.0 - ((float)RTSizeY / (float)RTSizeX);

			static const ovrRectf LeftEyeRect  = { 0.0f, 0.0f, 0.5f, 1.0f };
			static const ovrRectf RightEyeRect = { 0.5f, 0.0f, 0.5f, 1.0f };
			RenderLayer->Layer.Textures[VRAPI_FRAME_LAYER_EYE_LEFT].TextureRect = LeftEyeRect;
			RenderLayer->Layer.Textures[VRAPI_FRAME_LAYER_EYE_RIGHT].TextureRect= RightEyeRect;

			RenderLayer->Layer.Textures[0].ColorTextureSwapChain = RenderLayer->Layer.Textures[1].ColorTextureSwapChain = RenderLayer->GetSwapTextureSet();
			RenderLayer->Layer.Textures[0].TextureSwapChainIndex = RenderLayer->Layer.Textures[1].TextureSwapChainIndex = RenderLayer->GetSwapTextureIndex();
			RenderLayer->CommitTextureSet(RHICmdList);
			RenderLayer->ResetChangedFlags();
			break;
		}
		case FHMDLayerDesc::Quad:
			if (Texture)
			{
				bool JustAllocated = false;
				bool ReloadTex = LayerDesc.IsTextureChanged() || (LayerDesc.GetFlags() & IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE);

				uint32 SizeX = Texture->GetTexture2D()->GetSizeX() + 2;
				uint32 SizeY = Texture->GetTexture2D()->GetSizeY() + 2;

				const ovrTextureFormat VrApiFormat = VRAPI_TEXTURE_FORMAT_8888;

				if (RenderLayer->TextureSet.IsValid() && ReloadTex && (
					RenderLayer->TextureSet->GetSourceSizeX() != SizeX ||
					RenderLayer->TextureSet->GetSourceSizeY() != SizeY ||
					RenderLayer->TextureSet->GetSourceFormat() != VrApiFormat ||
					RenderLayer->TextureSet->GetSourceNumMips() != 1))
				{
					UE_LOG(LogHMD, Log, TEXT("Releasing resources"));
					RenderLayer->TextureSet->ReleaseResources();
					RenderLayer->TextureSet.Reset();
				}

				if (!RenderLayer->TextureSet.IsValid())
				{
					RenderLayer->TextureSet = pPresentBridge->CreateTextureSet(SizeX, SizeY, VrApiFormat, 1, false);
					if (!RenderLayer->TextureSet.IsValid())
					{
						UE_LOG(LogHMD, Log, TEXT("ERROR : Couldn't instanciate textureset"));
					}
					ReloadTex = true;
					JustAllocated = true;
				}

				if (ReloadTex && RenderLayer->TextureSet.IsValid())
				{
					pPresentBridge->CopyTexture_RenderThread(RHICmdList, RenderLayer->TextureSet->GetRHITexture2D(), Texture, SizeX, SizeY, FIntRect(), FIntRect(), true);
				}

				//transform calculation
				OVR::Posef pose;
				pose.Rotation = ToOVRQuat<OVR::Quatf>(LayerDesc.GetTransform().GetRotation());
				pose.Translation = ToOVRVector_U2M<OVR::Vector3f>(LayerDesc.GetTransform().GetTranslation(), WorldToMetersScale);

				OVR::Vector3f scale(LayerDesc.GetQuadSize().X * LayerDesc.GetTransform().GetScale3D().Y / WorldToMetersScale, LayerDesc.GetQuadSize().Y * LayerDesc.GetTransform().GetScale3D().Z / WorldToMetersScale, 1.0f);
				// apply the scale from transform. We use Y for width and Z for height to match the UE coord space
				OVR::Matrix4f scaling = OVR::Matrix4f::Scaling(0.5f * (OVR::Vector3f&)scale);

				OVR::Posef PlayerTorso(ToOVRQuat<OVR::Quatf>(FrameSettings->BaseOrientation.Inverse() * CurrentFrame->PlayerOrientation),
					ToOVRVector_U2M<OVR::Vector3f>(CurrentFrame->PlayerLocation, WorldToMetersScale));

				if (LayerDesc.IsTorsoLocked())
				{
					// for torso locked consider torso as identity
					PlayerTorso = Posef(Quatf(0, 0, 0, 1), Vector3f(0, 0, 0));
				}

				for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
				{
					RenderLayer->Layer.Textures[eye].ColorTextureSwapChain = RenderLayer->GetSwapTextureSet();
					RenderLayer->Layer.Textures[eye].TextureSwapChainIndex = RenderLayer->GetSwapTextureIndex();
					RenderLayer->Layer.Textures[eye].HeadPose = CurrentFrame->HeadPose;

					ovrPosef eyeToIC = CurrentFrame->EyeRenderPose[eye];
					OVR::Posef centerToEye = (PlayerTorso * Posef(eyeToIC)).Inverted();

					//world locked!
					if (LayerDesc.IsWorldLocked() || LayerDesc.IsTorsoLocked())
					{
						OVR::Matrix4f m2e(centerToEye * pose);
						m2e *= scaling;

						const ovrMatrix4f mv = (ovrMatrix4f&)m2e;
						RenderLayer->Layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromUnitSquare(&mv);
					}
					else
					{
						ovrPosef centerEyeToIC = CurrentFrame->HeadPose.Pose;
						OVR::Posef centerTocenterEye = PlayerTorso * Posef(centerEyeToIC);

						OVR::Matrix4f m2e(centerToEye * centerTocenterEye* pose);
						m2e *= scaling;

						const ovrMatrix4f mv = (ovrMatrix4f&)m2e;
						RenderLayer->Layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromUnitSquare(&mv);
					}

				}
				RenderLayer->Layer.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
				RenderLayer->Layer.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;

				RenderLayer->Layer.Flags = 0;
				RenderLayer->Layer.Flags |= (LayerDesc.IsHeadLocked()) ? VRAPI_FRAME_LAYER_FLAG_FIXED_TO_VIEW : 0;

				// A HACK: for some reasons if the same texture is used for two layers at the same frame
				// then the second one could be not copied when the textureSet is just allocated. Therefore,
				// giving another chance to copy the content of the texture.
				if (!JustAllocated)
				{
					RenderLayer->ResetChangedFlags();
				}
			}
			break;
		}
	}
}

TSharedPtr<FHMDRenderLayer> FLayerManager::CreateRenderLayer_RenderThread(FHMDLayerDesc& InDesc)
{
	TSharedPtr<FHMDRenderLayer> NewLayer = MakeShareable(new FRenderLayer(InDesc));
	return NewLayer;
}

void FLayerManager::SubmitFrame_RenderThread(ovrMobile* mobilePtr, ovrFrameParms* currentParams)
{
	for (currentParams->LayerCount = 0;currentParams->LayerCount < VRAPI_FRAME_LAYER_TYPE_MAX && currentParams->LayerCount < LayersToRender.Num(); ++currentParams->LayerCount) 
	{
		FRenderLayer* RenderLayer = static_cast<FRenderLayer*>(LayersToRender[currentParams->LayerCount].Get());
		currentParams->Layers[currentParams->LayerCount] = RenderLayer->Layer;
	}

	//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FR IDX = %d! tid = %d"), int(currentParams->FrameIndex), int(gettid()));
	if (currentParams->LayerCount > 0)
	{
		vrapi_SubmitFrame(mobilePtr, currentParams);

		for (uint32 i = 0; i < LayersToRender.Num(); ++i)
		{
			auto RenderLayer = static_cast<FRenderLayer*>(LayersToRender[i].Get());
			if (RenderLayer->GetLayerDesc().GetType() == FHMDLayerDesc::Eye && RenderLayer->TextureSet.IsValid())
			{
				RenderLayer->TextureSet->SwitchToNextElement();
			}
		}
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("Skipping frame: no layers with valid texture sets"));
	}
}

void FGearVR::RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture) const
{
	check(IsInRenderingThread());

	check(pGearVRBridge);

	pGearVRBridge->UpdateLayers(RHICmdList);

#if OCULUS_STRESS_TESTS_ENABLED
	if (StressTester)
	{
		//StressTester->TickGPU_RenderThread(RHICmdList, BackBuffer, SrcTexture);
		StressTester->TickGPU_RenderThread(RHICmdList, SrcTexture, BackBuffer);
	}
#endif
}

bool FGearVR::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(Index == 0);
#if !OVR_DEBUG_DRAW
	UE_LOG(LogHMD, Log, TEXT("Allocating Render Target textures"));
	// ignore NumMips for RT, use 1 
	pGearVRBridge->AllocateRenderTargetTexture(SizeX, SizeY, Format, 1, Flags, TargetableTextureFlags, OutTargetableTexture, OutShaderResourceTexture, NumSamples);
	return true;
#else
	return false;
#endif
}

bool FCustomPresent::AllocateRenderTargetTexture(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(SizeX != 0 && SizeY != 0);

	Flags |= TargetableTextureFlags;

	UE_LOG(LogHMD, Log, TEXT("Allocating a new swap texture set (size %d x %d)"), SizeX, SizeY);

	const FHMDLayerDesc* pEyeLayerDesc = LayerMgr->GetEyeLayerDesc();
	check(pEyeLayerDesc);
	auto TextureSet = pEyeLayerDesc->GetTextureSet();

	if (TextureSet.IsValid())
	{
		TextureSet->ReleaseResources();
	}

	FTexture2DSetProxyPtr ColorTextureSet = CreateTextureSet(SizeX, SizeY, EPixelFormat(Format), NumMips, true);
	if (ColorTextureSet.IsValid())
	{
		OutTargetableTexture = ColorTextureSet->GetRHITexture2D();
		OutShaderResourceTexture = ColorTextureSet->GetRHITexture2D();

		// update the eye layer textureset. at the moment only one eye layer is supported
		FHMDLayerDesc EyeLayerDesc = *pEyeLayerDesc;
		EyeLayerDesc.SetTextureSet(ColorTextureSet);
		LayerMgr->UpdateLayer(EyeLayerDesc);

		check(IsInGameThread() && IsInRenderingThread()); // checking if rendering thread is suspended

		UE_LOG(LogHMD, Log, TEXT("New swap texture %p (%d x %d) has been allocated"), ColorTextureSet->GetTextureSet()->GetColorTextureSet(), SizeX, SizeY);

		return true;
	}

	return false;
}

FTexture2DSetProxyPtr FCustomPresent::CreateTextureSet(uint32 InSizeX, uint32 InSizeY, uint8 InFormat, uint32 InNumMips, bool bBuffered)
{
	check(InSizeX != 0 && InSizeY != 0);
	auto GLRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
	const uint32 NumMips = 1; // (InNumMips == 0) ? GetNumMipLevels(InSizeX, InSizeY) : InNumMips; // MipMaps are not supported yet
	FOpenGLTexture2DSetRef texref = FOpenGLTexture2DSet::CreateTexture2DSet(
		GLRHI,
		InSizeX, InSizeY,
		1,
		NumMips,
		EPixelFormat(InFormat),
		TexCreate_RenderTargetable | TexCreate_ShaderResource,
		bBuffered);

	if (texref.IsValid())
	{
		return MakeShareable(new FTexture2DSetProxy(texref, InSizeX, InSizeY, EPixelFormat(InFormat), NumMips));
	}
	return nullptr;
}

FGearVR* FViewExtension::GetGearVR() const
{
	return static_cast<FGearVR*>(Delegate);
}

void FViewExtension::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View)
{
	check(IsInRenderingThread());
	FGameFrame* CurrentFrame = GetRenderFrame();

	if (!bFrameBegun || !ShowFlags.Rendering || !CurrentFrame || !CurrentFrame->Settings->IsStereoEnabled() || (pPresentBridge && pPresentBridge->IsSubmitFrameLocked()))
	{
		return;
	}

	const int eyeIdx = (View.StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
	if (ShowFlags.Rendering && CurrentFrame->Settings->Flags.bUpdateOnRT)
	{
		FQuat	CurrentEyeOrientation;
		FVector	CurrentEyePosition;
		CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->CurEyeRenderPose[eyeIdx], CurrentEyeOrientation, CurrentEyePosition);

		const FQuat ViewOrientation = View.ViewRotation.Quaternion();

		// recalculate delta control orientation; it should match the one we used in CalculateStereoViewOffset on a game thread.
		FVector GameEyePosition;
		FQuat GameEyeOrient;

		CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->EyeRenderPose[eyeIdx], GameEyeOrient, GameEyePosition);
		const FQuat DeltaControlOrientation = ViewOrientation * GameEyeOrient.Inverse();
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
			CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->CurSensorState.HeadPose.Pose, HeadOrientation, View.BaseHmdLocation);
		}

		// The HMDPosition already has HMD orientation applied.
		// Apply rotational difference between HMD orientation and ViewRotation
		// to HMDPosition vector. 
		const FVector DeltaPosition = CurrentEyePosition - View.BaseHmdLocation;
		const FVector vEyePosition = DeltaControlOrientation.RotateVector(DeltaPosition) + CurrentFrame->Settings->PositionOffset;
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
		FCustomPresent*, pGearVRBridge, pGearVRBridge,
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
		FCustomPresent*, pGearVRBridge, pGearVRBridge,
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
	uint32 RenderTargetHeight = ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeY();
	CurrentFrame->GetSettings()->SetEyeRenderViewport(RenderTargetWidth / 2, RenderTargetHeight);
	pPresentBridge->BeginRendering(*this, ViewFamily.RenderTarget->GetRenderTargetTexture());

	bFrameBegun = true;

	FQuat OldOrientation;
	FVector OldPosition;
	CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->CurSensorState.HeadPose.Pose, OldOrientation, OldPosition);
	const FTransform OldRelativeTransform(OldOrientation, OldPosition);

	if (ShowFlags.Rendering)
	{
		checkf(pPresentBridge->GetRenderThreadId() == gettid(), TEXT("pPresentBridge->GetRenderThreadId() = %d, gettid() = %d"), pPresentBridge->GetRenderThreadId(), gettid());
		// get latest orientation/position and cache it
		if (!pGearVRPlugin->GetEyePoses(*CurrentFrame, CurrentFrame->CurEyeRenderPose, CurrentFrame->CurSensorState))
		{
			UE_LOG(LogHMD, Error, TEXT("GetEyePoses from RT failed"));
			return;
		}
	}

	FQuat NewOrientation;
	FVector NewPosition;
	CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->CurSensorState.HeadPose.Pose, NewOrientation, NewPosition);
	const FTransform NewRelativeTransform(NewOrientation, NewPosition);

	Delegate->ApplyLateUpdate(ViewFamily.Scene, OldRelativeTransform, NewRelativeTransform);

	if (ViewFamily.Views[0])
	{
		const FQuat ViewOrientation = ViewFamily.Views[0]->ViewRotation.Quaternion();
		CurrentFrame->PlayerOrientation = ViewOrientation * CurrentFrame->LastHmdOrientation.Inverse();
		//UE_LOG(LogHMD, Log, TEXT("PLAYER: Pos %.3f %.3f %.3f"), CurrentFrame->PlayerLocation.X, CurrentFrame->PlayerLocation.Y, CurrentFrame->PlayerLocation.Z);
		//UE_LOG(LogHMD, Log, TEXT("VIEW  : Yaw %.3f Pitch %.3f Roll %.3f"), FRotator(ViewOrientation).Yaw, FRotator(ViewOrientation).Pitch, FRotator(ViewOrientation).Roll);
		//UE_LOG(LogHMD, Log, TEXT("HEAD  : Yaw %.3f Pitch %.3f Roll %.3f"), FRotator(CurrentFrame->LastHmdOrientation).Yaw, FRotator(CurrentFrame->LastHmdOrientation).Pitch, FRotator(CurrentFrame->LastHmdOrientation).Roll);
		//UE_LOG(LogHMD, Log, TEXT("PLAYER: Yaw %.3f Pitch %.3f Roll %.3f"), FRotator(CurrentFrame->PlayerOrientation).Yaw, FRotator(CurrentFrame->PlayerOrientation).Pitch, FRotator(CurrentFrame->PlayerOrientation).Roll);
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

void FGearVR::SetLoadingIconTexture(FTextureRHIRef InTexture)
{
	if (pGearVRBridge)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(EnterVRMode,
		FCustomPresent*, pGearVRBridge, pGearVRBridge,
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
FCustomPresent::FCustomPresent(jobject InActivityObject, int InMinimumVsyncs) :
	FRHICustomPresent(nullptr),
	bInitialized(false),
	bLoadingIconIsActive(false),
	bExtraLatencyMode(true),
	MinimumVsyncs(InMinimumVsyncs),
	LoadingIconTextureSet(nullptr),
	LayerMgr(MakeShareable(new FLayerManager(this))),
	OvrMobile(nullptr),
	ActivityObject(InActivityObject)
{
	bHMTWasMounted = false;
	Init();

	static const FName RendererModuleName("Renderer");
	RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	LayerMgr->Startup();
}

void FCustomPresent::Shutdown()
{
	UE_LOG(LogHMD, Log, TEXT("FCustomPresent::Shutdown() is called"));
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
	LayerMgr->Shutdown();
}


void FCustomPresent::SetRenderContext(FHMDViewExtension* InRenderContext)
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

void FCustomPresent::BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT)
{
	check(IsInRenderingThread());

	SetRenderContext(&InRenderContext);

	check(IsValidRef(RT));

	FGameFrame* CurrentFrame = GetRenderFrame();
	check(CurrentFrame);

	CurrentFrame->ViewportSize = FIntPoint(RT->GetSizeX(), RT->GetSizeY());
}

void FCustomPresent::FinishRendering()
{
 	check(IsInRenderingThread());

	if (!IsSubmitFrameLocked())
	{
		if (RenderContext.IsValid() && RenderContext->bFrameBegun)
		{
			FScopeLock lock(&OvrMobileLock);
			// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
			if (OvrMobile)
			{
				check(RenderThreadId == gettid());

				FGameFrame* CurrentFrame = GetRenderFrame();

				if (IsInLoadingIconMode())
				{
					LoadingIconParms.FrameIndex = CurrentFrame->FrameNumber;
					DoRenderLoadingIcon_RenderThread(2, 0, RenderContext->GetRenderFrame()->GameThreadId);
				}
				else
				{
					ovrFrameParms frameParms = DefaultFrameParms;
					frameParms.FrameIndex = CurrentFrame->FrameNumber;
					frameParms.PerformanceParms = DefaultPerfParms;
					frameParms.PerformanceParms.CpuLevel = RenderContext->GetFrameSetting()->CpuLevel;
					frameParms.PerformanceParms.GpuLevel = RenderContext->GetFrameSetting()->GpuLevel;
					frameParms.PerformanceParms.MainThreadTid = RenderContext->GetRenderFrame()->GameThreadId;
					frameParms.PerformanceParms.RenderThreadTid = gettid();
					frameParms.Java = JavaRT;
					SystemActivities_Update_RenderThread();

					LayerMgr->SubmitFrame_RenderThread(OvrMobile, &frameParms);
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
			else
			{
				UE_LOG(LogHMD, Warning, TEXT("Skipping frame: No RenderContext set"));
			}
		}
	}
	SetRenderContext(nullptr);
}

void FCustomPresent::Init()
{
	bInitialized = true;

	DefaultPerfParms = vrapi_DefaultPerformanceParms();

	JavaRT.Vm = nullptr;
	JavaRT.Env = nullptr;
	RenderThreadId = 0;

	auto GLRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
	GLRHI->SetCustomPresent(this);
}

void FCustomPresent::Reset()
{
	check(IsInRenderingThread());

	if (RenderContext.IsValid())
	{

		RenderContext->bFrameBegun = false;
		RenderContext.Reset();
	}
	bInitialized = false;
}

void FCustomPresent::OnBackBufferResize()
{
	// if we are in the middle of rendering: prevent from calling EndFrame
	if (RenderContext.IsValid())
	{
		RenderContext->bFrameBegun = false;
	}
}

void FCustomPresent::UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI)
{
	check(IsInGameThread());

	this->ViewportRHI = ViewportRHI;

	if (ViewportRHI)
	{
		ViewportRHI->SetCustomPresent(this);
	}
}

void FCustomPresent::UpdateLayers(FRHICommandListImmediate& RHICmdList) 
{
	check(IsInRenderingThread());

	if (RenderContext.IsValid() && !IsSubmitFrameLocked())
	{
		if (RenderContext->ShowFlags.Rendering)
		{
			check(GetRenderFrame());

			LayerMgr->PreSubmitUpdate_RenderThread(RHICmdList, GetRenderFrame(), RenderContext->ShowFlags.Rendering);
		}
	}
}

bool FCustomPresent::Present(int& SyncInterval)
{
	check(IsInRenderingThread());

	FinishRendering();

	return false; // indicates that we are presenting here, UE shouldn't do Present.
}

void FCustomPresent::EnterVRMode_RenderThread()
{
	check(IsInRenderingThread());

	FScopeLock lock(&OvrMobileLock);
	if (!OvrMobile)
	{
		ovrJava JavaVM;
		JavaVM.Vm = GJavaVM;
		JavaVM.ActivityObject = ActivityObject;
		GJavaVM->AttachCurrentThread(&JavaVM.Env, nullptr);

		// Make sure JavaRT is valid. Can be re-set by OnAcquireThreadOwnership
		JavaRT = JavaVM;
		RenderThreadId = gettid();

		LoadingIconParms = vrapi_DefaultFrameParms(&JavaVM, VRAPI_FRAME_INIT_LOADING_ICON, vrapi_GetTimeInSeconds(), nullptr);
		LoadingIconParms.MinimumVsyncs = MinimumVsyncs;

 		DefaultFrameParms = vrapi_DefaultFrameParms(&JavaVM, VRAPI_FRAME_INIT_DEFAULT, vrapi_GetTimeInSeconds(), nullptr);
 		DefaultFrameParms.MinimumVsyncs = MinimumVsyncs;
 		DefaultFrameParms.ExtraLatencyMode = (bExtraLatencyMode) ? VRAPI_EXTRA_LATENCY_MODE_ON : VRAPI_EXTRA_LATENCY_MODE_OFF;

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

void FCustomPresent::LeaveVRMode_RenderThread()
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

void FCustomPresent::OnAcquireThreadOwnership()
{
	UE_LOG(LogHMD, Log, TEXT("!!! Rendering thread is acquired! tid = %d"), gettid());

	JavaRT.Vm = GJavaVM;
	JavaRT.ActivityObject = ActivityObject;
	GJavaVM->AttachCurrentThread(&JavaRT.Env, nullptr);
	RenderThreadId = gettid();
}

void FCustomPresent::OnReleaseThreadOwnership()
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

void FCustomPresent::SetLoadingIconMode(bool bLoadingIconActive)
{
	bLoadingIconIsActive = bLoadingIconActive;
}

bool FCustomPresent::IsInLoadingIconMode() const
{
	return bLoadingIconIsActive;
}

void FCustomPresent::SetLoadingIconTexture_RenderThread(FTextureRHIRef InTexture)
{
	check(IsInRenderingThread());

	if (LoadingIconTextureSet.IsValid())
	{
		//UE_LOG(LogHMD, Log, TEXT("Freeing loading icon textureSet %p"), LoadingIconTextureSet);
		//vrapi_DestroyTextureSwapChain(LoadingIconTextureSet);
		LoadingIconTextureSet->ReleaseResources();
		LoadingIconTextureSet = nullptr;
	}

	// Reset LoadingIconParms
	LoadingIconParms = vrapi_DefaultFrameParms(&JavaRT, VRAPI_FRAME_INIT_LOADING_ICON, vrapi_GetTimeInSeconds(), nullptr);
	LoadingIconParms.MinimumVsyncs = MinimumVsyncs;

	if (InTexture && InTexture->GetTexture2D())
	{
		const uint32 SizeX = InTexture->GetTexture2D()->GetSizeX();
		const uint32 SizeY = InTexture->GetTexture2D()->GetSizeY();

		LoadingIconTextureSet = FCustomPresent::CreateTextureSet(SizeX, SizeY, EPixelFormat::PF_B8G8R8A8, 0, false);
		CopyTexture_RenderThread(FRHICommandListExecutor::GetImmediateCommandList(), LoadingIconTextureSet->GetRHITexture2D(), InTexture->GetTexture2D() , SizeX, SizeY, FIntRect(), FIntRect(), false);
	}
}

void FCustomPresent::CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef DstTexture, FTextureRHIParamRef SrcTexture, int SrcSizeX, int SrcSizeY, 
	FIntRect DstRect, FIntRect SrcRect, bool bAlphaPremultiply) const
{
	check(IsInRenderingThread());

	if (DstRect.IsEmpty())
	{
		DstRect = FIntRect(1, 1, DstTexture->GetSizeX() - 2, DstTexture->GetSizeY() - 2);
	}
	const uint32 ViewportWidth = DstRect.Width();
	const uint32 ViewportHeight = DstRect.Height();
	const FIntPoint TargetSize(ViewportWidth, ViewportHeight);

	const float SrcTextureWidth = SrcSizeX;
	const float SrcTextureHeight = SrcSizeY;
	float U = 0.f, V = 0.f, USize = 1.f, VSize = 1.f;
	if (!SrcRect.IsEmpty())
	{
		U = SrcRect.Min.X / SrcTextureWidth;
		V = SrcRect.Min.Y / SrcTextureHeight;
		USize = SrcRect.Width() / SrcTextureWidth;
		VSize = SrcRect.Height() / SrcTextureHeight;
	}

	FRHITexture* SrcTextureRHI = SrcTexture;

	SetRenderTarget(RHICmdList, DstTexture, FTextureRHIRef());
	//RHICmdList.Clear(true, FLinearColor(1.0f, 0.0f, 0.0f, 1.0f), false, 0.0f, false, 0, FIntRect()); // @DBG
	RHICmdList.Clear(true, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), false, 0.0f, false, 0, FIntRect());
	RHICmdList.SetViewport(DstRect.Min.X, DstRect.Min.Y, 0, DstRect.Max.X, DstRect.Max.Y, 1.0f);

	if (bAlphaPremultiply)
	{
		// for quads, write RGBA, RGB = src.rgb * src.a + dst.rgb * 0, A = src.a + dst.a * 0
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
	}
	else
	{
		// for mirror window
		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	}
	RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	const auto FeatureLevel = GMaxRHIFeatureLevel;
	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
	TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

	static FGlobalBoundShaderState BoundShaderState;
	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTextureRHI);

	RendererModule->DrawRectangle(
		RHICmdList,
		0, 0,
		ViewportWidth, ViewportHeight,
		U, V,
		USize, VSize,
		TargetSize,
		FIntPoint(1, 1),
		*VertexShader,
		EDRF_Default);
}

void FCustomPresent::RenderLoadingIcon_RenderThread(uint32 FrameIndex)
{
	check(IsInRenderingThread());
	LoadingIconParms.FrameIndex = FrameIndex;
	DoRenderLoadingIcon_RenderThread(2, 0, 0);
}

void FCustomPresent::DoRenderLoadingIcon_RenderThread(int CpuLevel, int GpuLevel, pid_t GameTid)
{
	check(IsInRenderingThread());

	if (OvrMobile)
	{
		LoadingIconParms.PerformanceParms = DefaultPerfParms;
		if (CpuLevel >= 0)
		{
			LoadingIconParms.PerformanceParms.CpuLevel = CpuLevel;
		}
		if (GpuLevel >= 0)
		{
			LoadingIconParms.PerformanceParms.GpuLevel = GpuLevel;
		}
		if (GameTid)
		{
			LoadingIconParms.PerformanceParms.MainThreadTid = GameTid;
		}
		LoadingIconParms.PerformanceParms.RenderThreadTid = gettid();

		if (LoadingIconTextureSet.IsValid())
		{
			for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
			{
				LoadingIconParms.Layers[VRAPI_FRAME_LAYER_TYPE_OVERLAY].Textures[eye].ColorTextureSwapChain = LoadingIconTextureSet->GetTextureSet()->GetColorTextureSet();
			}
		}

		SystemActivities_Update_RenderThread();

		vrapi_SubmitFrame(OvrMobile, &LoadingIconParms);
	}
}

void FCustomPresent::PushBlackFinal(const FGameFrame* frame)
{
	check(IsInRenderingThread());

	FScopeLock lock(&OvrMobileLock);
	if (OvrMobile)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("+++++++ PushBlackFinal() ++++++, On RT! tid = %d"), gettid());
		
		check(JavaRT.Vm != nullptr);
		ovrFrameParms frameParms = vrapi_DefaultFrameParms(&JavaRT, VRAPI_FRAME_INIT_BLACK_FINAL, vrapi_GetTimeInSeconds(), nullptr);
		frameParms.PerformanceParms = DefaultPerfParms;
		frameParms.Java = JavaRT;
		if (frame)
		{
			const FSettings* Settings = frame->GetSettings();
			frameParms.PerformanceParms.MainThreadTid = frame->GameThreadId;
			frameParms.FrameIndex = frame->FrameNumber;
			frameParms.PerformanceParms.CpuLevel = Settings->CpuLevel;
			frameParms.PerformanceParms.GpuLevel = Settings->GpuLevel;
		}
		frameParms.PerformanceParms.RenderThreadTid = gettid();

		vrapi_SubmitFrame(OvrMobile, &frameParms);
		FPlatformMisc::LowLevelOutputDebugString(TEXT("------- PushBlackFinal() ------"));
	}
}

void FCustomPresent::PushFrame(FLayerManager* pInLayerMgr, const FGameFrame* CurrentFrame)
{
	check(IsInRenderingThread());

	FScopeLock lock(&OvrMobileLock);
	if (OvrMobile)
	{
		const FSettings* Settings = CurrentFrame->GetSettings();
		if (IsInLoadingIconMode())
		{
			LoadingIconParms.FrameIndex = CurrentFrame->FrameNumber;
			DoRenderLoadingIcon_RenderThread(2, 0, CurrentFrame->GameThreadId);
		}
		else
		{
			check(JavaRT.Vm != nullptr);
			ovrFrameParms frameParms = vrapi_DefaultFrameParms(&JavaRT, VRAPI_FRAME_INIT_DEFAULT, vrapi_GetTimeInSeconds(), nullptr);
			frameParms.MinimumVsyncs = MinimumVsyncs;
			frameParms.ExtraLatencyMode = (bExtraLatencyMode) ? VRAPI_EXTRA_LATENCY_MODE_ON : VRAPI_EXTRA_LATENCY_MODE_OFF;
			frameParms.FrameIndex = CurrentFrame->FrameNumber;
			frameParms.PerformanceParms = DefaultPerfParms;
			frameParms.PerformanceParms.CpuLevel = Settings->CpuLevel;
			frameParms.PerformanceParms.GpuLevel = Settings->GpuLevel;
			frameParms.PerformanceParms.MainThreadTid = CurrentFrame->GameThreadId;
			frameParms.PerformanceParms.RenderThreadTid = gettid();
			SystemActivities_Update_RenderThread();

			pInLayerMgr->SubmitFrame_RenderThread(OvrMobile, &frameParms);
		}
	}
}

void FCustomPresent::SystemActivities_Update_RenderThread()
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

int32 FCustomPresent::LockSubmitFrame() 
{ 
	if (GEngine)
	{
		// limit FPS for render thread while submiting frame is locked
		GEngine->SetMaxFPS(30); 
	}
	return SubmitFrameLocker.Increment(); 
}

int32 FCustomPresent::UnlockSubmitFrame() 
{
	if (GEngine)
	{
		// restore full speed to render thread
		GEngine->SetMaxFPS(0);
	}
	return SubmitFrameLocker.Decrement(); 
}

#endif //GEARVR_SUPPORTED_PLATFORMS

