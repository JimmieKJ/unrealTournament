// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "HMDPrivatePCH.h"
#include "OculusRiftHMD.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)

#include "D3D11RHIPrivate.h"
#include "D3D11Util.h"

#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "AllowWindowsPlatformTypes.h"
#endif
#include "OVR_CAPI_D3D.h"

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
	void SwitchToNextElement();
	void AddTexture(ID3D11Texture2D*, ID3D11ShaderResourceView*, TArray<TRefCountPtr<ID3D11RenderTargetView> >* = nullptr);

	ovrSwapTextureSet* GetSwapTextureSet() const { return TextureSet; }

	static FD3D11Texture2DSet* D3D11CreateTexture2DSet(
		FD3D11DynamicRHI* InD3D11RHI,
		ovrSwapTextureSet* InTextureSet,
		const D3D11_TEXTURE2D_DESC& InDsDesc,
		EPixelFormat InFormat,
		uint32 InFlags
		);
protected:
	void InitWithCurrentElement();

	struct TextureElement
	{
		TRefCountPtr<ID3D11Texture2D> Texture;
		TRefCountPtr<ID3D11ShaderResourceView> SRV;
		TArray<TRefCountPtr<ID3D11RenderTargetView> > RTVs;
	};
	TArray<TextureElement> Textures;

	ovrSwapTextureSet* TextureSet;
};

class FD3D11Texture2DSetProxy : public FTexture2DSetProxy
{
public:
	FD3D11Texture2DSetProxy(ovrSession InOvrSession, FTextureRHIRef InTexture, uint32 SrcSizeX, uint32 SrcSizeY, EPixelFormat SrcFormat)
		: FTexture2DSetProxy(InOvrSession, InTexture, SrcSizeX, SrcSizeY, SrcFormat) {}

	virtual ovrSwapTextureSet* GetSwapTextureSet() const override
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
			D3D11TS->SwitchToNextElement();
		}
	}

	virtual void ReleaseResources() override
	{
		if (RHITexture.IsValid())
		{
			auto D3D11TS = static_cast<FD3D11Texture2DSet*>(RHITexture->GetTexture2D());
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

void FD3D11Texture2DSet::SwitchToNextElement()
{
	check(TextureSet);
	check(TextureSet->TextureCount == Textures.Num());

	TextureSet->CurrentIndex = (TextureSet->CurrentIndex + 1) % TextureSet->TextureCount;
	InitWithCurrentElement();
}

void FD3D11Texture2DSet::InitWithCurrentElement()
{
	check(TextureSet);
	check(TextureSet->TextureCount == Textures.Num());

	Resource = Textures[TextureSet->CurrentIndex].Texture;
	ShaderResourceView = Textures[TextureSet->CurrentIndex].SRV;

	RenderTargetViews.Empty(Textures[TextureSet->CurrentIndex].RTVs.Num());
	for (int32 i = 0; i < Textures[TextureSet->CurrentIndex].RTVs.Num(); ++i)
	{
		RenderTargetViews.Add(Textures[TextureSet->CurrentIndex].RTVs[i]);
	}
}

void FD3D11Texture2DSet::ReleaseResources(ovrSession InOvrSession)
{
	if (TextureSet)
	{
		UE_LOG(LogHMD, Log, TEXT("Freeing textureSet 0x%p"), TextureSet);
		ovr_DestroySwapTextureSet(InOvrSession, TextureSet);
		TextureSet = nullptr;
	}
	Textures.Empty(0);
}

FD3D11Texture2DSet* FD3D11Texture2DSet::D3D11CreateTexture2DSet(
	FD3D11DynamicRHI* InD3D11RHI,
	ovrSwapTextureSet* InTextureSet,
	const D3D11_TEXTURE2D_DESC& InDsDesc,
	EPixelFormat InFormat,
	uint32 InFlags
	)
{
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

	const uint32 TexCount = InTextureSet->TextureCount;
	const bool bSRGB = (InFlags & TexCreate_SRGB) != 0;

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[InFormat].PlatformFormat;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	D3D11_RTV_DIMENSION RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	if (InDsDesc.SampleDesc.Count > 1)
	{
		RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	}
	for (uint32 i = 0; i < TexCount; ++i)
	{
		ovrD3D11Texture D3DTex;
		D3DTex.Texture = InTextureSet->Textures[i];

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
				VERIFYD3D11RESULT(InD3D11RHI->GetDevice()->CreateRenderTargetView(D3DTex.D3D11.pTexture, &RTVDesc, RenderTargetView.GetInitReference()));
				RenderTargetViews.Add(RenderTargetView);
			}
		}

		TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView = D3DTex.D3D11.pSRView;

		// Create a shader resource view for the texture.
		if (!ShaderResourceView && (InFlags & TexCreate_ShaderResource))
		{
			D3D11_SRV_DIMENSION ShaderResourceViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			SRVDesc.Format = PlatformShaderResourceFormat;

			SRVDesc.ViewDimension = ShaderResourceViewDimension;
			SRVDesc.Texture2D.MostDetailedMip = 0;
			SRVDesc.Texture2D.MipLevels = InDsDesc.MipLevels;

			VERIFYD3D11RESULT(InD3D11RHI->GetDevice()->CreateShaderResourceView(D3DTex.D3D11.pTexture, &SRVDesc, ShaderResourceView.GetInitReference()));

			check(IsValidRef(ShaderResourceView));
		}

		NewTextureSet->AddTexture(D3DTex.D3D11.pTexture, ShaderResourceView, &RenderTargetViews);
	}

	if (InFlags & TexCreate_RenderTargetable)
	{
		//NewTextureSet->SetCurrentGPUAccess(EResourceTransitionAccess::EWritable);
	}
	NewTextureSet->TextureSet = InTextureSet;
	NewTextureSet->InitWithCurrentElement();
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
			VERIFYD3D11RESULT(InD3D11RHI->GetDevice()->CreateRenderTargetView(InResource, &RTVDesc, RenderTargetView.GetInitReference()));
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

		VERIFYD3D11RESULT(InD3D11RHI->GetDevice()->CreateShaderResourceView(InResource, &SRVDesc, ShaderResourceView.GetInitReference()));

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
		//NewTexture->SetCurrentGPUAccess(EResourceTransitionAccess::EWritable);
	}

	return NewTexture;
}


