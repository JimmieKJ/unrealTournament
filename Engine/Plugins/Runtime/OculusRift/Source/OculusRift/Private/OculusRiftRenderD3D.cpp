// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "OculusRiftPrivatePCH.h"
#include "OculusRiftHMD.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#if defined(OVR_D3D)

#include "D3D11RHIPrivate.h"
#include "D3D11Util.h"

#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "AllowWindowsPlatformTypes.h"
#endif
#include "OVR_CAPI_D3D.h"
#include "OVR_CAPI_Audio.h"
#include <nvapi.h>
#include <audiodefs.h>
#include <dsound.h>
#include <xaudio2.h>

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"
#include "ScreenRendering.h"

#include "SlateBasics.h"

//-------------------------------------------------------------------------------------------------
// FD3D11Texture2DSet
//-------------------------------------------------------------------------------------------------

class FD3D11Texture2DSet : public FD3D11Texture2D
{
public:
	FD3D11Texture2DSet(
		class FD3D11DynamicRHI* InD3DRHI,
		ID3D11Texture2D* InResource,
		ID3D11ShaderResourceView* InShaderResourceView,
		bool bInCreatedRTVsPerSlice,
		int32 InRTVArraySize,
		const TArray<TRefCountPtr<ID3D11RenderTargetView> >& InRenderTargetViews,
		TRefCountPtr<ID3D11DepthStencilView>* InDepthStencilViews,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		uint32 InNumSamples,
		EPixelFormat InFormat,
		bool bInCubemap,
		uint32 InFlags,
		bool bInPooled
		)
		: FD3D11Texture2D(
		InD3DRHI,
		InResource,
		InShaderResourceView,
		bInCreatedRTVsPerSlice,
		InRTVArraySize,
		InRenderTargetViews,
		InDepthStencilViews,
		InSizeX,
		InSizeY,
		InSizeZ,
		InNumMips,
		InNumSamples,
		InFormat,
		bInCubemap,
		InFlags,
		bInPooled,
		FClearValueBinding::None
		)
	{
		TextureSet = nullptr;
	}
	~FD3D11Texture2DSet()
	{
		check(!TextureSet);
	}

	void ReleaseResources(ovrSession InOvrSession);
	void SwitchToNextElement(ovrSession InOvrSession);
	void AddTexture(ID3D11Texture2D*, ID3D11ShaderResourceView*, TArray<TRefCountPtr<ID3D11RenderTargetView> >* = nullptr);

	ovrTextureSwapChain GetSwapTextureSet() const { return TextureSet; }

	static FD3D11Texture2DSet* D3D11CreateTexture2DSet(
		FD3D11DynamicRHI* InD3D11RHI,
		const FOvrSessionSharedPtr& OvrSession,
		ovrTextureSwapChain InTextureSet,
		const D3D11_TEXTURE2D_DESC& InDsDesc,
		EPixelFormat InFormat,
		uint32 InFlags
		);
protected:
	void InitWithCurrentElement(int CurrentIndex);

	struct TextureElement
	{
		TRefCountPtr<ID3D11Texture2D> Texture;
		TRefCountPtr<ID3D11ShaderResourceView> SRV;
		TArray<TRefCountPtr<ID3D11RenderTargetView> > RTVs;
	};
	TArray<TextureElement> Textures;

	ovrTextureSwapChain TextureSet;
};

class FD3D11Texture2DSetProxy : public FTexture2DSetProxy
{
public:
	FD3D11Texture2DSetProxy(const FOvrSessionSharedPtr& InOvrSession, FTextureRHIRef InTexture, uint32 SrcSizeX, uint32 SrcSizeY, EPixelFormat SrcFormat, uint32 SrcNumMips)
		: FTexture2DSetProxy(InOvrSession, InTexture, SrcSizeX, SrcSizeY, SrcFormat, SrcNumMips) {}

	virtual ovrTextureSwapChain GetSwapTextureSet() const override
	{
		if (!RHITexture.IsValid())
		{
			return nullptr;
		}
		auto D3D11TS = static_cast<FD3D11Texture2DSet*>(RHITexture->GetTexture2D());
		return D3D11TS->GetSwapTextureSet();
	}

