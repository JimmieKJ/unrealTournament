// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureStreamingHelpers.cpp: Definitions of classes used for texture streaming.
=============================================================================*/

#include "EnginePrivate.h"
#include "TextureStreamingHelpers.h"
#include "Engine/Texture2D.h"
#include "GenericPlatformMemoryPoolStats.h"
#include "Engine/LightMapTexture2D.h"
#include "Engine/ShadowMapTexture2D.h"

/** Streaming stats */

// Streaming
DECLARE_MEMORY_STAT_POOL(TEXT("Safety Pool"), STAT_Streaming01_SafetyPool, STATGROUP_Streaming, FPlatformMemory::MCR_TexturePool);
DECLARE_MEMORY_STAT_POOL(TEXT("Temporary Pool"), STAT_Streaming02_TemporaryPool, STATGROUP_Streaming, FPlatformMemory::MCR_TexturePool);
DECLARE_MEMORY_STAT_POOL(TEXT("Streaming Pool"), STAT_Streaming03_StreamingPool, STATGROUP_Streaming, FPlatformMemory::MCR_TexturePool);
DECLARE_MEMORY_STAT_POOL(TEXT("NonStreaming Mips"), STAT_Streaming04_NonStreamingMips, STATGROUP_Streaming, FPlatformMemory::MCR_TexturePool);

DECLARE_MEMORY_STAT_POOL(TEXT("Required Pool"), STAT_Streaming05_RequiredPool, STATGROUP_Streaming, FPlatformMemory::MCR_StreamingPool);
DECLARE_MEMORY_STAT_POOL(TEXT("Visible Mips"), STAT_Streaming06_VisibleMips, STATGROUP_Streaming, FPlatformMemory::MCR_StreamingPool);
DECLARE_MEMORY_STAT_POOL(TEXT("Hidden Mips"), STAT_Streaming07_HiddenMips, STATGROUP_Streaming, FPlatformMemory::MCR_StreamingPool);
DECLARE_MEMORY_STAT_POOL(TEXT("Forced Mips"), STAT_Streaming08_ForcedMips, STATGROUP_Streaming, FPlatformMemory::MCR_StreamingPool);
DECLARE_MEMORY_STAT_POOL(TEXT("Cached Mips"), STAT_Streaming09_CachedMips, STATGROUP_Streaming, FPlatformMemory::MCR_StreamingPool);

DECLARE_MEMORY_STAT_POOL(TEXT("Wanted Mips"), STAT_Streaming10_WantedMips, STATGROUP_Streaming, FPlatformMemory::MCR_UsedStreamingPool);
DECLARE_MEMORY_STAT_POOL(TEXT("Inflight Requests"), STAT_Streaming11_InflightRequests, STATGROUP_Streaming, FPlatformMemory::MCR_UsedStreamingPool);
DECLARE_MEMORY_STAT_POOL(TEXT("IO Bandwidth"), STAT_Streaming12_MipIOBandwidth, STATGROUP_Streaming, FPlatformMemory::MCR_UsedStreamingPool);

DECLARE_CYCLE_STAT(TEXT("Setup Async Task"), STAT_Streaming01_SetupAsyncTask, STATGROUP_Streaming);
DECLARE_CYCLE_STAT(TEXT("Update Streaming Data"), STAT_Streaming02_UpdateStreamingData, STATGROUP_Streaming);
DECLARE_CYCLE_STAT(TEXT("Streaming Texture"), STAT_Streaming03_StreamTextures, STATGROUP_Streaming);

DEFINE_STAT(STAT_GameThreadUpdateTime);

// StreamingDetails
DEFINE_STAT(STAT_GrowingReallocations);
DEFINE_STAT(STAT_ShrinkingReallocations);
DEFINE_STAT(STAT_FullReallocations);
DEFINE_STAT(STAT_FailedReallocations);
DEFINE_STAT(STAT_PanicDefragmentations);

DEFINE_LOG_CATEGORY(LogContentStreaming);

ENGINE_API TAutoConsoleVariable<int32> CVarStreamingUseNewMetrics(
	TEXT("r.Streaming.UseNewMetrics"),
	1,
	TEXT("If non-zero, will use tight AABB bounds and improved texture factors."),
	ECVF_Default);

TAutoConsoleVariable<float> CVarStreamingBoost(
	TEXT("r.Streaming.Boost"),
	1.0f,
	TEXT("=1.0: normal\n")
	TEXT("<1.0: decrease wanted mip levels\n")
	TEXT(">1.0: increase wanted mip levels"),
	ECVF_Scalability
	);

TAutoConsoleVariable<float> CVarStreamingScreenSizeEffectiveMax(
	TEXT("r.Streaming.MaxEffectiveScreenSize"),
	0,
	TEXT("0: Use current actual vertical screen size\n")	
	TEXT("> 0: Clamp wanted mip size calculation to this value for the vertical screen size component."),
	ECVF_Scalability
	);

#if PLATFORM_SUPPORTS_TEXTURE_STREAMING
TAutoConsoleVariable<int32> CVarSetTextureStreaming(
	TEXT("r.TextureStreaming"),
	1,
	TEXT("Allows to define if texture streaming is enabled, can be changed at run time.\n")
	TEXT("0: off\n")
	TEXT("1: on (default)"),
	ECVF_Default | ECVF_RenderThreadSafe);
#endif

TAutoConsoleVariable<int32> CVarStreamingUseFixedPoolSize(
	TEXT("r.Streaming.UseFixedPoolSize"),
	0,
	TEXT("If non-zero, do not allow the pool size to change at run time."),
	ECVF_ReadOnly);

TAutoConsoleVariable<int32> CVarStreamingPoolSize(
	TEXT("r.Streaming.PoolSize"),
	-1,
	TEXT("-1: Default texture pool size, otherwise the size in MB"),
	ECVF_Scalability);

TAutoConsoleVariable<int32> CVarStreamingMaxTempMemoryAllowed(
	TEXT("r.Streaming.MaxTempMemoryAllowed"),
	50,
	TEXT("Maximum temporary memory used when streaming in or out texture mips.\n")
	TEXT("This memory contains mips used for the new updated texture.\n")
	TEXT("The value must be high enough to not be a limiting streaming speed factor.\n"),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarStreamingDropMips(
	TEXT("r.Streaming.DropMips"),
	0,
	TEXT("0: Drop No Mips \n")
	TEXT("1: Drop Cached Mips\n")
	TEXT("2: Drop Cached and Hidden Mips"),
	ECVF_Cheat);

TAutoConsoleVariable<int32> CVarStreamingHLODStrategy(
	TEXT("r.Streaming.HLODStrategy"),
	0,
	TEXT("Define the HLOD streaming strategy.\n")
	TEXT("0: stream\n")
	TEXT("1: stream only mip 0\n")
	TEXT("2: disable streaming"),
	ECVF_Default);

TAutoConsoleVariable<float> CVarStreamingHiddenPrimitiveScale(
	TEXT("r.Streaming.HiddenPrimitiveScale"),
	0.5,
	TEXT("Define the resolution scale to apply when not in range.\n")
	TEXT(".5: drop one mip\n")
	TEXT("1: ignore visiblity"),
	ECVF_Default
	);

// Used for scalability (GPU memory, streaming stalls)
TAutoConsoleVariable<float> CVarStreamingMipBias(
	TEXT("r.Streaming.MipBias"),
	0.0f,
	TEXT("0..x reduce texture quality for streaming by a floating point number.\n")
	TEXT("0: use full resolution (default)\n")
	TEXT("1: drop one mip\n")
	TEXT("2: drop two mips"),
	ECVF_Scalability
	);

TAutoConsoleVariable<int32> CVarStreamingUsePerTextureBias(
	TEXT("r.Streaming.UsePerTextureBias"),
	0,
	TEXT("If non-zero, each texture will be assigned a mip bias between 0 and MipBias as required to fit in budget."),
	ECVF_Default);


TAutoConsoleVariable<int32> CVarStreamingFullyLoadUsedTextures(
	TEXT("r.Streaming.FullyLoadUsedTextures"),
	0,
	TEXT("If non-zero, all used texture will be fully streamed in as fast as possible"),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarStreamingUseAllMips(
	TEXT("r.Streaming.UseAllMips"),
	0,
	TEXT("If non-zero, all available mips will be used"),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarStreamingLimitPoolSizeToVRAM(
	TEXT("r.Streaming.LimitPoolSizeToVRAM"),
	0,
	TEXT("If non-zero, texture pool size with be limited to how much GPU mem is available."),
	ECVF_Scalability);


void FTextureStreamingSettings::Update()
{
	MaxEffectiveScreenSize = CVarStreamingScreenSizeEffectiveMax.GetValueOnAnyThread();
	MaxTempMemoryAllowed = CVarStreamingMaxTempMemoryAllowed.GetValueOnAnyThread();
	DropMips = CVarStreamingDropMips.GetValueOnAnyThread();
	HLODStrategy = CVarStreamingHLODStrategy.GetValueOnAnyThread();
	HiddenPrimitiveScale = CVarStreamingHiddenPrimitiveScale.GetValueOnAnyThread();
	MipBias = FMath::Max<float>(0.f, CVarStreamingMipBias.GetValueOnAnyThread());
	PoolSize = CVarStreamingPoolSize.GetValueOnAnyThread();
	bUsePerTextureBias = !GIsEditor && CVarStreamingUsePerTextureBias.GetValueOnAnyThread() != 0;
	bUseNewMetrics = CVarStreamingUseNewMetrics.GetValueOnAnyThread() != 0;
	bLimitPoolSizeToVRAM = !GIsEditor && CVarStreamingLimitPoolSizeToVRAM.GetValueOnAnyThread() != 0;
	bFullyLoadUsedTextures = CVarStreamingFullyLoadUsedTextures.GetValueOnAnyThread() != 0;
	bUseAllMips = CVarStreamingUseAllMips.GetValueOnAnyThread() != 0;

	if (bUseAllMips)
	{
		bUsePerTextureBias = false;
		MipBias = 0;
	}
}


extern ENGINE_API UPrimitiveComponent* GDebugSelectedComponent;

/** the float table {-1.0f,1.0f} **/
float ENGINE_API GNegativeOneOneTable[2] = {-1.0f,1.0f};


/** Smaller value will stream out lightmaps more aggressively. */
float GLightmapStreamingFactor = 1.0f;

/** Smaller value will stream out shadowmaps more aggressively. */
float GShadowmapStreamingFactor = 0.09f;

/** For testing, finding useless textures or special demo purposes. If true, textures will never be streamed out (but they can be GC'd). 
* Caution: this only applies to unlimited texture pools (i.e. not consoles)
*/
bool GNeverStreamOutTextures = false;

/**
 * Checks whether a UTexture2D is supposed to be streaming.
 * @param Texture	Texture to check
 * @return			true if the UTexture2D is supposed to be streaming
 */
bool IsStreamingTexture( const UTexture2D* Texture2D )
{
	return Texture2D && Texture2D->bIsStreamable && !Texture2D->NeverStream && Texture2D->GetNumMips() > Texture2D->GetNumNonStreamingMips();
}


void FTextureStreamingStats::Apply()
{
	SET_MEMORY_STAT(MCR_TexturePool, TexturePool); 
	SET_MEMORY_STAT(MCR_StreamingPool, StreamingPool);
	SET_MEMORY_STAT(MCR_UsedStreamingPool, UsedStreamingPool);

	SET_MEMORY_STAT(STAT_Streaming01_SafetyPool, SafetyPool);
	SET_MEMORY_STAT(STAT_Streaming02_TemporaryPool, TemporaryPool);
	SET_MEMORY_STAT(STAT_Streaming03_StreamingPool, StreamingPool);
	SET_MEMORY_STAT(STAT_Streaming04_NonStreamingMips, NonStreamingMips);
		
	SET_MEMORY_STAT(STAT_Streaming05_RequiredPool, RequiredPool);
	SET_MEMORY_STAT(STAT_Streaming06_VisibleMips, VisibleMips);
	SET_MEMORY_STAT(STAT_Streaming07_HiddenMips, HiddenMips);
	SET_MEMORY_STAT(STAT_Streaming08_ForcedMips, ForcedMips);
	SET_MEMORY_STAT(STAT_Streaming09_CachedMips, CachedMips);
		
	SET_MEMORY_STAT(STAT_Streaming10_WantedMips, WantedMips);
	SET_MEMORY_STAT(STAT_Streaming11_InflightRequests, PendingRequests);	
	SET_MEMORY_STAT(STAT_Streaming12_MipIOBandwidth, MipIOBandwidth);

	SET_CYCLE_COUNTER(STAT_Streaming01_SetupAsyncTask, SetupAsyncTaskCycles);
	SET_CYCLE_COUNTER(STAT_Streaming02_UpdateStreamingData, UpdateStreamingDataCycles);
	SET_CYCLE_COUNTER(STAT_Streaming03_StreamTextures, StreamTexturesCycles);
}
