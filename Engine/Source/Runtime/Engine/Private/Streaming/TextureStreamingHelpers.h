// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureStreamingHelpers.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

/**
 * Streaming stats
 */

DECLARE_CYCLE_STAT_EXTERN(TEXT("Game Thread Update Time"),STAT_GameThreadUpdateTime,STATGROUP_Streaming, );


// Streaming Details

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Growing Reallocations"),STAT_GrowingReallocations,STATGROUP_StreamingDetails, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Shrinking Reallocations"),STAT_ShrinkingReallocations,STATGROUP_StreamingDetails, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Full Reallocations"),STAT_FullReallocations,STATGROUP_StreamingDetails, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Failed Reallocations"),STAT_FailedReallocations,STATGROUP_StreamingDetails, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Panic Defragmentations"),STAT_PanicDefragmentations,STATGROUP_StreamingDetails, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("AddToWorld Time"),STAT_AddToWorldTime,STATGROUP_StreamingDetails, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("RemoveFromWorld Time"),STAT_RemoveFromWorldTime,STATGROUP_StreamingDetails, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("UpdateLevelStreaming Time"),STAT_UpdateLevelStreamingTime,STATGROUP_StreamingDetails, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Volume Streaming Tick"),STAT_VolumeStreamingTickTime,STATGROUP_StreamingDetails, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Streaming Volumes"),STAT_VolumeStreamingChecks,STATGROUP_StreamingDetails, );


DECLARE_LOG_CATEGORY_EXTERN(LogContentStreaming, Log, All);

extern float GLightmapStreamingFactor;
extern float GShadowmapStreamingFactor;
extern bool GNeverStreamOutTextures;

//@DEBUG:
// Set to 1 to log all dynamic component notifications
#define STREAMING_LOG_DYNAMIC		0
// Set to 1 to log when we change a view
#define STREAMING_LOG_VIEWCHANGES	0
// Set to 1 to log when levels are added/removed
#define STREAMING_LOG_LEVELS		0
// Set to 1 to log textures that are canceled by CancelForcedTextures()
#define STREAMING_LOG_CANCELFORCED	0

#if PLATFORM_SUPPORTS_TEXTURE_STREAMING
extern TAutoConsoleVariable<int32> CVarSetTextureStreaming;
#endif

extern TAutoConsoleVariable<float> CVarStreamingBoost;
extern TAutoConsoleVariable<int32> CVarStreamingUseFixedPoolSize;
extern TAutoConsoleVariable<int32> CVarStreamingPoolSize;
extern TAutoConsoleVariable<int32> CVarStreamingCheckBuildStatus;

struct FTextureStreamingSettings
{
	FTextureStreamingSettings() { Update(); }
	void Update();

	FORCEINLINE bool operator ==(const FTextureStreamingSettings& Rhs) const { return FMemory::Memcmp(this, &Rhs, sizeof(FTextureStreamingSettings)) == 0; }
	FORCEINLINE bool operator !=(const FTextureStreamingSettings& Rhs) const { return FMemory::Memcmp(this, &Rhs, sizeof(FTextureStreamingSettings)) != 0; }

	FORCEINLINE float GlobalMipBias() const { return !bUsePerTextureBias ? MipBias : 0; }
	FORCEINLINE int32 GlobalMipBiasAsInt() const { return !bUsePerTextureBias ? FMath::FloorToInt(MipBias) : 0; }
	FORCEINLINE float GlobalMipBiasAsScale() const { return !bUsePerTextureBias ? FMath::Exp2(-MipBias) : 1.f; }

	FORCEINLINE int32 MaxExpectedPerTextureMipBias() const { return bUsePerTextureBias ? FMath::FloorToInt(MipBias) : 0; }

