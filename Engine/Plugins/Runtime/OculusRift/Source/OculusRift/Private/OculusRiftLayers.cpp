// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HMDPrivatePCH.h"
#include "OculusRiftHMD.h"
#include "OculusRiftLayers.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

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
	, bTextureSetsAreInvalid(true)
{
	uint32 ID;
	auto EyeLayer = AddLayer(FHMDLayerDesc::Eye, INT_MIN, false, ID);
	check(EyeLayer.IsValid());
}

FLayerManager::~FLayerManager()
{
	FMemory::Free(LayersList);
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

	const uint32 NumLayers = LayersToRender.Num();
	for (uint32 i = 0; i < NumLayers; ++i)
	{
		auto RenderLayer = static_cast<FRenderLayer*>(LayersToRender[i].Get());
		RenderLayer->ReleaseResources();
	}
}

void FLayerManager::PreSubmitUpdate_RenderThread(FRHICommandListImmediate& RHICmdList, const FHMDGameFrame* InCurrentFrame)
{
	check(IsInRenderingThread());

	const bool bLayersWereChanged = bLayersChanged;

	const FGameFrame* CurrentFrame = static_cast<const FGameFrame*>(InCurrentFrame);

	// Call base method first, it will make sure the LayersToRender is ready
	FHMDLayerManager::PreSubmitUpdate_RenderThread(RHICmdList, CurrentFrame);

	const uint32 NumLayers = LayersToRender.Num();

	if (bLayersWereChanged)
	{
		float WorldToMetersScale = CurrentFrame->Settings->WorldToMetersScale;

		// Create array of ovrLayerHeaders, using LayersToRender array
		LayersList = (ovrLayerHeader**)FMemory::Realloc(LayersList, NumLayers * sizeof(LayersList[0]));
		for (uint32 i = 0, n = NumLayers; i < n; ++i)
		{
			auto RenderLayer = static_cast<FRenderLayer*>(LayersToRender[i].Get());
			LayersList[i] = &RenderLayer->Layer.Header;
		}
	}

	const float WorldToMetersScale = CurrentFrame->Settings->WorldToMetersScale;

	const FSettings* FrameSettings = CurrentFrame->GetSettings();

	for (uint32 i = 0; i < NumLayers; ++i)
	{
		auto RenderLayer = static_cast<FRenderLayer*>(LayersToRender[i].Get());
		check(RenderLayer);
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

				if (pPresentBridge->GetRenderContext()->ShowFlags.Rendering)
				{
					EyeLayer.EyeFov.RenderPose[0] = CurrentFrame->CurEyeRenderPose[0];
					EyeLayer.EyeFov.RenderPose[1] = CurrentFrame->CurEyeRenderPose[1];
				}
				else
				{
					EyeLayer.EyeFov.RenderPose[0] = ovrPosef(OVR::Posef());
					EyeLayer.EyeFov.RenderPose[1] = ovrPosef(OVR::Posef());
				}

				RenderLayer->CommitTextureSet();
				break;
			}
			case FHMDLayerDesc::Quad:
			{
				OVR::Posef pose;
				pose.Rotation = ToOVRQuat<OVR::Quatf>(LayerDesc.GetTransform().GetRotation());
				pose.Translation = ToOVRVector_U2M<OVR::Vector3f>(LayerDesc.GetTransform().GetTranslation(), WorldToMetersScale);
				// Now move them relative to the torso.
				//pose = PlayerTorso.Inverted() * pose;
				// Physical size of the quad in meters.
				OVR::Vector2f quadSize(LayerDesc.GetQuadSize().X / WorldToMetersScale, LayerDesc.GetQuadSize().Y / WorldToMetersScale);

				if (LayerDesc.IsWorldLocked())
				{
					// calculate the location of the quad considering the player's location/orientation
					OVR::Posef PlayerTorso(ToOVRQuat<OVR::Quatf>(CurrentFrame->PlayerOrientation), 
										   ToOVRVector_U2M<OVR::Vector3f>(CurrentFrame->PlayerLocation, WorldToMetersScale));
					pose = PlayerTorso.Inverted() * pose;
				}

				RenderLayer->Layer.Quad.Header.Type = ovrLayerType_Quad;
				RenderLayer->Layer.Quad.Header.Flags = (LayerDesc.IsHighQuality() ? ovrLayerFlag_HighQuality : 0) | (LayerDesc.IsHeadLocked() ? ovrLayerFlag_HeadLocked : 0);
				RenderLayer->Layer.Quad.QuadPoseCenter = pose;
				RenderLayer->Layer.Quad.QuadSize = quadSize;

				RenderLayer->Layer.Quad.Viewport = OVR::Recti();

				UTexture* TextureObj = LayerDesc.GetTexture();
				if (TextureObj && TextureObj->Resource && TextureObj->Resource->TextureRHI)
				{
					FTextureRHIRef Texture = TextureObj->Resource->TextureRHI;
					const uint32 SizeX = Texture->GetTexture2D()->GetSizeX();
					const uint32 SizeY = Texture->GetTexture2D()->GetSizeY();
					const EPixelFormat Format = Texture->GetFormat();

					RenderLayer->Layer.Quad.Viewport = OVR::Recti(OVR::Sizei(SizeX, SizeY));
					const FBox2D vp = LayerDesc.GetTextureViewport();
					if (vp.bIsValid)
					{
						RenderLayer->Layer.Quad.Viewport.Pos.x = float(vp.Min.X) * SizeX;
						RenderLayer->Layer.Quad.Viewport.Pos.y = float(vp.Min.Y) * SizeY;
						RenderLayer->Layer.Quad.Viewport.Size.w = float(vp.Max.X - vp.Min.X) * SizeX;
						RenderLayer->Layer.Quad.Viewport.Size.h = float(vp.Max.Y - vp.Min.Y) * SizeY;
					}

					if (RenderLayer->TextureSet.IsValid() && (bTextureSetsAreInvalid || 
						RenderLayer->TextureSet->GetSourceSizeX() != SizeX ||
						RenderLayer->TextureSet->GetSourceSizeY() != SizeY ||
						RenderLayer->TextureSet->GetSourceFormat() != Format))
					{
						RenderLayer->TextureSet->ReleaseResources();
						RenderLayer->TextureSet.Reset();
					}
					if (!RenderLayer->TextureSet.IsValid())
					{
						// create texture set
						check(pPresentBridge);
						RenderLayer->TextureSet = pPresentBridge->CreateTextureSet(SizeX, SizeY, Format);
					}
					RenderLayer->Layer.Quad.ColorTexture = RenderLayer->GetSwapTextureSet();

					// Copy texture
					if (RenderLayer->TextureSet.IsValid())
					{
						pPresentBridge->CopyTexture_RenderThread(RHICmdList, RenderLayer->TextureSet->GetRHITexture2D(), Texture->GetTexture2D());

						RenderLayer->CommitTextureSet();
					}
				}
				break;
			}
		}
	}
	bTextureSetsAreInvalid = false;
}

ovrResult FLayerManager::SubmitFrame(FViewExtension* RenderContext)
{
	// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
	FSettings* FrameSettings = RenderContext->GetFrameSettings();

	// Set up positional data.
	ovrViewScaleDesc viewScaleDesc;
	viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;
	viewScaleDesc.HmdToEyeViewOffset[0] = FrameSettings->EyeRenderDesc[0].HmdToEyeViewOffset;
	viewScaleDesc.HmdToEyeViewOffset[1] = FrameSettings->EyeRenderDesc[1].HmdToEyeViewOffset;

	const uint32 NumLayers = LayersToRender.Num();

	ovrResult res = ovr_SubmitFrame(RenderContext->OvrSession, RenderContext->RenderFrame->FrameNumber, &viewScaleDesc, LayersList, NumLayers);

	if (pPresentBridge->GetRenderContext()->ShowFlags.Rendering)
	{
		for (uint32 i = 0; i < NumLayers; ++i)
		{
			auto RenderLayer = static_cast<FRenderLayer*>(LayersToRender[i].Get());
			if (RenderLayer->TextureSet.IsValid())
			{
				RenderLayer->TextureSet->SwitchToNextElement();
			}
		}
	}

	if (!OVR_SUCCESS(res))
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