//-------------------------------------------------------------------------------------------------
// FOculusRiftHMD
//-------------------------------------------------------------------------------------------------

#pragma comment(lib, "dxgi")

void FOculusRiftHMD::PreInit()
{
	// Find the adapterIndex where the HMD is connected
	ovrHmd hmd;
	ovrGraphicsLuid luid;

	if(OVR_SUCCESS(ovr_Create(&hmd, &luid)))
	{
		SetHmdGraphicsAdapter(luid);
		ovr_Destroy(hmd);
	}
}

void FOculusRiftHMD::SetHmdGraphicsAdapter(const ovrGraphicsLuid& luid)
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
				IConsoleVariable* CVarHmdGraphicsAdapter = IConsoleManager::Get().FindConsoleVariable(L"r.HmdGraphicsAdapter");

				if(CVarHmdGraphicsAdapter)
				{
					if(adapterIndex != CVarHmdGraphicsAdapter->GetInt())
						CVarHmdGraphicsAdapter->Set(adapterIndex);
				}

				break;
			}
		}
	}
}

bool FOculusRiftHMD::IsRHIUsingHmdGraphicsAdapter(const ovrGraphicsLuid& luid)
{
	if (IsPCPlatform(GMaxRHIShaderPlatform) && !IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		TRefCountPtr<ID3D11Device> Device;

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			GetNativeDevice,
			TRefCountPtr<ID3D11Device>&, DeviceRef, Device,
			{
				DeviceRef = (ID3D11Device*) RHIGetNativeDevice();
			});

		FlushRenderingCommands();

		if(Device)
		{
			TRefCountPtr<IDXGIDevice> DXGIDevice;
			TRefCountPtr<IDXGIAdapter> DXGIAdapter;
			DXGI_ADAPTER_DESC DXGIAdapterDesc;

			if( SUCCEEDED(Device->QueryInterface(__uuidof(IDXGIDevice), (void**) DXGIDevice.GetInitReference())) &&
				SUCCEEDED(DXGIDevice->GetAdapter(DXGIAdapter.GetInitReference())) &&
				SUCCEEDED(DXGIAdapter->GetDesc(&DXGIAdapterDesc)) )
			{
				return !FMemory::Memcmp(&luid, &DXGIAdapterDesc.AdapterLuid, sizeof(LUID));
			}
		}
	}

	// Not enough information.  Assume that we are using the correct adapter.
	return true;
}


