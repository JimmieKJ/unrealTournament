// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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
		bInPooled
		)
	{
		TextureSet = nullptr;
	}

	void ReleaseResources(ovrHmd Hmd);
	void SwitchToNextElement();
	void AddTexture(ID3D11Texture2D*, ID3D11ShaderResourceView*, TArray<TRefCountPtr<ID3D11RenderTargetView> >* = nullptr);

	ovrSwapTextureSet* GetTextureSet() const { return TextureSet; }

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

void FD3D11Texture2DSet::ReleaseResources(ovrHmd Hmd)
{
	if (TextureSet)
	{
		ovrHmd_DestroySwapTextureSet(Hmd, TextureSet);
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

	TArray<TRefCountPtr<ID3D11RenderTargetView> > RenderTargetViews;
	FD3D11Texture2DSet* NewTextureSet = new FD3D11Texture2DSet(
		InD3D11RHI,
		nullptr,
		nullptr,
		false,
		1,
		RenderTargetViews,
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
		/*bPooledTexture=*/ false
		);

	return NewTexture;
}

//////////////////////////////////////////////////////////////////////////
FOculusRiftHMD::D3D11Bridge::D3D11Bridge(ovrHmd Hmd)
	: FCustomPresent()
{
	MirrorTexture = nullptr;
	Init(Hmd);
}

void FOculusRiftHMD::D3D11Bridge::SetHmd(ovrHmd InHmd)
{
	if (InHmd != Hmd)
	{
		Reset();
		Init(InHmd);
		bNeedReAllocateTextureSet = true;
		bNeedReAllocateMirrorTexture = true;
	}
}

void FOculusRiftHMD::D3D11Bridge::Init(ovrHmd InHmd)
{
	Hmd = InHmd;
	bInitialized = true;
}

//FTexture2DRHIRef FOculusRiftHMD::D3D11Bridge::AllocateRenderTargetTexture(int32 TexWidth, int32 TexHeight, EPixelFormat Format)

bool FOculusRiftHMD::D3D11Bridge::AllocateRenderTargetTexture(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(SizeX != 0 && SizeY != 0);

	if (!ColorTextureSet || (ColorTextureSet->GetSizeX() != SizeX || ColorTextureSet->GetSizeY() != SizeY || ColorTextureSet->GetFormat() != Format))
	{
		bNeedReAllocateTextureSet = true;
	}

	if (Hmd && bNeedReAllocateTextureSet)
	{
		auto D3D11RHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
		if (ColorTextureSet)
		{
			ColorTextureSet->ReleaseResources(Hmd);
			ColorTextureSet = nullptr;
		}
		ID3D11Device* D3DDevice = D3D11RHI->GetDevice();

		const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;

		D3D11_TEXTURE2D_DESC dsDesc;
		dsDesc.Width = SizeX;
		dsDesc.Height = SizeY;
		dsDesc.MipLevels = 1;
		dsDesc.ArraySize = 1;
		dsDesc.Format = PlatformResourceFormat; //DXGI_FORMAT_B8G8R8A8_UNORM;
		dsDesc.SampleDesc.Count = 1;
		dsDesc.SampleDesc.Quality = 0;
		dsDesc.Usage = D3D11_USAGE_DEFAULT;
		dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		dsDesc.CPUAccessFlags = 0;
		dsDesc.MiscFlags = 0;

		ovrSwapTextureSet* textureSet;
		ovrHmd_CreateSwapTextureSetD3D11(Hmd, D3DDevice, &dsDesc, &textureSet);
		if (!textureSet)
		{
			UE_LOG(LogHMD, Error, TEXT("Can't create swap texture set (size %d x %d)"), SizeX, SizeY);
			return false;
		}
		bNeedReAllocateTextureSet = false;
		bNeedReAllocateMirrorTexture = true;
		UE_LOG(LogHMD, Log, TEXT("Allocated a new swap texture set (size %d x %d)"), SizeX, SizeY);

		ColorTextureSet = FD3D11Texture2DSet::D3D11CreateTexture2DSet(
			D3D11RHI,
			textureSet,
			dsDesc,
			EPixelFormat(Format),
			TexCreate_RenderTargetable | TexCreate_ShaderResource
			);
	}
	OutTargetableTexture = ColorTextureSet->GetTexture2D();
	OutShaderResourceTexture = ColorTextureSet->GetTexture2D();
	return true;
}

void FOculusRiftHMD::D3D11Bridge::BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT)
{
	check(IsInRenderingThread());

	SetRenderContext(&InRenderContext);

	FGameFrame* CurrentFrame = GetRenderFrame();
	check(CurrentFrame);
	FSettings* FrameSettings = CurrentFrame->GetSettings();
	check(FrameSettings);

	const uint32 RTSizeX = RT->GetSizeX();
	const uint32 RTSizeY = RT->GetSizeY();

	FIntPoint ActualMirrorWindowSize(0, 0);
	if (RenderContext->Hmd)
	{
		ActualMirrorWindowSize.X = (FrameSettings->MirrorWindowSize.X == 0) ? RenderContext->Hmd->Resolution.w : FrameSettings->MirrorWindowSize.X;
		ActualMirrorWindowSize.Y = (FrameSettings->MirrorWindowSize.Y == 0) ? RenderContext->Hmd->Resolution.h : FrameSettings->MirrorWindowSize.Y;
	}
	// detect if mirror texture needs to be re-allocated or freed
	if (Hmd && MirrorTextureRHI && (bNeedReAllocateMirrorTexture || Hmd != RenderContext->Hmd ||
		(FrameSettings->Flags.bMirrorToWindow && (
		FrameSettings->MirrorWindowMode != FSettings::eMirrorWindow_Distorted ||
		ActualMirrorWindowSize != FIntPoint(MirrorTextureRHI->GetSizeX(), MirrorTextureRHI->GetSizeY()))) || 
		!FrameSettings->Flags.bMirrorToWindow ))
	{
		check(MirrorTexture);
		ovrHmd_DestroyMirrorTexture(Hmd, MirrorTexture);
		MirrorTexture = nullptr;
		MirrorTextureRHI = nullptr;
		bNeedReAllocateMirrorTexture = false;
	}

	// need to allocate a mirror texture?
	if (FrameSettings->Flags.bMirrorToWindow && FrameSettings->MirrorWindowMode == FSettings::eMirrorWindow_Distorted && !MirrorTextureRHI &&
		ActualMirrorWindowSize.X != 0 && ActualMirrorWindowSize.Y != 0)
	{
		D3D11_TEXTURE2D_DESC dsDesc;
		dsDesc.Width = ActualMirrorWindowSize.X;
		dsDesc.Height = ActualMirrorWindowSize.Y;
		dsDesc.MipLevels = 1;
		dsDesc.ArraySize = 1;
		dsDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		dsDesc.SampleDesc.Count = 1;
		dsDesc.SampleDesc.Quality = 0;
		dsDesc.Usage = D3D11_USAGE_DEFAULT;
		dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		dsDesc.CPUAccessFlags = 0;
		dsDesc.MiscFlags = 0;

		ID3D11Device* D3DDevice = (ID3D11Device*)RHIGetNativeDevice();

		ovrHmd_CreateMirrorTextureD3D11(Hmd, D3DDevice, &dsDesc, &MirrorTexture);
		if (!MirrorTexture)
		{
			UE_LOG(LogHMD, Error, TEXT("Can't create a mirror texture"));
			return;
		}
		UE_LOG(LogHMD, Log, TEXT("Allocated a new mirror texture (size %d x %d)"), ActualMirrorWindowSize.X, ActualMirrorWindowSize.Y);
		ovrD3D11Texture D3DMirrorTexture;
		D3DMirrorTexture.Texture = *MirrorTexture;
		MirrorTextureRHI = D3D11CreateTexture2DAlias(
			static_cast<FD3D11DynamicRHI*>(GDynamicRHI),
			D3DMirrorTexture.D3D11.pTexture,
			D3DMirrorTexture.D3D11.pSRView,
			dsDesc.Width,
			dsDesc.Height,
			0,
			dsDesc.MipLevels,
			/*ActualMSAACount=*/ 1,
			(EPixelFormat)PF_B8G8R8A8,
			TexCreate_RenderTargetable);
		bNeedReAllocateMirrorTexture = false;
	}
}

