// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHI.cpp: Metal device RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#if PLATFORM_IOS
#include "IOSAppDelegate.h"
#elif PLATFORM_MAC
#include "MacApplication.h"
#endif
#include "ShaderCache.h"
#include "MetalProfiler.h"
#include "GenericPlatformDriver.h"

/** Set to 1 to enable GPU events in Xcode frame debugger */
#define ENABLE_METAL_GPUEVENTS	(UE_BUILD_DEBUG | UE_BUILD_DEVELOPMENT)
#define ENABLE_METAL_GPUPROFILE	(ENABLE_METAL_GPUEVENTS & 1)

DEFINE_LOG_CATEGORY(LogMetal)

bool FMetalDynamicRHIModule::IsSupported()
{
	return true;
}

FDynamicRHI* FMetalDynamicRHIModule::CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel)
{
	return new FMetalDynamicRHI();
}

IMPLEMENT_MODULE(FMetalDynamicRHIModule, MetalRHI);

FMetalDynamicRHI::FMetalDynamicRHI()
: FMetalRHICommandContext(nullptr, FMetalDeviceContext::CreateDeviceContext())
, AsyncComputeContext(nullptr)
{
	// This should be called once at the start 
	check( IsInGameThread() );
	check( !GIsThreadedRendering );
	
	// @todo Zebra This is now supported on all GPUs in Mac Metal, but not on iOS.
	// we cannot render to a volume texture without geometry shader support
	GSupportsVolumeTextureRendering = false;

	//@todo-rco: Query name from API
	GRHIAdapterName = TEXT("Metal");
	GRHIVendorId = 1; // non-zero to avoid asserts
	
#if PLATFORM_IOS
	// get the device to ask about capabilities
	id<MTLDevice> Device = [IOSAppDelegate GetDelegate].IOSView->MetalDevice;
	// A8 can use 256 bits of MRTs
#if PLATFORM_TVOS
	bool bCanUseWideMRTs = true;
	bool bCanUseASTC = true;
#else
	bool bCanUseWideMRTs = [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1];
	bool bCanUseASTC = [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1] && !FParse::Param(FCommandLine::Get(),TEXT("noastc"));
#endif

    bool bProjectSupportsMRTs = false;
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bProjectSupportsMRTs, GEngineIni);
    
    // only allow GBuffers, etc on A8s (A7s are just not going to cut it)
    if (bProjectSupportsMRTs && bCanUseWideMRTs && FParse::Param(FCommandLine::Get(),TEXT("metalmrt")))
    {
        GMaxRHIFeatureLevel = ERHIFeatureLevel::SM4;
        GMaxRHIShaderPlatform = SP_METAL_MRT;
		GSupportsMultipleRenderTargets = true;
    }
    else
    {
        GMaxRHIFeatureLevel = ERHIFeatureLevel::ES3_1;
        GMaxRHIShaderPlatform = SP_METAL;
		// disable MRTs completely, because there is code in the engine that assumes the ability to use
		// 32 bytes of pixel storage (like the 2 VERY WIDE ones in GPU particles)
		GSupportsMultipleRenderTargets = false;
	}
	
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_METAL;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_METAL;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4) ? GMaxRHIShaderPlatform : SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_NumPlatforms;

