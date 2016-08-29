// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusRiftPrivatePCH.h"
#include "OculusRiftHMD.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#include "OculusRiftLayers.h"
#include "MediaTexture.h"

FRenderLayer::FRenderLayer(FHMDLayerDesc& InDesc) :
	FHMDRenderLayer(InDesc)
{
	FMemory::Memset(Layer, 0);
	switch (InDesc.GetType())
	{
	case FHMDLayerDesc::Eye:
		Layer.Header.Type = ovrLayerType_EyeFov;
		break;
	case FHMDLayerDesc::Quad:
		Layer.Header.Type = ovrLayerType_Quad;
		break;

	default:
		check(0); // unsupported layer type
	}
}

FRenderLayer::~FRenderLayer()
{
}

TSharedPtr<FHMDRenderLayer> FRenderLayer::Clone() const
{
	TSharedPtr<FHMDRenderLayer> NewLayer = MakeShareable(new FRenderLayer(*this));
	return NewLayer;
}


//////////////////////////////////////////////////////////////////////////
FLayerManager::FLayerManager(FCustomPresent* InBridge) :
	pPresentBridge(InBridge)
	, LayersList(nullptr)
	, LayersListLen(0)
	, EyeLayerId(0)
	, bTextureSetsAreInvalid(true)
	, bInitialized(false)
{
}

FLayerManager::~FLayerManager()
{
	check(!bInitialized);
}

void FLayerManager::Startup()
{
	if (!bInitialized)
	{
		FHMDLayerManager::Startup();

		auto EyeLayer = AddLayer(FHMDLayerDesc::Eye, INT_MIN, Layer_UnknownOrigin, EyeLayerId);
		check(EyeLayer.IsValid());
		bInitialized = true;
		bTextureSetsAreInvalid = true;
	}
}