//-------------------------------------------------------------------------------------------------
// FOculusRiftHMD::D3D11Bridge
//-------------------------------------------------------------------------------------------------

FOculusRiftHMD::D3D11Bridge::D3D11Bridge(ovrSession InOvrSession)
	: FCustomPresent()
{
	Init(InOvrSession);
}

void FOculusRiftHMD::D3D11Bridge::SetHmd(ovrSession InOvrSession)
{
	if (InOvrSession != OvrSession)
	{
		Reset();
		Init(InOvrSession);
		bNeedReAllocateTextureSet = true;
		bNeedReAllocateMirrorTexture = true;
	}
}

void FOculusRiftHMD::D3D11Bridge::Init(ovrSession InOvrSession)
{
	OvrSession = InOvrSession;
	bInitialized = true;
}

void FOculusRiftHMD::D3D11Bridge::BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT)
{
	SCOPE_CYCLE_COUNTER(STAT_BeginRendering);

	check(IsInRenderingThread());

	SetRenderContext(&InRenderContext);

	FGameFrame* CurrentFrame = GetRenderFrame();
	check(CurrentFrame);
	FSettings* FrameSettings = CurrentFrame->GetSettings();
	check(FrameSettings);

	const uint32 RTSizeX = RT->GetSizeX();
	const uint32 RTSizeY = RT->GetSizeY();

	const FVector2D ActualMirrorWindowSize = CurrentFrame->WindowSize;
	// detect if mirror texture needs to be re-allocated or freed
	if (OvrSession && MirrorTextureRHI && (bNeedReAllocateMirrorTexture || OvrSession != RenderContext->OvrSession ||
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
		D3D11_TEXTURE2D_DESC dsDesc;
		dsDesc.Width = (UINT)ActualMirrorWindowSize.X;
		dsDesc.Height = (UINT)ActualMirrorWindowSize.Y;
		dsDesc.MipLevels = 1;
		dsDesc.ArraySize = 1;
		dsDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // SRGB is required for the compositor
		dsDesc.SampleDesc.Count = 1;
		dsDesc.SampleDesc.Quality = 0;
		dsDesc.Usage = D3D11_USAGE_DEFAULT;
		dsDesc.BindFlags = 0;// D3D11_BIND_SHADER_RESOURCE; //can't even use D3DMirrorTexture.D3D11.pSRView since we need one w/o SRGB set
		dsDesc.CPUAccessFlags = 0;
		dsDesc.MiscFlags = 0;

		ID3D11Device* D3DDevice = (ID3D11Device*)RHIGetNativeDevice();

		ovrResult res = ovr_CreateMirrorTextureD3D11(OvrSession, D3DDevice, &dsDesc, ovrSwapTextureSetD3D11_Typeless, &MirrorTexture);
		if (!MirrorTexture || !OVR_SUCCESS(res))
		{
			UE_LOG(LogHMD, Error, TEXT("Can't create a mirror texture, error = %d"), res);
			return;
		}

		UE_LOG(LogHMD, Log, TEXT("Allocated a new mirror texture (size %d x %d)"), (int)ActualMirrorWindowSize.X, (int)ActualMirrorWindowSize.Y);
		ovrD3D11Texture D3DMirrorTexture;
		D3DMirrorTexture.Texture = *MirrorTexture;
		MirrorTextureRHI = D3D11CreateTexture2DAlias(
			static_cast<FD3D11DynamicRHI*>(GDynamicRHI),
			D3DMirrorTexture.D3D11.pTexture,
			nullptr,// can't use D3DMirrorTexture.D3D11.pSRView since we need one w/o SRGB set
			dsDesc.Width,
			dsDesc.Height,
			0,
			dsDesc.MipLevels,
			/*ActualMSAACount=*/ 1,
			(EPixelFormat)PF_B8G8R8A8,
			TexCreate_ShaderResource);
		bNeedReAllocateMirrorTexture = false;
	}
}

void FOculusRiftHMD::D3D11Bridge::Reset_RenderThread()
{
	if (MirrorTexture)
	{
		ovr_DestroyMirrorTexture(OvrSession, MirrorTexture);
		MirrorTextureRHI = nullptr;
		MirrorTexture = nullptr;
	}
	LayerMgr.ReleaseRenderLayers_RenderThread();
	OvrSession = nullptr;

	if (RenderContext.IsValid())
	{
		RenderContext->bFrameBegun = false;
		SetRenderContext(nullptr);
	}
}

void FOculusRiftHMD::D3D11Bridge::Reset()
{
	if (IsInGameThread())
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ResetD3D,
		FOculusRiftHMD::D3D11Bridge*, Bridge, this,
		{
			Bridge->Reset_RenderThread();
		});
		// Wait for all resources to be released
		FlushRenderingCommands();
	}
	else
	{
		Reset_RenderThread();
	}

	bInitialized = false;
}