#else // @todo zebra
    // get the device to ask about capabilities?
    id<MTLDevice> Device = Context->GetDevice();
	uint32 DeviceIndex = ((FMetalDeviceContext*)Context)->GetDeviceIndex();
	
	TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = FPlatformMisc::GetGPUDescriptors();
	check(DeviceIndex < GPUs.Num());
	FMacPlatformMisc::FGPUDescriptor const& GPUDesc = GPUs[DeviceIndex];
	
    // A8 can use 256 bits of MRTs
    bool bCanUseWideMRTs = true;
    bool bCanUseASTC = false;
	bool bSupportsD24S8 = false;
    bool bSupportsD16 = false;

	// Default to Metal SM5 on 10.11.5 or later. Earlier versions should still be running SM4.
	bool const bTenElevenFiveOrLater = (FPlatformMisc::MacOSXVersionCompare(10,11,5) >= 0);
	if(!FParse::Param(FCommandLine::Get(),TEXT("metal")) && (bTenElevenFiveOrLater || FParse::Param(FCommandLine::Get(),TEXT("metalsm5"))))
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
		GMaxRHIShaderPlatform = SP_METAL_SM5;
	}
	else
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM4;
		GMaxRHIShaderPlatform = SP_METAL_SM4;
	}
	
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_METAL_MACES2;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_METAL_MACES3_1;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = SP_METAL_SM4;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5) ? GMaxRHIShaderPlatform : SP_NumPlatforms;
	
	GRHIAdapterName = FString(Device.name);
	
	// Mac GPUs support layer indexing.
	GSupportsVolumeTextureRendering = true;
	
	// However they don't all support other features depending on the version of the OS.
	bool bSupportsPointLights = false;
	bool bSupportsTiledReflections = false;
	bool bSupportsDistanceFields = false;
	if(GRHIAdapterName.Contains("Nvidia"))
	{
		bSupportsPointLights = true;
		GRHIVendorId = 0x10DE;
		bSupportsTiledReflections = true;
		bSupportsDistanceFields = (FPlatformMisc::MacOSXVersionCompare(10,11,4) >= 0);
	}
	else if(GRHIAdapterName.Contains("ATi") || GRHIAdapterName.Contains("AMD"))
	{
		bSupportsPointLights = true;
		GRHIVendorId = 0x1002;
		if(GPUDesc.GPUVendorId == GRHIVendorId)
		{
			GRHIAdapterName = FString(GPUDesc.GPUName);
		}
		bSupportsTiledReflections = true;
		bSupportsDistanceFields = (FPlatformMisc::MacOSXVersionCompare(10,11,4) >= 0);
	}
	else if(GRHIAdapterName.Contains("Intel"))
	{
		bSupportsPointLights = (FPlatformMisc::MacOSXVersionCompare(10,11,4) >= 0);
		GRHIVendorId = 0x8086;
	}
	
	// Make sure the vendors match - the assumption that order in IORegistry is the order in Metal may not hold up forever.
	if(GPUDesc.GPUVendorId == GRHIVendorId)
	{
		GRHIDeviceId = GPUDesc.GPUDeviceId;
		MemoryStats.DedicatedVideoMemory = GPUDesc.GPUMemoryMB * 1024 * 1024;
		MemoryStats.TotalGraphicsMemory = GPUDesc.GPUMemoryMB * 1024 * 1024;
		MemoryStats.DedicatedSystemMemory = 0;
		MemoryStats.SharedSystemMemory = 0;
	}
	
	GPoolSizeVRAMPercentage = 0;
	GTexturePoolSize = 0;
	GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSizeVRAMPercentage"), GPoolSizeVRAMPercentage, GEngineIni);
	if ( GPoolSizeVRAMPercentage > 0 && MemoryStats.TotalGraphicsMemory > 0 )
	{
		float PoolSize = float(GPoolSizeVRAMPercentage) * 0.01f * float(MemoryStats.TotalGraphicsMemory);
		
		// Truncate GTexturePoolSize to MB (but still counted in bytes)
		GTexturePoolSize = int64(FGenericPlatformMath::TruncToFloat(PoolSize / 1024.0f / 1024.0f)) * 1024 * 1024;
		
		UE_LOG(LogRHI,Log,TEXT("Texture pool is %llu MB (%d%% of %llu MB)"),
						GTexturePoolSize / 1024 / 1024,
						GPoolSizeVRAMPercentage,
						MemoryStats.TotalGraphicsMemory);
	}
	
	// Change the support depth format if we can
	bSupportsD24S8 = Device.depth24Stencil8PixelFormatSupported;
	
	// Force disable vertex-shader-layer point light rendering on GPUs that don't support it properly yet.
	if(!bSupportsPointLights && !FParse::Param(FCommandLine::Get(),TEXT("metalpointlights")))
	{
		// Disable point light cubemap shadows on Mac Metal as currently they aren't supported.
		static auto CVarCubemapShadows = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowPointLightCubemapShadows"));
		if(CVarCubemapShadows && CVarCubemapShadows->GetInt() != 0)
		{
			CVarCubemapShadows->Set(0);
		}
	}
	
	// Disable tiled reflections on Mac Metal for some GPU drivers that ignore the lod-level and so render incorrectly.
	if (!bSupportsTiledReflections && !FParse::Param(FCommandLine::Get(),TEXT("metaltiledreflections")))
	{
		static auto CVarDoTiledReflections = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DoTiledReflections"));
		if(CVarDoTiledReflections && CVarDoTiledReflections->GetInt() != 0)
		{
			CVarDoTiledReflections->Set(0);
		}
	}
	
	// Disable the distance field AO & shadowing effects on GPU drivers that don't currently execute the shaders correctly.
	if (GMaxRHIShaderPlatform == SP_METAL_SM5 && !bSupportsDistanceFields && !FParse::Param(FCommandLine::Get(),TEXT("metaldistancefields")))
	{
		static auto CVarDistanceFieldAO = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFieldAO"));
		if(CVarDistanceFieldAO && CVarDistanceFieldAO->GetInt() != 0)
		{
			CVarDistanceFieldAO->Set(0);
		}
		
		static auto CVarDistanceFieldShadowing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFieldShadowing"));
		if(CVarDistanceFieldShadowing && CVarDistanceFieldShadowing->GetInt() != 0)
		{
			CVarDistanceFieldShadowing->Set(0);
		}
	}
	
	GRHISupportsRHIThread = false;
	if (GMaxRHIShaderPlatform == SP_METAL_SM5)
	{
#if METAL_SUPPORTS_PARALLEL_RHI_EXECUTE
#if WITH_EDITORONLY_DATA
		GRHISupportsRHIThread = !GIsEditor;
#else
		GRHISupportsRHIThread = true;
#endif
		GRHISupportsParallelRHIExecute = GRHISupportsRHIThread;
#endif
		GSupportsEfficientAsyncCompute = GRHISupportsParallelRHIExecute && IsRHIDeviceAMD(); // Only AMD currently support async. compute and it requires parallel execution to be useful.
		GSupportsParallelOcclusionQueries = GRHISupportsRHIThread;
	}
	else
	{
		GRHISupportsParallelRHIExecute = false;
		GSupportsEfficientAsyncCompute = false;
		GSupportsParallelOcclusionQueries = false;
	}
	