	float MaxEffectiveScreenSize;
	int32 MaxTempMemoryAllowed;
	int32 DropMips;
	int32 HLODStrategy;
	float HiddenPrimitiveScale;
	int32 PoolSize;
	bool bLimitPoolSizeToVRAM;
	bool bUseNewMetrics;
	bool bFullyLoadUsedTextures;
	bool bUseAllMips;
	bool bUsePerTextureBias;

protected:

	float MipBias;
};

typedef TArray<int32, TMemStackAllocator<> > FStreamingRequests;
typedef TArray<const UTexture2D*, TInlineAllocator<12> > FRemovedTextureArray;

#define NUM_BANDWIDTHSAMPLES 512
#define NUM_LATENCYSAMPLES 512

/** Streaming priority: Linear distance factor from 0 to MAX_STREAMINGDISTANCE. */
#define MAX_STREAMINGDISTANCE	10000.0f
#define MAX_MIPDELTA			5.0f
#define MAX_LASTRENDERTIME		90.0f

class UTexture2D;
class UPrimitiveComponent;
class FTextureBoundsVisibility;
class FDynamicComponentTextureManager;
template<typename T>
class FAsyncTask;
class FAsyncTextureStreamingTask;


struct FStreamingTexture;
struct FStreamingContext;
struct FStreamingHandlerTextureBase;
struct FTexturePriority;

/**
 * Checks whether a UTexture2D is supposed to be streaming.
 * @param Texture	Texture to check
 * @return			true if the UTexture2D is supposed to be streaming
 */
bool IsStreamingTexture( const UTexture2D* Texture2D );


/**
 *	Helper struct that represents a texture and the parameters used for sorting and streaming out high-res mip-levels.
 **/
struct FTextureSortElement
{
	/**
	 *	Constructor.
	 *
	 *	@param InTexture					- The texture to represent
	 *	@param InSize						- Size of the whole texture and all current mip-levels, in bytes
	 *	@param bInIsCharacterTexture		- 1 if this is a character texture, otherwise 0
	 *	@param InTextureDataAddress			- Starting address of the texture data
	 *	@param InNumRequiredResidentMips	- Minimum number of mip-levels required to stay in memory
	 */
	FTextureSortElement( UTexture2D* InTexture, int32 InSize, int32 bInIsCharacterTexture, uint32 InTextureDataAddress, int32 InNumRequiredResidentMips )
	:	Texture( InTexture )
	,	Size( InSize )
	,	bIsCharacterTexture( bInIsCharacterTexture )
	,	TextureDataAddress( InTextureDataAddress )
	,	NumRequiredResidentMips( InNumRequiredResidentMips )
	{
	}
	/** The texture that this element represents */
	UTexture2D*	Texture;
	/** Size of the whole texture and all current mip-levels, in bytes. */
	int32			Size;
	/** 1 if this is a character texture, otherwise 0 */
	int32			bIsCharacterTexture;
	/** Starting address of the texture data. */
	uint32		TextureDataAddress;			
	/** Minimum number of mip-levels required to stay in memory. */
	int32			NumRequiredResidentMips;
};



struct FTextureStreamingStats
{
	FTextureStreamingStats() { Reset(); }

	void Reset() { FMemory::Memzero(this, sizeof(FTextureStreamingStats)); }

	void Apply();

	int64 TexturePool;
	// int64 StreamingPool;
	int64 UsedStreamingPool;

	int64 SafetyPool;
	int64 TemporaryPool;
	int64 StreamingPool;
	int64 NonStreamingMips;

	int64 RequiredPool;
	int64 VisibleMips;
	int64 HiddenMips;
	int64 ForcedMips;
	int64 CachedMips;

	int64 WantedMips;
	int64 NewRequests; // How much texture memory is required by new requests.
	int64 PendingRequests; // How much texture memory is waiting to be loaded for previous requests.
	int64 MipIOBandwidth;

	int64 OverBudget;

	double Timestamp;

	
	int32 SetupAsyncTaskCycles;
	int32 UpdateStreamingDataCycles;
	int32 StreamTexturesCycles;
};