FTexture2DSetProxyRef FOculusRiftHMD::D3D11Bridge::CreateTextureSet(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, uint32 InNumMips, uint32 InFlags)
{
	auto D3D11RHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
	ID3D11Device* D3DDevice = D3D11RHI->GetDevice();

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[InFormat].PlatformFormat;

	D3D11_TEXTURE2D_DESC dsDesc;
	dsDesc.Width = InSizeX;
	dsDesc.Height = InSizeY;
	dsDesc.MipLevels = InNumMips;
	dsDesc.ArraySize = 1;

	// just make sure the proper format is used; if format is different then we might
	// need to make some changes here.
	check(PlatformResourceFormat == DXGI_FORMAT_B8G8R8A8_TYPELESS);

	dsDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // use SRGB for compositor
	dsDesc.SampleDesc.Count = 1;
	dsDesc.SampleDesc.Quality = 0;
	dsDesc.Usage = D3D11_USAGE_DEFAULT;
	dsDesc.BindFlags = ((InFlags & TexCreate_ShaderResource) ? D3D11_BIND_SHADER_RESOURCE : 0) | 
					   ((InFlags & TexCreate_RenderTargetable) ? D3D11_BIND_RENDER_TARGET : 0);
	dsDesc.CPUAccessFlags = 0;
	dsDesc.MiscFlags = 0;

	ovrSwapTextureSet* textureSet;
	ovrResult res = ovr_CreateSwapTextureSetD3D11(OvrSession, D3DDevice, &dsDesc, ovrSwapTextureSetD3D11_Typeless, &textureSet);
	if (!textureSet || !OVR_SUCCESS(res))
	{
		UE_LOG(LogHMD, Error, TEXT("Can't create swap texture set (size %d x %d), error = %d"), InSizeX, InSizeY, res);
		return false;
	}

	// set the proper format for RTV & SRV
	dsDesc.Format = PlatformResourceFormat; //DXGI_FORMAT_B8G8R8A8_UNORM;

	UE_LOG(LogHMD, Log, TEXT("Allocated a new swap texture set (size %d x %d), 0x%p"), InSizeX, InSizeY, textureSet);

	TRefCountPtr<FD3D11Texture2DSet> ColorTextureSet = FD3D11Texture2DSet::D3D11CreateTexture2DSet(
		D3D11RHI,
		textureSet,
		dsDesc,
		EPixelFormat(InFormat),
		InFlags
		);
	if (ColorTextureSet)
	{
		return MakeShareable(new FD3D11Texture2DSetProxy(OvrSession, ColorTextureSet->GetTexture2D(), InSizeX, InSizeY, InFormat));
	}
	return nullptr;
}

#if PLATFORM_WINDOWS
	// It is required to undef WINDOWS_PLATFORM_TYPES_GUARD for any further D3D11 / GL private includes
	#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // #if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)

#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