	virtual void SwitchToNextElement() override
	{
		if (RHITexture.IsValid())
		{
			auto D3D11TS = static_cast<FD3D11Texture2DSet*>(RHITexture->GetTexture2D());
			FOvrSessionShared::AutoSession OvrSession(Session);
			D3D11TS->SwitchToNextElement(OvrSession);
		}
	}

	virtual void ReleaseResources() override
	{
		if (RHITexture.IsValid())
		{
			auto D3D11TS = static_cast<FD3D11Texture2DSet*>(RHITexture->GetTexture2D());
			FOvrSessionShared::AutoSession OvrSession(Session);
			D3D11TS->ReleaseResources(OvrSession);
			RHITexture = nullptr;
		}
	}

protected:
};


void FD3D11Texture2DSet::AddTexture(ID3D11Texture2D* InTexture, ID3D11ShaderResourceView* InSRV, TArray<TRefCountPtr<ID3D11RenderTargetView> >* InRTVs)
{
	TextureElement element;
	element.Texture = InTexture;
	element.SRV = InSRV;
	if (InRTVs)
	{
		element.RTVs.Empty(InRTVs->Num());
		for (int32 i = 0; i < InRTVs->Num(); ++i)
		{
			element.RTVs.Add((*InRTVs)[i]);
		}
	}
	Textures.Push(element);
}

void FD3D11Texture2DSet::SwitchToNextElement(ovrSession InOvrSession)
{
	check(TextureSet);
	
	int CurrentIndex;
	ovr_GetTextureSwapChainCurrentIndex(InOvrSession, TextureSet, &CurrentIndex);

	InitWithCurrentElement(CurrentIndex);
}

void FD3D11Texture2DSet::InitWithCurrentElement(int CurrentIndex)
{
	check(TextureSet);

	Resource = Textures[CurrentIndex].Texture;
	ShaderResourceView = Textures[CurrentIndex].SRV;

	RenderTargetViews.Empty(Textures[CurrentIndex].RTVs.Num());
	for (int32 i = 0; i < Textures[CurrentIndex].RTVs.Num(); ++i)
	{
		RenderTargetViews.Add(Textures[CurrentIndex].RTVs[i]);
	}
}

void FD3D11Texture2DSet::ReleaseResources(ovrSession InOvrSession)
{
	if (TextureSet)
	{
		UE_LOG(LogHMD, Log, TEXT("Freeing textureSet 0x%p"), TextureSet);
		ovr_DestroyTextureSwapChain(InOvrSession, TextureSet);
		TextureSet = nullptr;
	}
	Textures.Empty(0);
}

