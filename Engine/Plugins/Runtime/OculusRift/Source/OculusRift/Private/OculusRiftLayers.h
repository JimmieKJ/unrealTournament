// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IHeadMountedDisplay.h"
#include "HeadMountedDisplayCommon.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif

#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	#include <OVR_Version.h>
	#include <OVR_CAPI.h>
	#include <OVR_CAPI_Keys.h>
	#include <Extras/OVR_Math.h>
#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

namespace OculusRift {

// A wrapper class to work with texture sets in abstract way
class FTexture2DSetProxy : public FTextureSetProxy
{
public:
	FTexture2DSetProxy(ovrSession InOvrSession, FTextureRHIParamRef InTexture, uint32 InSrcSizeX, uint32 InSrcSizeY, EPixelFormat InSrcFormat) 
		: FTextureSetProxy(InSrcSizeX, InSrcSizeY, InSrcFormat)
		, OvrSession(InOvrSession), RHITexture(InTexture) {}

	virtual ovrSwapTextureSet* GetSwapTextureSet() const = 0;
	virtual FTextureRHIRef GetRHITexture() const override { return RHITexture; }
	virtual FTexture2DRHIRef GetRHITexture2D() const override { return (RHITexture) ? RHITexture->GetTexture2D() : nullptr; }

	virtual void SwitchToNextElement() = 0;

	virtual bool Commit()
	{
		return true;
	}

protected:
	ovrSession		OvrSession;
	FTextureRHIRef	RHITexture;
};
typedef TSharedPtr<FTexture2DSetProxy, ESPMode::ThreadSafe>	FTexture2DSetProxyParamRef;
typedef TSharedPtr<FTexture2DSetProxy, ESPMode::ThreadSafe>	FTexture2DSetProxyRef;

// Implementation of FHMDRenderLayer for OculusRift.
class FRenderLayer : public FHMDRenderLayer
{
	friend class FLayerManager;
public:
	FRenderLayer(FHMDLayerDesc&);
	~FRenderLayer();

	ovrLayer_Union&	GetOvrLayer() { return Layer; }
	const ovrLayer_Union&	GetOvrLayer() const { return Layer; }
	ovrSwapTextureSet* GetSwapTextureSet() const
	{
		if (TextureSet.IsValid())
		{
			return static_cast<FTexture2DSetProxy*>(TextureSet.Get())->GetSwapTextureSet();
		}
		return nullptr;
	}

	virtual TSharedPtr<FHMDRenderLayer> Clone() const override;

protected:
	ovrLayer_Union			Layer;
};

// Implementation of FHMDLayerManager for Oculus Rift
class FLayerManager : public FHMDLayerManager
{
public:
	FLayerManager(class FCustomPresent*);
	~FLayerManager();

	FRenderLayer& GetEyeLayer_RenderThread() const;

	// Releases all textureSets used by all layers
	virtual void ReleaseTextureSets_RenderThread_NoLock() override;

	// Should be called before SubmitFrame is called.
	// Updates sizes, distances, orientations, textures of each layer, as needed.
	void PreSubmitUpdate_RenderThread(FRHICommandListImmediate& RHICmdList, const FHMDGameFrame* CurrentFrame);

	// Submits all the layers
	ovrResult SubmitFrame(class FViewExtension* RenderContext);

protected:
	virtual TSharedPtr<FHMDRenderLayer> CreateRenderLayer_RenderThread(FHMDLayerDesc& InDesc) override;

protected:
	class FCustomPresent*	pPresentBridge;
	// Complete layer list. Some entries may be null.
	ovrLayerHeader**		LayersList;
	bool					bTextureSetsAreInvalid;
};

} // namespace
#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