#endif

	if (FPlatformMisc::IsDebuggerPresent() && UE_BUILD_DEBUG)
	{
#if PLATFORM_IOS // @todo zebra : needs a RENDER_API or whatever
		// Enable GL debug markers if we're running in Xcode
		extern int32 GEmitMeshDrawEvent;
		GEmitMeshDrawEvent = 1;
#endif
		GEmitDrawEvents = true;
	}
	
	GEmitDrawEvents |= ENABLE_METAL_GPUEVENTS;

	GSupportsShaderFramebufferFetch = !PLATFORM_MAC;
	GHardwareHiddenSurfaceRemoval = true;
	GSupportsRenderTargetFormat_PF_G8 = false;
	GSupportsQuads = false;
	GRHISupportsTextureStreaming = true;
	GSupportsWideMRT = bCanUseWideMRTs;

	GRHIRequiresEarlyBackBufferRenderTarget = false;
	GSupportsSeparateRenderTargetBlendState = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4);

#if PLATFORM_MAC
	check([Device supportsFeatureSet:MTLFeatureSet_OSX_GPUFamily1_v1]);
	GRHISupportsBaseVertexIndex = FPlatformMisc::MacOSXVersionCompare(10,11,2) >= 0 || !IsRHIDeviceAMD(); // Supported on macOS & iOS but not tvOS - broken on AMD prior to 10.11.2
	GRHISupportsFirstInstance = true; // Supported on macOS & iOS but not tvOS.
	GMaxTextureDimensions = 16384;
	GMaxCubeTextureDimensions = 16384;
	GMaxTextureArrayLayers = 2048;
	GMaxShadowDepthBufferSizeX = 16384;
	GMaxShadowDepthBufferSizeY = 16384;
    bSupportsD16 = FParse::Param(FCommandLine::Get(),TEXT("metalv2")) || [Device supportsFeatureSet:MTLFeatureSet_OSX_GPUFamily1_v2];
#else
#if PLATFORM_TVOS
	GRHISupportsBaseVertexIndex = false;
	GRHISupportsFirstInstance = false; // Supported on macOS & iOS but not tvOS.
#else
	GRHISupportsBaseVertexIndex = [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1];
	GRHISupportsFirstInstance = GRHISupportsBaseVertexIndex;
#endif
	GMaxTextureDimensions = 4096;
	GMaxCubeTextureDimensions = 4096;
	GMaxTextureArrayLayers = 2048;
	GMaxShadowDepthBufferSizeX = 4096;
	GMaxShadowDepthBufferSizeY = 4096;
#endif
	
	GMaxTextureMipCount = FPlatformMath::CeilLogTwo( GMaxTextureDimensions ) + 1;
	GMaxTextureMipCount = FPlatformMath::Min<int32>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );

	// Initialize the platform pixel format map.
	GPixelFormats[PF_Unknown			].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_A32B32G32R32F		].PlatformFormat	= MTLPixelFormatRGBA32Float;
	GPixelFormats[PF_B8G8R8A8			].PlatformFormat	= MTLPixelFormatBGRA8Unorm;
	GPixelFormats[PF_G8					].PlatformFormat	= MTLPixelFormatR8Unorm;
	GPixelFormats[PF_G16				].PlatformFormat	= MTLPixelFormatR16Unorm;
#if PLATFORM_IOS
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
#else // @todo zebra : srgb?
    GPixelFormats[PF_DXT1				].PlatformFormat	= MTLPixelFormatBC1_RGBA;
    GPixelFormats[PF_DXT3				].PlatformFormat	= MTLPixelFormatBC2_RGBA;
    GPixelFormats[PF_DXT5				].PlatformFormat	= MTLPixelFormatBC3_RGBA;
#endif
	GPixelFormats[PF_UYVY				].PlatformFormat	= MTLPixelFormatInvalid;
#if PLATFORM_IOS
	GPixelFormats[PF_FloatRGB			].PlatformFormat	= MTLPixelFormatRGBA16Float;
	GPixelFormats[PF_FloatRGB			].BlockBytes		= 8;
#else
	GPixelFormats[PF_FloatRGB			].PlatformFormat	= MTLPixelFormatRG11B10Float;
	GPixelFormats[PF_FloatRGB			].BlockBytes		= 4;
#endif
	GPixelFormats[PF_FloatRGBA			].PlatformFormat	= MTLPixelFormatRGBA16Float;
	GPixelFormats[PF_FloatRGBA			].BlockBytes		= 8;
#if PLATFORM_IOS
	GPixelFormats[PF_DepthStencil		].PlatformFormat	= MTLPixelFormatDepth32Float;
	GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
    GPixelFormats[PF_ShadowDepth		].PlatformFormat	= GPixelFormats[PF_DepthStencil].PlatformFormat; // all depth formats must be the same, for the pipeline state hash (see NUMBITS_DEPTH_ENABLED)
#else
	// Use Depth28_Stencil8 when it is available for consistency
	if(bSupportsD24S8)
	{
		GPixelFormats[PF_DepthStencil	].PlatformFormat	= MTLPixelFormatDepth24Unorm_Stencil8;
	}
	else
	{
		GPixelFormats[PF_DepthStencil	].PlatformFormat	= MTLPixelFormatDepth32Float_Stencil8;
	}
    GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
    if (bSupportsD16)
    {
        GPixelFormats[PF_ShadowDepth		].PlatformFormat	= MTLPixelFormatDepth16Unorm;
        GPixelFormats[PF_ShadowDepth		].BlockBytes		= 2;
    }
    else
    {
        GPixelFormats[PF_ShadowDepth		].PlatformFormat	= MTLPixelFormatDepth32Float;
        GPixelFormats[PF_ShadowDepth		].BlockBytes		= 4;
    }
#endif
    GPixelFormats[PF_X24_G8				].PlatformFormat	= MTLPixelFormatStencil8;
    GPixelFormats[PF_X24_G8				].BlockBytes		= 1;
	GPixelFormats[PF_R32_FLOAT			].PlatformFormat	= MTLPixelFormatR32Float;
#if PLATFORM_IOS
	GPixelFormats[PF_G16R16				].PlatformFormat	= MTLPixelFormatInvalid;//MTLPixelFormatRG16Unorm;
	// we can't render to this in Metal, so mark it as unsupported (although we could texture from it, but we are only using it for render targets)
	GPixelFormats[PF_G16R16				].Supported			= false;
#else
	GPixelFormats[PF_G16R16				].PlatformFormat	= MTLPixelFormatRG16Unorm;
	GPixelFormats[PF_G16R16				].Supported			= true;
#endif
	GPixelFormats[PF_G16R16F			].PlatformFormat	= MTLPixelFormatRG16Float;
	GPixelFormats[PF_G16R16F_FILTER		].PlatformFormat	= MTLPixelFormatRG16Float;
	GPixelFormats[PF_G32R32F			].PlatformFormat	= MTLPixelFormatRG32Float;
	GPixelFormats[PF_A2B10G10R10		].PlatformFormat    = MTLPixelFormatRGB10A2Unorm;
	GPixelFormats[PF_A16B16G16R16		].PlatformFormat    = MTLPixelFormatRGBA16Unorm;
#if PLATFORM_MAC
	if(bSupportsD24S8)
	{
		GPixelFormats[PF_D24			].PlatformFormat	= MTLPixelFormatDepth24Unorm_Stencil8;
	}
	else
	{
		GPixelFormats[PF_D24			].PlatformFormat	= MTLPixelFormatDepth32Float;
	}
	GPixelFormats[PF_D24				].Supported			= true;
#endif
	GPixelFormats[PF_R16F				].PlatformFormat	= MTLPixelFormatR16Float;
	GPixelFormats[PF_R16F_FILTER		].PlatformFormat	= MTLPixelFormatR16Float;
#if PLATFORM_IOS
	GPixelFormats[PF_BC5				].PlatformFormat	= MTLPixelFormatInvalid;
#else
    GPixelFormats[PF_BC4				].Supported			= true;
    GPixelFormats[PF_BC4				].PlatformFormat	= MTLPixelFormatBC4_RUnorm;
    GPixelFormats[PF_BC5				].Supported			= true;
    GPixelFormats[PF_BC5				].PlatformFormat	= MTLPixelFormatBC5_RGUnorm;
    GPixelFormats[PF_BC6H				].Supported			= true;
    GPixelFormats[PF_BC6H               ].PlatformFormat	= MTLPixelFormatBC6H_RGBUfloat;
    GPixelFormats[PF_BC7				].Supported			= true;
    GPixelFormats[PF_BC7				].PlatformFormat	= MTLPixelFormatBC7_RGBAUnorm;
#endif
	GPixelFormats[PF_V8U8				].PlatformFormat	=
	GPixelFormats[PF_A1					].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= MTLPixelFormatRG11B10Float;
	GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 4;
	GPixelFormats[PF_A8					].PlatformFormat	= MTLPixelFormatA8Unorm;
	GPixelFormats[PF_R32_UINT			].PlatformFormat	= MTLPixelFormatR32Uint;
	GPixelFormats[PF_R32_SINT			].PlatformFormat	= MTLPixelFormatR32Sint;
	GPixelFormats[PF_R16G16B16A16_UINT	].PlatformFormat	= MTLPixelFormatRGBA16Uint;
	GPixelFormats[PF_R16G16B16A16_SINT	].PlatformFormat	= MTLPixelFormatRGBA16Sint;
#if PLATFORM_IOS
	GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= MTLPixelFormatB5G6R5Unorm;
#else // @todo zebra
    GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= MTLPixelFormatInvalid;
#endif
	GPixelFormats[PF_R8G8B8A8			].PlatformFormat	= MTLPixelFormatRGBA8Unorm;
	GPixelFormats[PF_R8G8				].PlatformFormat	= MTLPixelFormatRG8Unorm;
	GPixelFormats[PF_R16_SINT			].PlatformFormat	= MTLPixelFormatR16Sint;
	GPixelFormats[PF_R16_UINT			].PlatformFormat	= MTLPixelFormatR16Uint;
	GPixelFormats[PF_R8_UINT			].PlatformFormat	= MTLPixelFormatR8Uint;

	// get driver version (todo: share with other RHIs)
	{
		FGPUDriverInfo GPUDriverInfo = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);
		
		GRHIAdapterUserDriverVersion = GPUDriverInfo.UserDriverVersion;
		GRHIAdapterInternalDriverVersion = GPUDriverInfo.InternalDriverVersion;
		GRHIAdapterDriverDate = GPUDriverInfo.DriverDate;
		
		UE_LOG(LogMetal, Display, TEXT("    Adapter Name: %s"), *GRHIAdapterName);
		UE_LOG(LogMetal, Display, TEXT("  Driver Version: %s (internal:%s, unified:%s)"), *GRHIAdapterUserDriverVersion, *GRHIAdapterInternalDriverVersion, *GPUDriverInfo.GetUnifiedDriverVersion());
		UE_LOG(LogMetal, Display, TEXT("     Driver Date: %s"), *GRHIAdapterDriverDate);
		UE_LOG(LogMetal, Display, TEXT("          Vendor: %s"), *GPUDriverInfo.ProviderName);
#if PLATFORM_MAC
		if(GPUDesc.GPUVendorId == GRHIVendorId)
		{
			UE_LOG(LogMetal, Display,  TEXT("      Vendor ID: %d"), GPUDesc.GPUVendorId);
			UE_LOG(LogMetal, Display,  TEXT("      Device ID: %d"), GPUDesc.GPUDeviceId);
			UE_LOG(LogMetal, Display,  TEXT("      VRAM (MB): %d"), GPUDesc.GPUMemoryMB);
		}
		else
		{
			UE_LOG(LogMetal, Warning,  TEXT("GPU descriptor (%s) from IORegistry failed to match Metal (%s)"), *FString(GPUDesc.GPUName), *GRHIAdapterName);
		}
#endif
	}

	GDynamicRHI = this;
	
#if PLATFORM_DESKTOP
	if (GMaxRHIFeatureLevel < ERHIFeatureLevel::SM5)
	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.Optimize"));
		
		if (CVar->GetInt() == 0)
		{
			FShaderCache::InitShaderCache(SCO_NoShaderPreload, 128);
		}
		else
		{
			FShaderCache::InitShaderCache(SCO_Default, 128);
		}
	}
#endif

	// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitRHI();
	}
	// Dynamic resources can have dependencies on static resources (with uniform buffers) and must initialized last!
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitDynamicRHI();
	}
	
    Profiler = nullptr;
#if ENABLE_METAL_GPUPROFILE
	Profiler = new FMetalGPUProfiler(Context);
#endif
	AsyncComputeContext = GSupportsEfficientAsyncCompute ? new FMetalRHIComputeContext(Profiler, new FMetalContext(Context->GetCommandQueue(), true)) : nullptr;
}

FMetalDynamicRHI::~FMetalDynamicRHI()
{
	check(IsInGameThread() && IsInRenderingThread());

#if ENABLE_METAL_GPUPROFILE
	delete Profiler;
#endif
	
#if PLATFORM_DESKTOP
	FShaderCache::ShutdownShaderCache();
#endif
	
	GIsRHIInitialized = false;
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
	GIsRHIInitialized = true;
}

void FMetalDynamicRHI::RHIBeginFrame()
{
	// @todo zebra: GPUProfilingData, GNumDrawCallsRHI, GNumPrimitivesDrawnRHI
#if ENABLE_METAL_GPUPROFILE
	Profiler->BeginFrame();
#endif
	((FMetalDeviceContext*)Context)->BeginFrame();
}

void FMetalRHICommandContext::RHIBeginFrame()
{
	check(false);
}

void FMetalDynamicRHI::RHIEndFrame()
{
	// @todo zebra: GPUProfilingData.EndFrame();
#if ENABLE_METAL_GPUPROFILE
	Profiler->EndFrame();
#endif
	((FMetalDeviceContext*)Context)->EndFrame();
}

void FMetalRHICommandContext::RHIEndFrame()
{
	check(false);
}

void FMetalDynamicRHI::RHIBeginScene()
{
	((FMetalDeviceContext*)Context)->BeginScene();
}

void FMetalRHICommandContext::RHIBeginScene()
{
	check(false);
}

void FMetalDynamicRHI::RHIEndScene()
{
	((FMetalDeviceContext*)Context)->EndScene();
}

void FMetalRHICommandContext::RHIEndScene()
{
	check(false);
}

void FMetalRHICommandContext::RHIPushEvent(const TCHAR* Name, FColor Color)
{
#if ENABLE_METAL_GPUEVENTS
#if ENABLE_METAL_GPUPROFILE
	Profiler->PushEvent(Name, Color);
#endif
	// @todo zebra : this was "[NSString stringWithTCHARString:Name]", an extension only on ios for some reason, it should be on Mac also
	Context->GetCommandEncoder().PushDebugGroup([NSString stringWithCString:TCHAR_TO_UTF8(Name) encoding:NSUTF8StringEncoding]);
#endif
}

void FMetalRHICommandContext::RHIPopEvent()
{
#if ENABLE_METAL_GPUEVENTS
	Context->GetCommandEncoder().PopDebugGroup();
#if ENABLE_METAL_GPUPROFILE
	Profiler->PopEvent();
#endif
#endif
}

void FMetalDynamicRHI::RHIGetSupportedResolution( uint32 &Width, uint32 &Height )
{
#if PLATFORM_MAC
	uint32 InitializedMode = false;
	uint32 BestWidth = 0;
	uint32 BestHeight = 0;
	
	CFArrayRef AllModes = CGDisplayCopyAllDisplayModes(kCGDirectMainDisplay, NULL);
	if (AllModes)
	{
		int32 NumModes = CFArrayGetCount(AllModes);
		for (int32 Index = 0; Index < NumModes; Index++)
		{
			CGDisplayModeRef Mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(AllModes, Index);
			int32 ModeWidth = (int32)CGDisplayModeGetWidth(Mode);
			int32 ModeHeight = (int32)CGDisplayModeGetHeight(Mode);
			
			bool IsEqualOrBetterWidth = FMath::Abs((int32)ModeWidth - (int32)Width) <= FMath::Abs((int32)BestWidth - (int32)Width);
			bool IsEqualOrBetterHeight = FMath::Abs((int32)ModeHeight - (int32)Height) <= FMath::Abs((int32)BestHeight - (int32)Height);
			if(!InitializedMode || (IsEqualOrBetterWidth && IsEqualOrBetterHeight))
			{
				BestWidth = ModeWidth;
				BestHeight = ModeHeight;
				InitializedMode = true;
			}
		}
		CFRelease(AllModes);
	}
	check(InitializedMode);
	Width = BestWidth;
	Height = BestHeight;
#else
	UE_LOG(LogMetal, Warning,  TEXT("RHIGetSupportedResolution unimplemented!"));
#endif
}

bool FMetalDynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
#if PLATFORM_MAC
	const int32 MinAllowableResolutionX = 0;
	const int32 MinAllowableResolutionY = 0;
	const int32 MaxAllowableResolutionX = 10480;
	const int32 MaxAllowableResolutionY = 10480;
	const int32 MinAllowableRefreshRate = 0;
	const int32 MaxAllowableRefreshRate = 10480;
	
	CFArrayRef AllModes = CGDisplayCopyAllDisplayModes(kCGDirectMainDisplay, NULL);
	if (AllModes)
	{
		const int32 NumModes = CFArrayGetCount(AllModes);
		const int32 Scale = FMacApplication::GetPrimaryScreenBackingScaleFactor();
		
		for (int32 Index = 0; Index < NumModes; Index++)
		{
			const CGDisplayModeRef Mode = (const CGDisplayModeRef)CFArrayGetValueAtIndex(AllModes, Index);
			const int32 Width = (int32)CGDisplayModeGetWidth(Mode) / Scale;
			const int32 Height = (int32)CGDisplayModeGetHeight(Mode) / Scale;
			const int32 RefreshRate = (int32)CGDisplayModeGetRefreshRate(Mode);
			
			if (Width >= MinAllowableResolutionX && Width <= MaxAllowableResolutionX && Height >= MinAllowableResolutionY && Height <= MaxAllowableResolutionY)
			{
				bool bAddIt = true;
				if (bIgnoreRefreshRate == false)
				{
					if (RefreshRate < MinAllowableRefreshRate || RefreshRate > MaxAllowableRefreshRate)
					{
						continue;
					}
				}
				else
				{
					// See if it is in the list already
					for (int32 CheckIndex = 0; CheckIndex < Resolutions.Num(); CheckIndex++)
					{
						FScreenResolutionRHI& CheckResolution = Resolutions[CheckIndex];
						if ((CheckResolution.Width == Width) &&
							(CheckResolution.Height == Height))
						{
							// Already in the list...
							bAddIt = false;
							break;
						}
					}
				}
				
				if (bAddIt)
				{
					// Add the mode to the list
					const int32 Temp2Index = Resolutions.AddZeroed();
					FScreenResolutionRHI& ScreenResolution = Resolutions[Temp2Index];
					
					ScreenResolution.Width = Width;
					ScreenResolution.Height = Height;
					ScreenResolution.RefreshRate = RefreshRate;
				}
			}
		}
		
		CFRelease(AllModes);
	}
	
	return true;
#else
	UE_LOG(LogMetal, Warning,  TEXT("RHIGetAvailableResolutions unimplemented!"));
	return false;
#endif
}

void FMetalDynamicRHI::RHIFlushResources()
{

}

void FMetalDynamicRHI::RHIAcquireThreadOwnership()
{
	Context->CreateAutoreleasePool();
	SetupRecursiveResources();
}

void FMetalDynamicRHI::RHIReleaseThreadOwnership()
{
	Context->DrainAutoreleasePool();
}

void* FMetalDynamicRHI::RHIGetNativeDevice()
{
	return (void*)Context->GetDevice();
}