FD3D11Texture2DSet* FD3D11Texture2DSet::D3D11CreateTexture2DSet(
	FD3D11DynamicRHI* InD3D11RHI,
	const FOvrSessionSharedPtr& InOvrSession,
	ovrTextureSwapChain InTextureSet,
	const D3D11_TEXTURE2D_DESC& InDsDesc,
	EPixelFormat InFormat,
	uint32 InFlags
	)
{
	FOvrSessionShared::AutoSession OvrSession(InOvrSession);
	check(InTextureSet);

	TArray<TRefCountPtr<ID3D11RenderTargetView> > TextureSetRenderTargetViews;
	FD3D11Texture2DSet* NewTextureSet = new FD3D11Texture2DSet(
		InD3D11RHI,
		nullptr,
		nullptr,
		false,
		1,
		TextureSetRenderTargetViews,
		/*DepthStencilViews=*/ NULL,
		InDsDesc.Width,
		InDsDesc.Height,
		0,
		InDsDesc.MipLevels,
		InDsDesc.SampleDesc.Count,
		InFormat,
		/*bInCubemap=*/ false,
		InFlags,
		/*bPooledTexture=*/ false
		);

	int TexCount;
	ovr_GetTextureSwapChainLength(OvrSession, InTextureSet, &TexCount);
	const bool bSRGB = (InFlags & TexCreate_SRGB) != 0;

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[InFormat].PlatformFormat;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	D3D11_RTV_DIMENSION RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	if (InDsDesc.SampleDesc.Count > 1)
	{
		RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	}
	for (int32 i = 0; i < TexCount; ++i)
	{
		TRefCountPtr<ID3D11Texture2D> pD3DTexture;
		ovrResult res = ovr_GetTextureSwapChainBufferDX(OvrSession, InTextureSet, i, IID_PPV_ARGS(pD3DTexture.GetInitReference()));
		if (!OVR_SUCCESS(res))
		{
			UE_LOG(LogHMD, Error, TEXT("ovr_GetTextureSwapChainBufferDX failed, error = %d"), int(res));
			return nullptr;
		}

		TArray<TRefCountPtr<ID3D11RenderTargetView> > RenderTargetViews;
		if (InFlags & TexCreate_RenderTargetable)
		{
			// Create a render target view for each mip
			for (uint32 MipIndex = 0; MipIndex < InDsDesc.MipLevels; MipIndex++)
			{
				check(!(InFlags & TexCreate_TargetArraySlicesIndependently)); // not supported
				D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
				FMemory::Memzero(&RTVDesc, sizeof(RTVDesc));
				RTVDesc.Format = PlatformRenderTargetFormat;
				RTVDesc.ViewDimension = RenderTargetViewDimension;
				RTVDesc.Texture2D.MipSlice = MipIndex;

				TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
				VERIFYD3D11RESULT_EX(InD3D11RHI->GetDevice()->CreateRenderTargetView(pD3DTexture, &RTVDesc, RenderTargetView.GetInitReference()), InD3D11RHI->GetDevice());
				RenderTargetViews.Add(RenderTargetView);
			}
		}

		TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;

		// Create a shader resource view for the texture.
		if (InFlags & TexCreate_ShaderResource)
		{
			D3D11_SRV_DIMENSION ShaderResourceViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			SRVDesc.Format = PlatformShaderResourceFormat;

			SRVDesc.ViewDimension = ShaderResourceViewDimension;
			SRVDesc.Texture2D.MostDetailedMip = 0;
			SRVDesc.Texture2D.MipLevels = InDsDesc.MipLevels;

			VERIFYD3D11RESULT_EX(InD3D11RHI->GetDevice()->CreateShaderResourceView(pD3DTexture, &SRVDesc, ShaderResourceView.GetInitReference()), InD3D11RHI->GetDevice());

			check(IsValidRef(ShaderResourceView));
		}

		NewTextureSet->AddTexture(pD3DTexture, ShaderResourceView, &RenderTargetViews);
	}

	if (InFlags & TexCreate_RenderTargetable)
	{
		NewTextureSet->SetCurrentGPUAccess(EResourceTransitionAccess::EWritable);
	}
	NewTextureSet->TextureSet = InTextureSet;
	NewTextureSet->InitWithCurrentElement(0);
	return NewTextureSet;
}

static FD3D11Texture2D* D3D11CreateTexture2DAlias(
	FD3D11DynamicRHI* InD3D11RHI,
	ID3D11Texture2D* InResource,
	ID3D11ShaderResourceView* InShaderResourceView,
	uint32 InSizeX,
	uint32 InSizeY,
	uint32 InSizeZ,
	uint32 InNumMips,
	uint32 InNumSamples,
	EPixelFormat InFormat,
	uint32 InFlags)
{
	const bool bSRGB = (InFlags & TexCreate_SRGB) != 0;

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[InFormat].PlatformFormat;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	D3D11_RTV_DIMENSION RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	if (InNumSamples > 1)
	{
		RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	}

	TArray<TRefCountPtr<ID3D11RenderTargetView> > RenderTargetViews;

	if (InFlags & TexCreate_RenderTargetable)
	{
		// Create a render target view for each mip
		for (uint32 MipIndex = 0; MipIndex < InNumMips; MipIndex++)
		{
			check(!(InFlags & TexCreate_TargetArraySlicesIndependently)); // not supported
			D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
			FMemory::Memzero(&RTVDesc, sizeof(RTVDesc));
			RTVDesc.Format = PlatformRenderTargetFormat;
			RTVDesc.ViewDimension = RenderTargetViewDimension;
			RTVDesc.Texture2D.MipSlice = MipIndex;

			TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
			VERIFYD3D11RESULT_EX(InD3D11RHI->GetDevice()->CreateRenderTargetView(InResource, &RTVDesc, RenderTargetView.GetInitReference()), InD3D11RHI->GetDevice());
			RenderTargetViews.Add(RenderTargetView);
		}
	}

	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;

	// Create a shader resource view for the texture.
	if (!InShaderResourceView && (InFlags & TexCreate_ShaderResource))
	{
		D3D11_SRV_DIMENSION ShaderResourceViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		SRVDesc.Format = PlatformShaderResourceFormat;

		SRVDesc.ViewDimension = ShaderResourceViewDimension;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = InNumMips;

		VERIFYD3D11RESULT_EX(InD3D11RHI->GetDevice()->CreateShaderResourceView(InResource, &SRVDesc, ShaderResourceView.GetInitReference()), InD3D11RHI->GetDevice());

		check(IsValidRef(ShaderResourceView));
	}
	else
	{
		ShaderResourceView = InShaderResourceView;
	}

	FD3D11Texture2D* NewTexture = new FD3D11Texture2D(
		InD3D11RHI,
		InResource,
		ShaderResourceView,
		false,
		1,
		RenderTargetViews,
		/*DepthStencilViews=*/ NULL,
		InSizeX,
		InSizeY,
		InSizeZ,
		InNumMips,
		InNumSamples,
		InFormat,
		/*bInCubemap=*/ false,
		InFlags,
		/*bPooledTexture=*/ false,
		FClearValueBinding::None
		);

	if (InFlags & TexCreate_RenderTargetable)
	{
		NewTexture->SetCurrentGPUAccess(EResourceTransitionAccess::EWritable);
	}

	return NewTexture;
}