void FOculusRiftHMD::D3D11Bridge::FinishRendering()
{
	check(IsInRenderingThread());
	
	check(RenderContext.IsValid());

	if (RenderContext->bFrameBegun && ColorTextureSet)
	{
		if (!ColorTextureSet)
		{
			UE_LOG(LogHMD, Warning, TEXT("Skipping frame: TextureSet is null ?"));
		}
		else
		{
			// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
			FSettings* FrameSettings = RenderContext->GetFrameSettings();

			check(ColorTextureSet->GetTextureSet());
			FrameSettings->EyeLayer.EyeFov.ColorTexture[0] = ColorTextureSet->GetTextureSet();
			FrameSettings->EyeLayer.EyeFov.ColorTexture[1] = ColorTextureSet->GetTextureSet();

			ovrLayerHeader* LayerList[1];
			LayerList[0] = &FrameSettings->EyeLayer.EyeFov.Header;

			// Set up positional data.
			ovrViewScaleDesc viewScaleDesc;
			viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;
			viewScaleDesc.HmdToEyeViewOffset[0] = FrameSettings->EyeRenderDesc[0].HmdToEyeViewOffset;
			viewScaleDesc.HmdToEyeViewOffset[1] = FrameSettings->EyeRenderDesc[1].HmdToEyeViewOffset;

			ovrHmd_SubmitFrame(RenderContext->Hmd, RenderContext->RenderFrame->FrameNumber, &viewScaleDesc, LayerList, 1);

			ColorTextureSet->SwitchToNextElement();
		}
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("Skipping frame: FinishRendering called with no corresponding BeginRendering (was BackBuffer re-allocated?)"));
	}
	RenderContext->bFrameBegun = false;
	SetRenderContext(nullptr);
}

void FOculusRiftHMD::D3D11Bridge::Reset_RenderThread()
{
	if (MirrorTexture)
	{
		ovrHmd_DestroyMirrorTexture(Hmd, MirrorTexture);
		MirrorTextureRHI = nullptr;
		MirrorTexture = nullptr;
	}
	if (ColorTextureSet)
	{
		ColorTextureSet->ReleaseResources(Hmd);
		ColorTextureSet = nullptr;
	}
	Hmd = nullptr;

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

void FOculusRiftHMD::D3D11Bridge::OnBackBufferResize()
{
	// if we are in the middle of rendering: prevent from calling EndFrame
	if (RenderContext.IsValid())
	{
		RenderContext->bFrameBegun = false;
	}
}

bool FOculusRiftHMD::D3D11Bridge::Present(int32& SyncInterval)
{
	check(IsInRenderingThread());

	if (!RenderContext.IsValid())
	{
		return true; // use regular Present; this frame is not ready yet
	}

	SyncInterval = 0; // turn off VSync for the 'normal Present'.
	bool bHostPresent = RenderContext->GetFrameSettings()->Flags.bMirrorToWindow;

	FinishRendering();
	return bHostPresent;
}

#endif // #if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)

#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS