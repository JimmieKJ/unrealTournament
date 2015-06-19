// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHI.cpp: Metal device RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "IOSAppDelegate.h"

/** Set to 1 to enable GPU events in Xcode frame debugger */
#define ENABLE_METAL_GPUEVENTS	(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)

DEFINE_LOG_CATEGORY(LogMetal)

DEFINE_STAT(STAT_MetalMakeDrawableTime);
DEFINE_STAT(STAT_MetalDrawCallTime);
DEFINE_STAT(STAT_MetalPrepareDrawTime);
DEFINE_STAT(STAT_MetalUniformBufferCleanupTime);
DEFINE_STAT(STAT_MetalFreeUniformBufferMemory);
DEFINE_STAT(STAT_MetalNumFreeUniformBuffers);
DEFINE_STAT(STAT_MetalPipelineStateTime);
DEFINE_STAT(STAT_MetalBoundShaderStateTime);
DEFINE_STAT(STAT_MetalVertexDeclarationTime);


bool FMetalDynamicRHIModule::IsSupported()
{
	return true;
}

FDynamicRHI* FMetalDynamicRHIModule::CreateRHI()
{
	return new FMetalDynamicRHI();
}

IMPLEMENT_MODULE(FMetalDynamicRHIModule, MetalRHI);

FMetalDynamicRHI::FMetalDynamicRHI()
{
	// This should be called once at the start 
	check( IsInGameThread() );
	check( !GIsThreadedRendering );


	// get the device to ask about capabilities
	id<MTLDevice> Device = [IOSAppDelegate GetDelegate].IOSView->MetalDevice;
	// A8 can use 256 bits of MRTs
	bool bCanUseWideMRTs = [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1];
	bool bCanUseASTC = [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1] && !FParse::Param(FCommandLine::Get(),TEXT("noastc"));

	bool bProjectSupportsMRTs = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bProjectSupportsMRTs, GEngineIni);

	// only allow GBuffers, etc on A8s (A7s are just not going to cut it)
	if (bProjectSupportsMRTs && bCanUseWideMRTs && FParse::Param(FCommandLine::Get(),TEXT("metalmrt")))
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM4;
		GMaxRHIShaderPlatform = SP_METAL_MRT;
	}
	else
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::ES3_1;
		GMaxRHIShaderPlatform = SP_METAL;
	}

	if (FPlatformMisc::IsDebuggerPresent() && UE_BUILD_DEBUG)
	{
		// Enable GL debug markers if we're running in Xcode
		extern int32 GEmitMeshDrawEvent;
		GEmitMeshDrawEvent = 1;
		GEmitDrawEvents = true;
	}

	GSupportsShaderFramebufferFetch = true;
	GHardwareHiddenSurfaceRemoval = true;
//	GRHIAdapterName = 
//	GRHIVendorId = 
	GSupportsRenderTargetFormat_PF_G8 = false;
	GSupportsQuads = false;
	GRHISupportsTextureStreaming = true;
	GMaxShadowDepthBufferSizeX = 4096;
	GMaxShadowDepthBufferSizeY = 4096;