//-------------------------------------------------------------------------------------------------
// FOculusRiftPlugin
//-------------------------------------------------------------------------------------------------

void FOculusRiftPlugin::DisableSLI()
{
	// Disable SLI by default
	NvAPI_D3D_ImplicitSLIControl(DISABLE_IMPLICIT_SLI);
}

void FOculusRiftPlugin::SetGraphicsAdapter(const ovrGraphicsLuid& luid)
{
	TRefCountPtr<IDXGIFactory> DXGIFactory;

	if(SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**) DXGIFactory.GetInitReference())))
	{
		for(int32 adapterIndex = 0;; adapterIndex++)
		{
			TRefCountPtr<IDXGIAdapter> DXGIAdapter;
			DXGI_ADAPTER_DESC DXGIAdapterDesc;

			if( FAILED(DXGIFactory->EnumAdapters(adapterIndex, DXGIAdapter.GetInitReference())) ||
				FAILED(DXGIAdapter->GetDesc(&DXGIAdapterDesc)) )
			{
				break;
			}

			if(!FMemory::Memcmp(&luid, &DXGIAdapterDesc.AdapterLuid, sizeof(LUID)))
			{
				// Remember this adapterIndex so we use the right adapter, even when we startup without HMD connected
				GConfig->SetInt(TEXT("Oculus.Settings"), TEXT("GraphicsAdapter"), adapterIndex, GEngineIni);
				break;
			}
		}
	}
}


//-------------------------------------------------------------------------------------------------
// FOculusRiftHMD::D3D11Bridge
//-------------------------------------------------------------------------------------------------

FOculusRiftHMD::D3D11Bridge::D3D11Bridge(const FOvrSessionSharedPtr& InOvrSession)
	: FCustomPresent(InOvrSession)
{
}

bool FOculusRiftHMD::D3D11Bridge::IsUsingGraphicsAdapter(const ovrGraphicsLuid& luid)
{
	TRefCountPtr<ID3D11Device> D3D11Device;

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		GetNativeDevice,
		TRefCountPtr<ID3D11Device>&, D3D11DeviceRef, D3D11Device,
		{
			D3D11DeviceRef = (ID3D11Device*) RHIGetNativeDevice();
		});

	FlushRenderingCommands();

	if(D3D11Device)
	{
		TRefCountPtr<IDXGIDevice> DXGIDevice;
		TRefCountPtr<IDXGIAdapter> DXGIAdapter;
		DXGI_ADAPTER_DESC DXGIAdapterDesc;

		if( SUCCEEDED(D3D11Device->QueryInterface(__uuidof(IDXGIDevice), (void**) DXGIDevice.GetInitReference())) &&
			SUCCEEDED(DXGIDevice->GetAdapter(DXGIAdapter.GetInitReference())) &&
			SUCCEEDED(DXGIAdapter->GetDesc(&DXGIAdapterDesc)) )
		{
			return !FMemory::Memcmp(&luid, &DXGIAdapterDesc.AdapterLuid, sizeof(LUID));
		}
	}

	// Not enough information.  Assume that we are using the correct adapter.
	return true;
}