void FLayerManager::Shutdown()
{
	if (bInitialized)
	{
		ReleaseTextureSets();
		FMemory::Free(LayersList);
		LayersList = nullptr;
		LayersListLen = 0;

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

TSharedPtr<FHMDRenderLayer> FLayerManager::CreateRenderLayer_RenderThread(FHMDLayerDesc& InDesc)
{
	TSharedPtr<FHMDRenderLayer> NewLayer = MakeShareable(new FRenderLayer(InDesc));
	return NewLayer;
}

FRenderLayer& FLayerManager::GetEyeLayer_RenderThread() const
{
	check(IsInRenderingThread());
	check(!bLayersChanged);
	check(LayersToRender.Num() > 0);
	check(LayersToRender[0].IsValid());
	auto LayerDesc = static_cast<FRenderLayer*>(LayersToRender[0].Get());
	check(LayerDesc->Layer.Header.Type == ovrLayerType_EyeFov);
	return *LayerDesc;
}

void FLayerManager::ReleaseTextureSets_RenderThread_NoLock()
{
	check(IsInRenderingThread());

	InvalidateTextureSets();

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
	check(IsInRenderingThread());

	const bool bLayersWereChanged = bLayersChanged;

	const FGameFrame* CurrentFrame = static_cast<const FGameFrame*>(InCurrentFrame);

	// Call base method first, it will make sure the LayersToRender is ready
	FHMDLayerManager::PreSubmitUpdate_RenderThread(RHICmdList, CurrentFrame, ShowFlagsRendering);

	const uint32 NumLayers = LayersToRender.Num();

	if (bLayersWereChanged)
	{
		float WorldToMetersScale = CurrentFrame->Settings->WorldToMetersScale;

		// Create array of ovrLayerHeaders, using LayersToRender array
		LayersList = (ovrLayerHeader**)FMemory::Realloc(LayersList, NumLayers * sizeof(LayersList[0]));
		LayersListLen = 0;
		for (uint32 i = 0, n = NumLayers; i < n; ++i)
		{
			auto RenderLayer = static_cast<FRenderLayer*>(LayersToRender[i].Get());
			// exclude not fully setup layers
			if (RenderLayer->IsFullySetup())
			{
				LayersList[LayersListLen++] = &RenderLayer->Layer.Header;
			}
		}
	}

	const float WorldToMetersScale = CurrentFrame->Settings->WorldToMetersScale;

	const FSettings* FrameSettings = CurrentFrame->GetSettings();

	for (uint32 i = 0; i < NumLayers; ++i)
	{
		auto RenderLayer = static_cast<FRenderLayer*>(LayersToRender[i].Get());
		if (!RenderLayer || !RenderLayer->IsFullySetup())
		{
			continue;
		}
		const FHMDLayerDesc& LayerDesc = RenderLayer->GetLayerDesc();
		switch (LayerDesc.GetType())
		{
		case FHMDLayerDesc::Eye:
		{
			ovrLayer_Union& EyeLayer = RenderLayer->GetOvrLayer();
			EyeLayer.EyeFov.Fov[0] = FrameSettings->EyeRenderDesc[0].Fov;
			EyeLayer.EyeFov.Fov[1] = FrameSettings->EyeRenderDesc[1].Fov;

			EyeLayer.EyeFov.Viewport[0] = ToOVRRecti(FrameSettings->EyeRenderViewport[0]);
			EyeLayer.EyeFov.Viewport[1] = ToOVRRecti(FrameSettings->EyeRenderViewport[1]);

			EyeLayer.EyeFov.ColorTexture[0] = RenderLayer->GetSwapTextureSet();
			EyeLayer.EyeFov.ColorTexture[1] = RenderLayer->GetSwapTextureSet();

#if DO_CHECK
			// some runtime checks to make sure swaptextureset is valid
			if (EyeLayer.EyeFov.ColorTexture[0] && pPresentBridge->GetSession()->IsActive())
			{
				FOvrSessionShared::AutoSession OvrSession(pPresentBridge->GetSession());
				ovrTextureSwapChainDesc schDesc;
				check(OVR_SUCCESS(ovr_GetTextureSwapChainDesc(OvrSession, EyeLayer.EyeFov.ColorTexture[0], &schDesc)));
				check(schDesc.Type == ovrTexture_2D);
			}
#endif

			// always use HighQuality for eyebuffers; the bHighQuality flag in LayerDesc means mipmaps are generated.
			EyeLayer.EyeFov.Header.Flags = ovrLayerFlag_HighQuality;

			if (ShowFlagsRendering)
			{
				EyeLayer.EyeFov.RenderPose[0] = CurrentFrame->CurEyeRenderPose[0];
				EyeLayer.EyeFov.RenderPose[1] = CurrentFrame->CurEyeRenderPose[1];
			}
			else
			{
				EyeLayer.EyeFov.RenderPose[0] = ovrPosef(OVR::Posef());
				EyeLayer.EyeFov.RenderPose[1] = ovrPosef(OVR::Posef());
			}

			RenderLayer->CommitTextureSet(RHICmdList);
			break;
		}
		case FHMDLayerDesc::Quad:
		{
			OVR::Posef pose;
			pose.Rotation = ToOVRQuat<OVR::Quatf>(LayerDesc.GetTransform().GetRotation());
			pose.Translation = ToOVRVector_U2M<OVR::Vector3f>(LayerDesc.GetTransform().GetTranslation(), WorldToMetersScale);

			// Physical size of the quad in meters.
			OVR::Vector2f quadSize(LayerDesc.GetQuadSize().X / WorldToMetersScale, LayerDesc.GetQuadSize().Y / WorldToMetersScale);

			// apply the scale from transform. We use Y for width and Z for height to match the UE coord space
			quadSize.x *= LayerDesc.GetTransform().GetScale3D().Y;
			quadSize.y *= LayerDesc.GetTransform().GetScale3D().Z;

			FQuat PlayerOrientation = CurrentFrame->PlayerOrientation;

			if (LayerDesc.IsWorldLocked())
			{
				// apply BaseOrientation
				PlayerOrientation = FrameSettings->BaseOrientation.Inverse() * CurrentFrame->PlayerOrientation;

				// calculate the location of the quad considering the player's location/orientation
				OVR::Posef PlayerTorso(ToOVRQuat<OVR::Quatf>(PlayerOrientation),
					ToOVRVector_U2M<OVR::Vector3f>(CurrentFrame->PlayerLocation, WorldToMetersScale));
				pose = PlayerTorso.Inverted() * pose;
			}

			RenderLayer->Layer.Quad.Header.Type = ovrLayerType_Quad;
			// LayerDesc.IsHighQuality() is used to gen mipmaps; the HQ flag on layer is always set (means, Aniso Filtering).
			RenderLayer->Layer.Quad.Header.Flags = ovrLayerFlag_HighQuality | (LayerDesc.IsHeadLocked() ? ovrLayerFlag_HeadLocked : 0);
			RenderLayer->Layer.Quad.QuadPoseCenter = pose;
			RenderLayer->Layer.Quad.QuadSize = quadSize;

			RenderLayer->Layer.Quad.Viewport = OVR::Recti();

			FTextureRHIRef Texture = LayerDesc.GetTexture();
			int32 SizeX = 16, SizeY = 16; // 16x16 default texture size for black rect (if the actual texture is not set)
			EPixelFormat Format = EPixelFormat::PF_B8G8R8A8;
			bool bTextureChanged = LayerDesc.IsTextureChanged();
			bool isStatic = !(LayerDesc.GetFlags() & IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE);

			if (Texture)
			{

				SizeX = Texture->GetTexture2D()->GetSizeX();
				SizeY = Texture->GetTexture2D()->GetSizeY();

				Format = Texture->GetFormat();

				FHeadMountedDisplay::QuantizeBufferSize(SizeX, SizeY, 32);

				RenderLayer->Layer.Quad.Viewport = OVR::Recti(OVR::Sizei(SizeX, SizeY));
				const FBox2D vp = LayerDesc.GetTextureViewport();
				if (vp.bIsValid)
				{
					RenderLayer->Layer.Quad.Viewport.Pos.x = float(vp.Min.X) * SizeX;
					RenderLayer->Layer.Quad.Viewport.Pos.y = float(vp.Min.Y) * SizeY;
					RenderLayer->Layer.Quad.Viewport.Size.w = float(vp.Max.X - vp.Min.X) * SizeX;
					RenderLayer->Layer.Quad.Viewport.Size.h = float(vp.Max.Y - vp.Min.Y) * SizeY;
				}
			}

			bTextureChanged |= (Texture && LayerDesc.HasPendingTextureCopy()) ? true : false;
			bTextureChanged |= (LayerDesc.GetFlags() & IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE) ? true : false;

			uint32 NumMips = (LayerDesc.IsHighQuality() ? 0 : 1);
			if (RenderLayer->TextureSet.IsValid() && (bTextureSetsAreInvalid ||
				RenderLayer->TextureSet->GetSourceSizeX() != SizeX ||
				RenderLayer->TextureSet->GetSourceSizeY() != SizeY ||
				RenderLayer->TextureSet->GetSourceFormat() != Format ||
				RenderLayer->TextureSet->GetSourceNumMips() != NumMips ||
				(bTextureChanged && RenderLayer->TextureSet->IsStaticImage())))
			{
				RenderLayer->TextureSet->ReleaseResources();
				RenderLayer->TextureSet.Reset();
			}
			if (!RenderLayer->TextureSet.IsValid() || !RenderLayer->TextureSet->GetRHITexture())
			{
				// create texture set
				check(pPresentBridge);
				if (isStatic)
				{
					RenderLayer->TextureSet = pPresentBridge->CreateTextureSet(SizeX, SizeY, Format, NumMips, FCustomPresent::DefaultStaticImage);
				}
				else
				{
					RenderLayer->TextureSet = pPresentBridge->CreateTextureSet(SizeX, SizeY, Format, NumMips, FCustomPresent::DefaultLinear);
				}
				bTextureChanged = true;
			}
			RenderLayer->Layer.Quad.ColorTexture = RenderLayer->GetSwapTextureSet();

#if DO_CHECK
			// some runtime checks to make sure swaptextureset is valid
			if (RenderLayer->Layer.Quad.ColorTexture && pPresentBridge->GetSession()->IsActive())
			{
				FOvrSessionShared::AutoSession OvrSession(pPresentBridge->GetSession());
				ovrTextureSwapChainDesc schDesc;
				check(OVR_SUCCESS(ovr_GetTextureSwapChainDesc(OvrSession, RenderLayer->Layer.Quad.ColorTexture, &schDesc)));
				check(schDesc.Type == ovrTexture_2D);
			}
#endif

			// Copy texture if it was changed
			if (RenderLayer->TextureSet.IsValid() && bTextureChanged)
			{
				if (Texture)
				{
					FRHITexture2D* Texture2D = Texture->GetTexture2D();
					pPresentBridge->CopyTexture_RenderThread(
						RHICmdList,
						RenderLayer->TextureSet->GetRHITexture2D(),
						Texture2D,
						Texture2D->GetSizeX(),
						Texture2D->GetSizeY(),
						FIntRect(),
						FIntRect(),
						true,
						!!(LayerDesc.GetFlags() & IStereoLayers::LAYER_FLAG_TEX_NO_ALPHA_CHANNEL));
					LayerDesc.ClearPendingTextureCopy();
				}
				RenderLayer->CommitTextureSet(RHICmdList);
			}
			break;
		}
		}
		RenderLayer->ResetChangedFlags();
	}
	bTextureSetsAreInvalid = false;
}

ovrResult FLayerManager::SubmitFrame_RenderThread(ovrSession OvrSession, const FGameFrame* RenderFrame, bool AdvanceToNextElem)
{
	if (bTextureSetsAreInvalid)
	{
		UE_LOG(LogHMD, Log, TEXT("FLayerManager::SubmitFrame_RenderThread: skipping frame, no valid texture sets"));
		return ovrError_TextureSwapChainInvalid;
	}

	check(RenderFrame);

	// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
	const FSettings* FrameSettings = RenderFrame->GetSettings();

	// Set up positional data.
	ovrViewScaleDesc viewScaleDesc;
	viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;
	viewScaleDesc.HmdToEyeOffset[0] = FrameSettings->EyeRenderDesc[0].HmdToEyeOffset;
	viewScaleDesc.HmdToEyeOffset[1] = FrameSettings->EyeRenderDesc[1].HmdToEyeOffset;

	const uint32 frameNumber = RenderFrame->FrameNumber;
	ovrResult res = ovr_SubmitFrame(OvrSession, frameNumber, &viewScaleDesc, LayersList, LayersListLen);

	if (AdvanceToNextElem)
	{
		for (uint32 i = 0; i < LayersListLen; ++i)
		{
			auto RenderLayer = static_cast<FRenderLayer*>(LayersToRender[i].Get());
			if (RenderLayer->TextureSet.IsValid())
			{
				RenderLayer->TextureSet->SwitchToNextElement();
			}
		}
	}

	LastSubmitFrameResult.Set(int32(res));

	if (OVR_FAILURE(res))
	{
		UE_LOG(LogHMD, Warning, TEXT("Error at SubmitFrame, err = %d"), int(res));

		if (res == ovrError_DisplayLost)
		{
			ReleaseTextureSets_RenderThread_NoLock();
			bTextureSetsAreInvalid = true;
		}
	}
	return res;
}

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