// 	GReadTexturePoolSizeFromIni = true;

	GRHISupportsBaseVertexIndex = false;
	GRHIRequiresEarlyBackBufferRenderTarget = false;

	GMaxTextureDimensions = 4096;
	GMaxTextureMipCount = FPlatformMath::CeilLogTwo( GMaxTextureDimensions ) + 1;
	GMaxTextureMipCount = FPlatformMath::Min<int32>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );
	GMaxCubeTextureDimensions = 4096;
	GMaxTextureArrayLayers = 2048;

	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_METAL;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = (GMaxRHIShaderPlatform == SP_METAL_MRT) ? SP_METAL_MRT : SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_NumPlatforms;

	// we cannot render to a volume texture without geometry shader support
	GSupportsVolumeTextureRendering = false;

	//@todo-rco: Query name from API
	GRHIAdapterName = TEXT("Metal");
	GRHIVendorId = 1; // non-zero to avoid asserts

	// Initialize the platform pixel format map.
	GPixelFormats[PF_Unknown			].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_A32B32G32R32F		].PlatformFormat	= MTLPixelFormatRGBA32Float;
	GPixelFormats[PF_B8G8R8A8			].PlatformFormat	= MTLPixelFormatBGRA8Unorm;
	GPixelFormats[PF_G8					].PlatformFormat	= MTLPixelFormatR8Unorm;
	GPixelFormats[PF_G16				].PlatformFormat	= MTLPixelFormatR16Unorm;
	GPixelFormats[PF_DXT1				].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_DXT3				].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_DXT5				].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_PVRTC2				].PlatformFormat	= MTLPixelFormatPVRTC_RGBA_2BPP;
	GPixelFormats[PF_PVRTC2				].Supported			= true;
	GPixelFormats[PF_PVRTC4				].PlatformFormat	= MTLPixelFormatPVRTC_RGBA_4BPP;
	GPixelFormats[PF_PVRTC4				].Supported			= true;
	GPixelFormats[PF_PVRTC4				].PlatformFormat	= MTLPixelFormatPVRTC_RGBA_4BPP;
	GPixelFormats[PF_PVRTC4				].Supported			= true;
	GPixelFormats[PF_ASTC_4x4			].PlatformFormat	= MTLPixelFormatASTC_4x4_LDR;
	GPixelFormats[PF_ASTC_4x4			].Supported			= bCanUseASTC;
	GPixelFormats[PF_ASTC_6x6			].PlatformFormat	= MTLPixelFormatASTC_6x6_LDR;
	GPixelFormats[PF_ASTC_6x6			].Supported			= bCanUseASTC;
	GPixelFormats[PF_ASTC_8x8			].PlatformFormat	= MTLPixelFormatASTC_8x8_LDR;
	GPixelFormats[PF_ASTC_8x8			].Supported			= bCanUseASTC;
	GPixelFormats[PF_ASTC_10x10			].PlatformFormat	= MTLPixelFormatASTC_10x10_LDR;
	GPixelFormats[PF_ASTC_10x10			].Supported			= bCanUseASTC;
	GPixelFormats[PF_ASTC_12x12			].PlatformFormat	= MTLPixelFormatASTC_12x12_LDR;
	GPixelFormats[PF_ASTC_12x12			].Supported			= bCanUseASTC;
	GPixelFormats[PF_UYVY				].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_FloatRGB			].PlatformFormat	= MTLPixelFormatRGBA16Float;
	GPixelFormats[PF_FloatRGB			].BlockBytes		= 8;
	GPixelFormats[PF_FloatRGBA			].PlatformFormat	= MTLPixelFormatRGBA16Float;
	GPixelFormats[PF_FloatRGBA			].BlockBytes		= 8;
	GPixelFormats[PF_DepthStencil		].PlatformFormat	= MTLPixelFormatDepth32Float;
	GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
	GPixelFormats[PF_X24_G8				].PlatformFormat	= MTLPixelFormatStencil8;
	GPixelFormats[PF_X24_G8				].BlockBytes		= 1;
	GPixelFormats[PF_ShadowDepth		].PlatformFormat	= GPixelFormats[PF_DepthStencil].PlatformFormat; // all depth formats must be the same, for the pipeline state hash (see NUMBITS_DEPTH_ENABLED)
	GPixelFormats[PF_R32_FLOAT			].PlatformFormat	= MTLPixelFormatR32Float;
	GPixelFormats[PF_G16R16				].PlatformFormat	= MTLPixelFormatInvalid;//MTLPixelFormatRG16Unorm;
	// we can't render to this in Metal, so mark it as unsupported (although we could texture from it, but we are only using it for render targets)
	GPixelFormats[PF_G16R16				].Supported			= false;
	GPixelFormats[PF_G16R16F			].PlatformFormat	= MTLPixelFormatRG16Float;
	GPixelFormats[PF_G16R16F_FILTER		].PlatformFormat	= MTLPixelFormatRG16Float;
	GPixelFormats[PF_G32R32F			].PlatformFormat	= MTLPixelFormatRG32Float;
	GPixelFormats[PF_A2B10G10R10		].PlatformFormat    = MTLPixelFormatRGB10A2Unorm;
	GPixelFormats[PF_A16B16G16R16		].PlatformFormat    = MTLPixelFormatRGBA16Unorm;
	GPixelFormats[PF_D24				].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_R16F				].PlatformFormat	= MTLPixelFormatR16Float;
	GPixelFormats[PF_R16F_FILTER		].PlatformFormat	= MTLPixelFormatR16Float;
	GPixelFormats[PF_BC5				].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_V8U8				].PlatformFormat	= 
	GPixelFormats[PF_A1					].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= MTLPixelFormatRG11B10Float;
	GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 4;
	GPixelFormats[PF_A8					].PlatformFormat	= MTLPixelFormatA8Unorm;
	GPixelFormats[PF_R32_UINT			].PlatformFormat	= MTLPixelFormatR32Uint;
	GPixelFormats[PF_R32_SINT			].PlatformFormat	= MTLPixelFormatR32Sint;
	GPixelFormats[PF_R16G16B16A16_UINT	].PlatformFormat	= MTLPixelFormatRGBA16Uint;
	GPixelFormats[PF_R16G16B16A16_SINT	].PlatformFormat	= MTLPixelFormatRGBA16Sint;
	GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= MTLPixelFormatB5G6R5Unorm;
	GPixelFormats[PF_R8G8B8A8			].PlatformFormat	= MTLPixelFormatRGBA8Unorm;
	GPixelFormats[PF_R8G8				].PlatformFormat	= MTLPixelFormatRG8Unorm;

	GDynamicRHI = this;
	GIsRHIInitialized = true;

	// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitDynamicRHI();
	}
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitRHI();
	}
}