void FOculusRiftHMD::D3D11Bridge::BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT)
{
	SCOPE_CYCLE_COUNTER(STAT_BeginRendering);

	check(IsInRenderingThread());

	SetRenderContext(&InRenderContext);

	FGameFrame* CurrentRenderFrame = GetRenderFrame();
	check(CurrentRenderFrame);
	FSettings* FrameSettings = CurrentRenderFrame->GetSettings();
	check(FrameSettings);

	const uint32 RTSizeX = RT->GetSizeX();
	const uint32 RTSizeY = RT->GetSizeY();

	FOvrSessionShared::AutoSession OvrSession(Session);
	const FVector2D ActualMirrorWindowSize = CurrentRenderFrame->WindowSize;
	// detect if mirror texture needs to be re-allocated or freed
	if (Session->IsActive() && MirrorTextureRHI && (bNeedReAllocateMirrorTexture || 
		(FrameSettings->Flags.bMirrorToWindow && (
		FrameSettings->MirrorWindowMode != FSettings::eMirrorWindow_Distorted ||
		ActualMirrorWindowSize != FVector2D(MirrorTextureRHI->GetSizeX(), MirrorTextureRHI->GetSizeY()))) ||
		!FrameSettings->Flags.bMirrorToWindow ))
	{
		check(MirrorTexture);
		ovr_DestroyMirrorTexture(OvrSession, MirrorTexture);
		MirrorTexture = nullptr;
		MirrorTextureRHI = nullptr;
		bNeedReAllocateMirrorTexture = false;
	}

	// need to allocate a mirror texture?
	if (FrameSettings->Flags.bMirrorToWindow && FrameSettings->MirrorWindowMode == FSettings::eMirrorWindow_Distorted && !MirrorTextureRHI &&
		ActualMirrorWindowSize.X != 0 && ActualMirrorWindowSize.Y != 0)
	{
		ovrMirrorTextureDesc desc {};
		// Override the format to be sRGB so that the compositor always treats eye buffers
		// as if they're sRGB even if we are sending in linear format textures
		desc.Format = OVR_FORMAT_B8G8R8A8_UNORM_SRGB;
		desc.Width = (int)ActualMirrorWindowSize.X;
		desc.Height = (int)ActualMirrorWindowSize.Y;
		desc.MiscFlags = ovrTextureMisc_DX_Typeless;

		ID3D11Device* D3DDevice = (ID3D11Device*)RHIGetNativeDevice();

		ovrResult res = ovr_CreateMirrorTextureDX(OvrSession, D3DDevice, &desc, &MirrorTexture);
		if (!MirrorTexture || !OVR_SUCCESS(res))
		{
			UE_LOG(LogHMD, Error, TEXT("Can't create a mirror texture, error = %d"), res);
			return;
		}
		bReady = true;

		UE_LOG(LogHMD, Log, TEXT("Allocated a new mirror texture (size %d x %d)"), desc.Width, desc.Height);

		TRefCountPtr<ID3D11Texture2D> pD3DTexture;
		res = ovr_GetMirrorTextureBufferDX(OvrSession, MirrorTexture, IID_PPV_ARGS(pD3DTexture.GetInitReference()));
		if (!OVR_SUCCESS(res))
		{
			UE_LOG(LogHMD, Error, TEXT("ovr_GetMirrorTextureBufferDX failed, error = %d"), int(res));
			return;
		}

		MirrorTextureRHI = D3D11CreateTexture2DAlias(
			static_cast<FD3D11DynamicRHI*>(GDynamicRHI),
			pD3DTexture,
			nullptr,
			desc.Width,
			desc.Height,
			0,
			1,
			/*ActualMSAACount=*/ 1,
			(EPixelFormat)PF_B8G8R8A8,
			TexCreate_ShaderResource);
		bNeedReAllocateMirrorTexture = false;
	}
}

