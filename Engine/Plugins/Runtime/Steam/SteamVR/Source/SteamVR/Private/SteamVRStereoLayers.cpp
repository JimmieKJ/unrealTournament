// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "SteamVRPrivatePCH.h"
#include "SteamVRHMD.h"

#if STEAMVR_SUPPORTED_PLATFORMS

// enable to ensure all openvr calls succeed
#if 0
static vr::EVROverlayError sOvrError;
#define OVR_VERIFY(x) sOvrError = x; ensure(sOvrError == vr::VROverlayError_None)
#else
#define OVR_VERIFY(x) x
#endif


/*=============================================================================
*
* Helper functions
*
*/

//=============================================================================
static uint32 IdToIndex(uint32 LayerId)
{
	return LayerId - 1;
}

//=============================================================================
static uint32 IndexToId(uint32 LayerIndex)
{
	return LayerIndex + 1;
}

//=============================================================================
static void TransformToSteamSpace(const FTransform& In, vr::HmdMatrix34_t& Out, float WorldToMeterScale)
{
	const FRotator InRot = In.Rotator();
	FRotator OutRot(InRot.Yaw, -InRot.Roll, -InRot.Pitch);

	const FVector InPos = In.GetTranslation();
	FVector OutPos(InPos.Y, InPos.Z, -InPos.X);
	OutPos /= WorldToMeterScale;

	const FVector InScale = In.GetScale3D();
	FVector OutScale(InScale.Y, InScale.Z, -InScale.X);
	OutScale /= WorldToMeterScale;

	Out = FSteamVRHMD::ToHmdMatrix34(FTransform(OutRot, OutPos, OutScale).ToMatrixNoScale());
}


/*=============================================================================
*
* FSteamVRHMD's IStereoLayer implementation
*
*/

//=============================================================================
uint32 FSteamVRHMD::CreateLayer(const FLayerDesc& InLayerDesc)
{
	FScopeLock LayerLock(&LayerCritSect);

	uint32 NewIndex;
	if (LayerFreeIndices.Num())
	{
		NewIndex = LayerFreeIndices.Pop(false);
	}
	else
	{
		NewIndex = Layers.AddUninitialized();
	}
	check(NewIndex < (uint32)Layers.Num());
	
	FLayer* NewLayer = Layers.GetData() + NewIndex;
	FMemory::Memzero(NewLayer, sizeof(*NewLayer));

	NewLayer->LayerId = IndexToId(NewIndex);
	NewLayer->LayerDesc = InLayerDesc;
	NewLayer->OverlayHandle = vr::k_ulOverlayHandleInvalid;

	UpdateLayer(*NewLayer);

	return NewLayer->LayerId;
}

//=============================================================================
void FSteamVRHMD::DestroyLayer(uint32 LayerId)
{
	if (!LayerId)
	{
		return;
	}

	FScopeLock LayerLock(&LayerCritSect);

	uint32 LayerIndex = IdToIndex(LayerId);
	check(LayerIndex < (uint32)Layers.Num());

	FLayer* ObsoleteLayer = Layers.GetData() + LayerIndex;
	ObsoleteLayer->LayerId = 0;
	ObsoleteLayer->LayerDesc.Texture = nullptr;

	UpdateLayer(*ObsoleteLayer);

	LayerFreeIndices.Push(LayerIndex);
}

//=============================================================================
void FSteamVRHMD::SetLayerDesc(uint32 LayerId, const FLayerDesc& InLayerDesc)
{
	if (!LayerId)
	{
		return;
	}

	FScopeLock LayerLock(&LayerCritSect);

	uint32 LayerIndex = IdToIndex(LayerId);
	FLayer& Layer = Layers[LayerIndex];
	Layer.LayerDesc = InLayerDesc;

	UpdateLayer(Layer);
}

//=============================================================================
bool FSteamVRHMD::GetLayerDesc(uint32 LayerId, FLayerDesc& OutLayerDesc)
{
	if (!LayerId)
	{
		return false;
	}

	uint32 LayerIndex = IdToIndex(LayerId);
	OutLayerDesc = Layers[LayerIndex].LayerDesc;
	return true;
}

//=============================================================================
void FSteamVRHMD::MarkTextureForUpdate(uint32 LayerId)
{
	if (!LayerId)
	{
		return;
	}

	uint32 LayerIndex = IdToIndex(LayerId);
	Layers[LayerIndex].bUpdateTexture = true;
}