FMetalDynamicRHI::~FMetalDynamicRHI()
{
	check(IsInGameThread() && IsInRenderingThread());

}

uint64 FMetalDynamicRHI::RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{
	OutAlign = 0;
	return CalcTextureSize(SizeX, SizeY, (EPixelFormat)Format, NumMips);
}

uint64 FMetalDynamicRHI::RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	OutAlign = 0;
	return CalcTextureSize3D(SizeX, SizeY, SizeZ, (EPixelFormat)Format, NumMips);
}

uint64 FMetalDynamicRHI::RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags,	uint32& OutAlign)
{
	OutAlign = 0;
	return CalcTextureSize(Size, Size, (EPixelFormat)Format, NumMips) * 6;
}


void FMetalDynamicRHI::Init()
{

}

void FMetalDynamicRHI::RHIBeginFrame()
{
}

void FMetalDynamicRHI::RHIEndFrame()
{
}

void FMetalDynamicRHI::RHIBeginScene()
{
	FMetalManager::Get()->BeginScene();
}

void FMetalDynamicRHI::RHIEndScene()
{
	FMetalManager::Get()->EndScene();
}

void FMetalDynamicRHI::RHIPushEvent(const TCHAR* Name)
{
#if ENABLE_METAL_GPUEVENTS
	[FMetalManager::GetContext() pushDebugGroup: [NSString stringWithTCHARString:Name]];
#endif
}

void FMetalDynamicRHI::RHIPopEvent()
{
#if ENABLE_METAL_GPUEVENTS
	[FMetalManager::GetContext() popDebugGroup];
#endif
}

void FMetalDynamicRHI::RHIGetSupportedResolution( uint32 &Width, uint32 &Height )
{

}

bool FMetalDynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{

	return false;
}

void FMetalDynamicRHI::RHIFlushResources()
{

}

void FMetalDynamicRHI::RHIAcquireThreadOwnership()
{
	FMetalManager::Get()->CreateAutoreleasePool();
}

void FMetalDynamicRHI::RHIReleaseThreadOwnership()
{
	FMetalManager::Get()->DrainAutoreleasePool();
}

void FMetalDynamicRHI::RHIGpuTimeBegin(uint32 Hash, bool bCompute)
{

}

void FMetalDynamicRHI::RHIGpuTimeEnd(uint32 Hash, bool bCompute)
{

}

void* FMetalDynamicRHI::RHIGetNativeDevice()
{
	return NULL;
}