FTexture2DSetProxyPtr FOculusRiftHMD::D3D11Bridge::CreateTextureSet(const uint32 InSizeX, const uint32 InSizeY, const EPixelFormat InSrcFormat, const uint32 InNumMips, uint32 InCreateTexFlags)
{
	auto D3D11RHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
	ID3D11Device* D3DDevice = D3D11RHI->GetDevice();
	
	const EPixelFormat Format = PF_B8G8R8A8;
	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;

	const uint32 TexCreate_Flags = (((InCreateTexFlags & ShaderResource) ? TexCreate_ShaderResource : 0)|
									((InCreateTexFlags & RenderTargetable) ? TexCreate_RenderTargetable : 0));

	ovrTextureSwapChainDesc desc {};
	desc.Type = ovrTexture_2D;
	desc.ArraySize = 1;
	desc.MipLevels = (InNumMips == 0) ? GetNumMipLevels(InSizeX, InSizeY, InCreateTexFlags) : InNumMips;
	check(desc.MipLevels > 0);
	desc.SampleCount = 1;
	desc.StaticImage = (InCreateTexFlags & StaticImage) ? ovrTrue : ovrFalse;
	desc.Width = InSizeX;
	desc.Height = InSizeY;
	// Override the format to be sRGB so that the compositor always treats eye buffers
	// as if they're sRGB even if we are sending in linear formatted textures
	desc.Format = (InCreateTexFlags & LinearSpace) ? OVR_FORMAT_B8G8R8A8_UNORM : OVR_FORMAT_B8G8R8A8_UNORM_SRGB;
	desc.MiscFlags = ovrTextureMisc_DX_Typeless;

	// just make sure the proper format is used; if format is different then we might
	// need to make some changes here.
	check(PlatformResourceFormat == DXGI_FORMAT_B8G8R8A8_TYPELESS);

	desc.BindFlags = ovrTextureBind_DX_RenderTarget;
	if (desc.MipLevels != 1)
	{
		desc.MiscFlags |= ovrTextureMisc_AllowGenerateMips;
	}

	FOvrSessionShared::AutoSession OvrSession(Session);
	ovrTextureSwapChain textureSet;
	ovrResult res = ovr_CreateTextureSwapChainDX(OvrSession, D3DDevice, &desc, &textureSet);

	if (!textureSet || !OVR_SUCCESS(res))
	{
		UE_LOG(LogHMD, Error, TEXT("Can't create swap texture set (size %d x %d), error = %d"), desc.Width, desc.Height, res);
		if (res == ovrError_DisplayLost)
		{
			bNeedReAllocateMirrorTexture = bNeedReAllocateTextureSet = true;
			FPlatformAtomics::InterlockedExchange(&NeedToKillHmd, 1);
		}
		return false;
	}
	bReady = true;

	// set the proper format for RTV & SRV
	D3D11_TEXTURE2D_DESC dsDesc;
	dsDesc.Width = desc.Width;
	dsDesc.Height = desc.Height;
	dsDesc.MipLevels = desc.MipLevels;
	dsDesc.ArraySize = desc.ArraySize;

	dsDesc.SampleDesc.Count = desc.SampleCount;
	dsDesc.SampleDesc.Quality = 0;
	dsDesc.Usage = D3D11_USAGE_DEFAULT;
	dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	dsDesc.CPUAccessFlags = 0;
	dsDesc.MiscFlags = 0;

	dsDesc.Format = PlatformResourceFormat; //DXGI_FORMAT_B8G8R8A8_UNORM;

	UE_LOG(LogHMD, Log, TEXT("Allocated a new swap texture set (size %d x %d, mipcount = %d), 0x%p"), desc.Width, desc.Height, desc.MipLevels, textureSet);

	TRefCountPtr<FD3D11Texture2DSet> ColorTextureSet = FD3D11Texture2DSet::D3D11CreateTexture2DSet(
		D3D11RHI,
		Session,
		textureSet,
		dsDesc,
		Format,
		TexCreate_Flags
		);
	if (ColorTextureSet)
	{
		return MakeShareable(new FD3D11Texture2DSetProxy(Session, ColorTextureSet->GetTexture2D(), InSizeX, InSizeY, InSrcFormat, InNumMips));
	}
	return nullptr;
}

#if PLATFORM_WINDOWS
	// It is required to undef WINDOWS_PLATFORM_TYPES_GUARD for any further D3D11 / GL private includes
	#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // #if defined(OVR_D3D)

#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
