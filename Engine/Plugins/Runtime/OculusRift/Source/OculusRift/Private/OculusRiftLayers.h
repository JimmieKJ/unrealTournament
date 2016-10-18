// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IHeadMountedDisplay.h"
#include "HeadMountedDisplayCommon.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#include "OculusRiftCommon.h"

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
	FTexture2DSetProxy(const FOvrSessionSharedPtr& InOvrSession, FTextureRHIParamRef InTexture, uint32 InSrcSizeX, uint32 InSrcSizeY, EPixelFormat InSrcFormat, uint32 InSrcNumMips) 
		: FTextureSetProxy(InSrcSizeX, InSrcSizeY, InSrcFormat, InSrcNumMips)
		, Session(InOvrSession), RHITexture(InTexture) 
	{
		DestroyDelegateHandle = Session->AddRawDestroyDelegate(this, &FTexture2DSetProxy::OnSessionDestroy);
	}
	~FTexture2DSetProxy()
	{
		Session->RemoveDestroyDelegate(DestroyDelegateHandle);
	}

	virtual ovrTextureSwapChain GetSwapTextureSet() const = 0;
	virtual FTextureRHIRef GetRHITexture() const override { return RHITexture; }
	virtual FTexture2DRHIRef GetRHITexture2D() const override { return (RHITexture) ? RHITexture->GetTexture2D() : nullptr; }

	virtual void SwitchToNextElement() = 0;

	virtual bool Commit(FRHICommandListImmediate& RHICmdList) override
	{
		if (RHITexture.IsValid() && RHITexture->GetNumMips() > 1)
		{
			RHICmdList.GenerateMips(RHITexture);
		}
		ovrTextureSwapChain TextureSwapSet = GetSwapTextureSet();
		if (TextureSwapSet)
		{
			FOvrSessionShared::AutoSession OvrSession(Session);
			ovrResult res = ovr_CommitTextureSwapChain(OvrSession, TextureSwapSet);
			if (!OVR_SUCCESS(res))
			{
				UE_LOG(LogHMD, Warning, TEXT("Error at ovr_CommitTextureSwapChain, err = %d"), int(res));
				return false;
			}
			return true;
		}
		return false;
	}
	virtual bool IsStaticImage() const override
	{
		ovrTextureSwapChain TextureSwapSet = GetSwapTextureSet();
		if (TextureSwapSet)
		{
			FOvrSessionShared::AutoSession OvrSession(Session);
			int len;
			ovrResult res = ovr_GetTextureSwapChainLength(OvrSession, TextureSwapSet, &len);
			if (!OVR_SUCCESS(res))
			{
				UE_LOG(LogHMD, Warning, TEXT("Error at ovr_GetTextureSwapChainLength, err = %d"), int(res));
				return false;
			}
			return len <= 1;
		}
		return false;
	}
protected:
	void OnSessionDestroy(ovrSession InSession)
	{
		ReleaseResources();
	}

protected:
	FOvrSessionSharedPtr Session;
	FTextureRHIRef		 RHITexture;
	FDelegateHandle		 DestroyDelegateHandle;
};
typedef TSharedPtr<FTexture2DSetProxy, ESPMode::ThreadSafe>	FTexture2DSetProxyPtr;

// Implementation of FHMDRenderLayer for OculusRift.
class FRenderLayer : public FHMDRenderLayer
{
	friend class FLayerManager;
public:
	FRenderLayer(FHMDLayerDesc&);
	~FRenderLayer();

	ovrLayer_Union&	GetOvrLayer() { return Layer; }
	const ovrLayer_Union&	GetOvrLayer() const { return Layer; }
	ovrTextureSwapChain GetSwapTextureSet() const
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

	virtual void Startup() override;
	virtual void Shutdown() override;

	FRenderLayer& GetEyeLayer_RenderThread() const;

	const FHMDLayerDesc* GetEyeLayerDesc() const { return GetLayerDesc(EyeLayerId); }

	// Releases all textureSets used by all layers
	virtual void ReleaseTextureSets_RenderThread_NoLock() override;

	void ReleaseTextureSets(); // can be called on any thread

	// Should be called before SubmitFrame is called.
	// Updates sizes, distances, orientations, textures of each layer, as needed.
	virtual void PreSubmitUpdate_RenderThread(FRHICommandListImmediate& RHICmdList, const FHMDGameFrame* CurrentFrame, bool ShowFlagsRendering) override;

	// Submits all the layers
	ovrResult SubmitFrame_RenderThread(ovrSession OvrSession, const class FGameFrame* RenderFrame, bool AdvanceToNextElem);

	ovrResult GetLastSubmitFrameResult() const { return (ovrResult)LastSubmitFrameResult.GetValue(); }

	// Should be called when any TextureSet is released; will be reset in PreSubmitUpdate.
	void InvalidateTextureSets() { bTextureSetsAreInvalid = true; }

protected:
	virtual TSharedPtr<FHMDRenderLayer> CreateRenderLayer_RenderThread(FHMDLayerDesc& InDesc) override;

	virtual uint32 GetTotalNumberOfLayersSupported() const override { return ovrMaxLayerCount; }

protected:
	class FCustomPresent*	pPresentBridge;
	// Complete layer list. Some entries may be null.
	ovrLayerHeader**		LayersList;
	uint32					LayersListLen;
	uint32					EyeLayerId;
	FThreadSafeCounter		LastSubmitFrameResult;
	FThreadSafeBool			bTextureSetsAreInvalid;
	bool					bInitialized : 1;
};

} // namespace
#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