//=============================================================================
void FSteamVRHMD::UpdateLayer(FLayer& Layer) const
{
	FScopeLock LayerLock(&LayerCritSect);

	if (Layer.LayerDesc.Texture.IsValid() && Layer.OverlayHandle == vr::k_ulOverlayHandleInvalid)
	{
		FString OverlayName = FString::Printf(TEXT("StereoLayer:%u"), Layer.LayerId);
		const char* OverlayNameAnsiStr = TCHAR_TO_ANSI(*OverlayName);
		OVR_VERIFY(VROverlay->CreateOverlay(OverlayNameAnsiStr, OverlayNameAnsiStr, &Layer.OverlayHandle));
		OVR_VERIFY(VROverlay->HideOverlay(Layer.OverlayHandle));
		Layer.bUpdateTexture = true;
	}
	else if (!Layer.LayerDesc.Texture.IsValid() && Layer.OverlayHandle != vr::k_ulOverlayHandleInvalid)
	{
		OVR_VERIFY(VROverlay->DestroyOverlay(Layer.OverlayHandle));
		Layer.OverlayHandle = vr::k_ulOverlayHandleInvalid;
	}

	if (Layer.OverlayHandle == vr::k_ulOverlayHandleInvalid)
	{
		return;
	}

	// UVs
	vr::VRTextureBounds_t TextureBounds;
    TextureBounds.uMin = Layer.LayerDesc.UVRect.Min.X;
    TextureBounds.uMax = Layer.LayerDesc.UVRect.Max.X;
    TextureBounds.vMin = Layer.LayerDesc.UVRect.Min.Y;
    TextureBounds.vMax = Layer.LayerDesc.UVRect.Max.Y;
	OVR_VERIFY(VROverlay->SetOverlayTextureBounds(Layer.OverlayHandle, &TextureBounds));
	const float WorldToMeterScale = FMath::Max(GetWorldToMetersScale(), 0.1f);
	OVR_VERIFY(VROverlay->SetOverlayWidthInMeters(Layer.OverlayHandle, Layer.LayerDesc.QuadSize.X / WorldToMeterScale));
	OVR_VERIFY(VROverlay->SetOverlayTexelAspect(Layer.OverlayHandle, Layer.LayerDesc.QuadSize.X / Layer.LayerDesc.QuadSize.Y));
	OVR_VERIFY(VROverlay->SetOverlaySortOrder(Layer.OverlayHandle, Layer.LayerDesc.Priority));

	// Transform
	switch (Layer.LayerDesc.Type)
	{
	case ELayerType::WorldLocked:
#if 0
	{
		// needs final implementation

		FQuat PlayerOrientation = BaseOrientation.Inverse() * PlayerOrientation;
		FTransform PlayerTorso(PlayerOrientation, PlayerPosition);

		FTransform Transform = PlayerTorso.Inverse() * Layer.LayerDesc.Transform;

		vr::HmdMatrix34_t HmdTransform;
		TransformToSteamSpace(Transform, HmdTransform, WorldToMeterScale);
		OVR_VERIFY(VROverlay->SetOverlayTransformTrackedDeviceRelative(Layer.OverlayHandle, vr::ETrackingUniverseOrigin::TrackingUniverseSeated, &HmdTransform));
		break;
	}
#endif
	case ELayerType::TrackerLocked:
	{
		vr::HmdMatrix34_t HmdTransform;
		TransformToSteamSpace(Layer.LayerDesc.Transform, HmdTransform, WorldToMeterScale);
		OVR_VERIFY(VROverlay->SetOverlayTransformAbsolute(Layer.OverlayHandle, vr::ETrackingUniverseOrigin::TrackingUniverseSeated, &HmdTransform));
		break;
	}
	case ELayerType::FaceLocked:
	default:
	{
		vr::HmdMatrix34_t HmdTransform;
		TransformToSteamSpace(Layer.LayerDesc.Transform, HmdTransform, WorldToMeterScale);
		OVR_VERIFY(VROverlay->SetOverlayTransformTrackedDeviceRelative(Layer.OverlayHandle, 0, &HmdTransform));
	}
	};
}

//=============================================================================
void FSteamVRHMD::UpdateLayerTextures() const
{
	FScopeLock LayerLock(&LayerCritSect);

	for (auto Layer : Layers)
	{
		if (Layer.OverlayHandle == vr::k_ulOverlayHandleInvalid)
		{
			continue;
		}

		if (Layer.bUpdateTexture || (Layer.LayerDesc.Flags & LAYER_FLAG_TEX_CONTINUOUS_UPDATE))
		{
			vr::Texture_t Texture;
			Texture.handle = Layer.LayerDesc.Texture->GetNativeResource();
			Texture.eType = vr::API_DirectX;
			Texture.eColorSpace = vr::ColorSpace_Auto;
			OVR_VERIFY(VROverlay->SetOverlayTexture(Layer.OverlayHandle, &Texture));
			OVR_VERIFY(VROverlay->ShowOverlay(Layer.OverlayHandle));

			Layer.bUpdateTexture = false;
		}
	}
}

//=============================================================================
IStereoLayers* FSteamVRHMD::GetStereoLayers ()
{
	check(VROverlay);
	return this;
}

#endif //STEAMVR_SUPPORTED_PLATFORMS
