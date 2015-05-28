// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ContentStreaming.cpp: Implementation of content streaming classes.
=============================================================================*/

#include "EnginePrivate.h"
#include "GenericPlatformMemoryPoolStats.h"
#include "AudioStreaming.h"
#include "Engine/LightMapTexture2D.h"
#include "Engine/ShadowMapTexture2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogContentStreaming, Log, All);

/*-----------------------------------------------------------------------------
	Stats.
-----------------------------------------------------------------------------*/

//@DEBUG:
// Set to 1 to log all dynamic component notifications
#define STREAMING_LOG_DYNAMIC		0
// Set to 1 to log when we change a view
#define STREAMING_LOG_VIEWCHANGES	0
// Set to 1 to log when levels are added/removed
#define STREAMING_LOG_LEVELS		0
// Set to 1 to log textures that are canceled by CancelForcedTextures()
#define STREAMING_LOG_CANCELFORCED	0

/** Streaming stats */

DEFINE_STAT(STAT_GameThreadUpdateTime);
DEFINE_STAT(STAT_TexturePoolSize);
DEFINE_STAT(STAT_TexturePoolAllocatedSize);
DEFINE_STAT(STAT_OptimalTextureSize);
DEFINE_STAT(STAT_StreamingTexturesSize);
DEFINE_STAT(STAT_NonStreamingTexturesSize);
DEFINE_STAT(STAT_StreamingTexturesMaxSize);
DEFINE_STAT(STAT_StreamingOverBudget);
DEFINE_STAT(STAT_StreamingUnderBudget);
DEFINE_STAT(STAT_NumWantingTextures);

DEFINE_STAT(STAT_StreamingTextures);

DEFINE_STAT(STAT_TotalStaticTextureHeuristicSize);
DEFINE_STAT(STAT_TotalDynamicHeuristicSize);
DEFINE_STAT(STAT_TotalLastRenderHeuristicSize);
DEFINE_STAT(STAT_TotalForcedHeuristicSize);

DEFINE_STAT(STAT_LightmapMemorySize);
DEFINE_STAT(STAT_LightmapDiskSize);
DEFINE_STAT(STAT_IntermediateTexturesSize);
DEFINE_STAT(STAT_RequestSizeCurrentFrame);
DEFINE_STAT(STAT_RequestSizeTotal);
DEFINE_STAT(STAT_LightmapRequestSizeTotal);

DEFINE_STAT(STAT_IntermediateTextures);
DEFINE_STAT(STAT_RequestsInCancelationPhase);
DEFINE_STAT(STAT_RequestsInUpdatePhase);
DEFINE_STAT(STAT_RequestsInFinalizePhase);
DEFINE_STAT(STAT_StreamingLatency);
DEFINE_STAT(STAT_StreamingBandwidth);
DEFINE_STAT(STAT_GrowingReallocations);
DEFINE_STAT(STAT_ShrinkingReallocations);
DEFINE_STAT(STAT_FullReallocations);
DEFINE_STAT(STAT_FailedReallocations);
DEFINE_STAT(STAT_PanicDefragmentations);
DEFINE_STAT(STAT_DynamicStreamingTotal);



/** Accumulated total time spent on dynamic primitives, in seconds. */
double GStreamingDynamicPrimitivesTime = 0.0;


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

/** Collection of views that need to be taken into account for streaming. */
TArray<FStreamingViewInfo> IStreamingManager::CurrentViewInfos;

/** Pending views. Emptied every frame. */
TArray<FStreamingViewInfo> IStreamingManager::PendingViewInfos;

/** Views that stick around for a while. Override views are ignored if no movie is playing. */
TArray<FStreamingViewInfo> IStreamingManager::LastingViewInfos;

/** Collection of view locations that will be added at the next call to AddViewInformation. */
TArray<IStreamingManager::FSlaveLocation> IStreamingManager::SlaveLocations;

/** Set when Tick() has been called. The first time a new view is added, it will clear out all old views. */
bool IStreamingManager::bPendingRemoveViews = false;

/** The lightmap used by the currently selected component (toggledebugcamera), if it's a static mesh component. */
extern ENGINE_API class FLightMap2D* GDebugSelectedLightmap;

/** Whether the texture pool size has been artificially lowered for testing. */
bool GIsOperatingWithReducedTexturePool = false;

/** Smaller value will stream out lightmaps more aggressively. */
float GLightmapStreamingFactor = 1.0f;

/** Smaller value will stream out shadowmaps more aggressively. */
float GShadowmapStreamingFactor = 0.09f;

/** For testing, finding useless textures or special demo purposes. If true, textures will never be streamed out (but they can be GC'd). 
* Caution: this only applies to unlimited texture pools (i.e. not consoles)
*/
bool GNeverStreamOutTextures = false;

// Used for scalability (GPU memory, streaming stalls)
static TAutoConsoleVariable<float> CVarStreamingMipBias(
	TEXT("r.Streaming.MipBias"),
	0.0f,
	TEXT("0..x reduce texture quality for streaming by a floating point number.\n")
	TEXT("0: use full resolution (default)\n")
	TEXT("1: drop one mip\n")
	TEXT("2: drop two mips"),
	ECVF_Scalability
	);

static TAutoConsoleVariable<float> CVarStreamingBoost(
	TEXT("r.Streaming.Boost"),
	1.0f,
	TEXT("=1.0: normal\n")
	TEXT("<1.0: decrease wanted mip levels\n")
	TEXT(">1.0: increase wanted mip levels"),
	ECVF_Default
	);

#if PLATFORM_SUPPORTS_TEXTURE_STREAMING
static TAutoConsoleVariable<int32> CVarSetTextureStreaming(
	TEXT("r.TextureStreaming"),
	1,
	TEXT("Allows to define if texture streaming is enabled, can be changed at run time.\n")
	TEXT("0: off\n")
	TEXT("1: on (default)"),
	ECVF_Default | ECVF_RenderThreadSafe);
#endif

static TAutoConsoleVariable<int32> CVarStreamingUseFixedPoolSize(
	TEXT("r.Streaming.UseFixedPoolSize"),
	0,
	TEXT("If non-zero, do not allow the pool size to change at run time."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarStreamingPoolSize(
	TEXT("r.Streaming.PoolSize"),
	-1,
	TEXT("-1: Default texture pool size, otherwise the size in MB"),
	ECVF_Scalability);

/** Streaming priority: Linear distance factor from 0 to MAX_STREAMINGDISTANCE. */
#define MAX_STREAMINGDISTANCE	10000.0f
#define MAX_MIPDELTA			5.0f
#define MAX_LASTRENDERTIME		90.0f

/** For debugging purposes: Whether to consider the time factor when streaming. Turning it off is easier for debugging. */
bool GStreamWithTimeFactor		= true;

extern ENGINE_API UPrimitiveComponent* GDebugSelectedComponent;

#if STATS
/** Ringbuffer of bandwidth samples for streaming in mip-levels (MB/sec). */
float FStreamingManagerTexture::BandwidthSamples[NUM_BANDWIDTHSAMPLES];
/** Number of bandwidth samples that have been filled in. Will stop counting when it reaches NUM_BANDWIDTHSAMPLES. */
int32 FStreamingManagerTexture::NumBandwidthSamples = 0;
/** Current sample index in the ring buffer. */
int32 FStreamingManagerTexture::BandwidthSampleIndex = 0;
/** Average of all bandwidth samples in the ring buffer, in MB/sec. */
float FStreamingManagerTexture::BandwidthAverage = 0.0f;
/** Maximum bandwidth measured since the start of the game.  */
float FStreamingManagerTexture::BandwidthMaximum = 0.0f;
/** Minimum bandwidth measured since the start of the game.  */
float FStreamingManagerTexture::BandwidthMinimum = 0.0f;
#endif

/** the float table {-1.0f,1.0f} **/
float ENGINE_API GNegativeOneOneTable[2] = {-1.0f,1.0f};

/**
 * Helper function to flush resource streaming from within Core project.
 */
void FlushResourceStreaming()
{
	RETURN_IF_EXIT_REQUESTED;
	IStreamingManager::Get().BlockTillAllRequestsFinished();
}

/**
 * Helper function to clamp the mesh to camera distance
 */
FORCEINLINE float ClampMeshToCameraDistanceSquared(float MeshToCameraDistanceSquared)
{
	// called from streaming thread, maybe even main thread
	return FMath::Max<float>(MeshToCameraDistanceSquared, 0.0f);
}


/*-----------------------------------------------------------------------------
	FStreamingTexture, the streaming system's version of UTexture2D.
-----------------------------------------------------------------------------*/

enum ETextureStreamingType
{
	StreamType_Static,
	StreamType_Dynamic,
	StreamType_Forced,
	StreamType_LastRenderTime,
	StreamType_Orphaned,
	StreamType_Other,
};

static const TCHAR* GStreamTypeNames[] =
{
	TEXT("Static"),
	TEXT("Dynamic"),
	TEXT("Forced"),
	TEXT("LastRenderTime"),
	TEXT("Orphaned"),
	TEXT("Other"),
};

struct FTexturePriority
{
	FTexturePriority( float InPriority, int32 InTextureIndex )
	:	Priority( InPriority )
	,	TextureIndex( InTextureIndex )
	{
	}
	/** Texture priority, higher value means more important. */
	float Priority;
	/** Index into the FStreamingManagerTexture::StreamingTextures array. */
	int32 TextureIndex;
};


/** Self-contained structure to manage a streaming texture, possibly on a separate thread. */
struct FStreamingTexture
{
	FStreamingTexture( UTexture2D* InTexture )
	{
		Texture = InTexture;
		WantedMips = InTexture->ResidentMips;
		DynamicScreenSize = 0.0f;
		MipCount = InTexture->GetNumMips();
		STAT( PerfectWantedMips = InTexture->ResidentMips );
		STAT( MostResidentMips = InTexture->ResidentMips );
		LODGroup = (TextureGroup) InTexture->LODGroup;
		NumNonStreamingMips = InTexture->GetNumNonStreamingMips();
		ForceLoadRefCount = 0;
		bIsStreamingLightmap = IsStreamingLightmap( Texture );
		bUsesStaticHeuristics = false;
		bUsesDynamicHeuristics = false;
		bUsesLastRenderHeuristics = false;
		bUsesForcedHeuristics = false;
		bUsesOrphanedHeuristics = false;
		bNeedPrimitiveUpdate = false;
		BoostFactor = 1.0f;
		InstanceRemovedTimestamp = -FLT_MAX;
		LastRenderTimeRefCountTimestamp = -FLT_MAX;
		LastRenderTimeRefCount = 0;

		for ( int32 MipIndex=1; MipIndex <= MAX_TEXTURE_MIP_COUNT; ++MipIndex )
		{
			TextureSizes[MipIndex - 1] = Texture->CalcTextureMemorySize( FMath::Min(MipIndex, MipCount) );
		}

		UpdateCachedInfo();
	}

	/** Reinitializes the cached variables from UTexture2D. */
	void UpdateCachedInfo( )
	{
		check(Texture && Texture->Resource);
		ResidentMips = Texture->ResidentMips;
		RequestedMips = Texture->RequestedMips;
		MinAllowedMips = 1;			//FMath::Min(Texture->ResidentMips, Texture->RequestedMips);
		MaxAllowedMips = MipCount;	//FMath::Max(Texture->ResidentMips, Texture->RequestedMips);
		STAT( MaxAllowedOptimalMips = MaxAllowedMips );
		STAT( MostResidentMips = FMath::Max(MostResidentMips, Texture->ResidentMips) );
		float LastRenderTimeForTexture = Texture->GetLastRenderTimeForStreaming();
		LastRenderTime = (FApp::GetCurrentTime() > LastRenderTimeForTexture) ? float( FApp::GetCurrentTime() - LastRenderTimeForTexture ) : 0.0f;
		MinDistance = MAX_STREAMINGDISTANCE;
		bForceFullyLoad = Texture->ShouldMipLevelsBeForcedResident() || (ForceLoadRefCount > 0);
		TextureLODBias = Texture->GetCachedLODBias();
		bInFlight = false;
		bReadyForStreaming = IsStreamingTexture( Texture ) && IsReadyForStreaming( Texture );
		NumCinematicMipLevels = Texture->bUseCinematicMipLevels ? Texture->NumCinematicMipLevels : 0;
	}

	ETextureStreamingType GetStreamingType() const
	{
		if ( bUsesForcedHeuristics || bForceFullyLoad )
		{
			return StreamType_Forced;
		}
		else if ( bUsesDynamicHeuristics )
		{
			return StreamType_Dynamic;
		}
		else if ( bUsesStaticHeuristics )
		{
			return StreamType_Static;
		}
		else if ( bUsesOrphanedHeuristics )
		{
			return StreamType_Orphaned;
		}
		else if ( bUsesLastRenderHeuristics )
		{
			return StreamType_LastRenderTime;
		}
		return StreamType_Other;
	}

	/**
	 * Checks whether a UTexture2D is supposed to be streaming.
	 * @param Texture	Texture to check
	 * @return			true if the UTexture2D is supposed to be streaming
	 */
	static bool IsStreamingTexture( const UTexture2D* Texture2D )
	{
		return Texture2D && Texture2D->bIsStreamable &&
			Texture2D->NeverStream == false && Texture2D->GetNumMips() > UTexture2D::GetMinTextureResidentMipCount();
	}

	/**
	 * Checks whether a UTexture2D is ready for streaming.
	 *
	 * @param Texture	Texture to check
	 * @return			true if the UTexture2D is ready to be streamed in or out
	 */
	static bool IsReadyForStreaming( UTexture2D* Texture )
	{
		return Texture->IsReadyForStreaming();
	}

	/**
	 * Checks whether a UTexture2D is a streaming lightmap or shadowmap.
	 *
	 * @param Texture	Texture to check
	 * @return			true if the UTexture2D is a streaming lightmap or shadowmap
	 */
	static bool IsStreamingLightmap( UTexture2D* Texture )
	{
		ULightMapTexture2D* Lightmap = Cast<ULightMapTexture2D>(Texture);
		UShadowMapTexture2D* Shadowmap = Cast<UShadowMapTexture2D>(Texture);

		if ( Lightmap && Lightmap->LightmapFlags & LMF_Streamed )
		{
			return true;
		}
		else if ( Shadowmap && Shadowmap->ShadowmapFlags & SMF_Streamed )
		{
			return true;
		}
		return false;
	}

	/**
	 * Returns the amount of memory used by the texture given a specified number of mip-maps, in bytes.
	 *
	 * @param MipCount	Number of mip-maps to account for
	 * @return			Total amount of memory used for the specified mip-maps, in bytes
	 */
	int32 GetSize( int32 InMipCount ) const
	{
		check(InMipCount >= 0 && InMipCount <= MAX_TEXTURE_MIP_COUNT);
		return TextureSizes[ InMipCount - 1 ];
	}

	/**
	 * Calculates a priority value for the texture. Higher value means more important.
	 * @return		Priority value
	 */
	float CalcPriority( )
	{
		if (LODGroup != TEXTUREGROUP_Terrain_Heightmap)
		{
	//		float DistanceFactor = 1.0f - FMath::Clamp( MinDistance, 1.0f, MAX_STREAMINGDISTANCE ) / MAX_STREAMINGDISTANCE;
			float DistanceFactor = 1 - FMath::Sqrt( FMath::Clamp( MinDistance, 1.0f, MAX_STREAMINGDISTANCE ) / MAX_STREAMINGDISTANCE );
	//		float MipFactor = WantedMips - ResidentMips;
			float MipFactor = float(WantedMips) / float(MAX_TEXTURE_MIP_COUNT);
			float TimeFactor = GStreamWithTimeFactor ? (FMath::Clamp( LastRenderTime, 1.0f, MAX_LASTRENDERTIME ) / MAX_LASTRENDERTIME) : 0.0f;
			float Priority = MipFactor + DistanceFactor * (1.0f - TimeFactor * 0.5f) + bForceFullyLoad * 100.0f;
			return Priority;
		}
		else
		{
			// Because heightmaps are technically geometry, it is absolutely imperative that they are streamed in
			return 1000.0f;
		}
	}

	/**
	 * Not thread-safe: Sets the streaming index in the corresponding UTexture2D.
	 * @param ArrayIndex	Index into the FStreamingManagerTexture::StreamingTextures array
	 */
	void SetStreamingIndex( int32 ArrayIndex )
	{
		Texture->StreamingIndex = ArrayIndex;
	}

	/** Texture to manage. */
	UTexture2D*		Texture;

	/** Cached number of mip-maps in the UTexture2D mip array (including the base mip) */
	int32			MipCount;
	/** Cached number of mip-maps in memory (including the base mip) */
	int32			ResidentMips;
	/** Cached number of mip-maps requested by the streaming system. */
	int32			RequestedMips;
	/** Cached number of mip-maps we would like to have in memory. */
	int32			WantedMips;
	/** Largest screen size occupied by this texture, counting dynamic primitives only. Call FFloatMipLevel::FromScreenSizeInTexels() to get DynamicWantedMips. */
	float			DynamicScreenSize;
#if STATS
	/** Legacy: Same as WantedMips, but not clamped by fudge factor. */
	int32			PerfectWantedMips;
	/** Same as MaxAllowedMips, but not clamped by the -reducepoolsize command line option. */
	int32			MaxAllowedOptimalMips;
	/** Most number of mip-levels this texture has ever had resident in memory. */
	int32			MostResidentMips;
#endif
	/** Minimum number of mip-maps that this texture must keep in memory. */
	int32			MinAllowedMips;
	/** Maximum number of mip-maps that this texture can have in memory. */
	int32			MaxAllowedMips;
	/** Cached memory sizes for each possible mipcount. */
	int32			TextureSizes[MAX_TEXTURE_MIP_COUNT + 1];
	/** Ref-count for how many ULevels want this texture fully-loaded. */
	int32			ForceLoadRefCount;

	/** Cached texture group. */
	TextureGroup	LODGroup;
	/** Cached LOD bias. */
	int32			TextureLODBias;
	/** Cached number of mipmaps that are not allowed to stream. */
	int32			NumNonStreamingMips;
	/** Cached number of cinematic (high-resolution) mip-maps. Normally not streamed in, unless the texture is forcibly fully loaded. */
	int32			NumCinematicMipLevels;

	/** Last time this texture was rendered, 0-based from GStartTime. */
	float			LastRenderTime;
	/** Distance to the closest instance of this texture, in world space units. Defaults to a specific value for non-staticmesh textures. */
	float			MinDistance;
	/** Squared distance to the closest dynamic instance of this texture. */
	float			DynamicMinDistanceSq;
	/** If non-zero, the most recent time an instance location was removed for this texture. */
	double			InstanceRemovedTimestamp;

	/** Set to FApp::GetCurrentTime() every time LastRenderTimeRefCount is modified. */
	double			LastRenderTimeRefCountTimestamp;
	/** Current number of instances that need LRT heuristics for this texture. */
	int32			LastRenderTimeRefCount;

	/**
	 * Temporary boost of the streaming distance factor.
	 * This factor is automatically reset to 1.0 after it's been used for mip-calculations.
	 */
	float			BoostFactor;

	/** Whether the texture should be forcibly fully loaded. */
	uint32			bForceFullyLoad : 1;
	/** Whether the texture is ready to be streamed in/out (cached from IsReadyForStreaming()). */
	uint32			bReadyForStreaming : 1;
	/** Whether the texture is currently being streamed in/out. */
	uint32			bInFlight : 1;
	/** Whether this is a streaming lightmap or shadowmap (cached from IsStreamingLightmap()). */
	uint32			bIsStreamingLightmap : 1;
	/** Whether this texture uses StaticTexture heuristics. */
	uint32			bUsesStaticHeuristics : 1;
	/** Whether this texture uses DynamicTexture heuristics. */
	uint32			bUsesDynamicHeuristics : 1;
	/** Whether this texture uses LastRenderTime heuristics. */
	uint32			bUsesLastRenderHeuristics : 1;
	/** Whether this texture uses ForcedIntoMemory heuristics. */
	uint32			bUsesForcedHeuristics : 1;
	/** Whether this texture uses the OrphanedTexture heuristics. */
	uint32			bUsesOrphanedHeuristics : 1;
	/** Whether this texture has been replaced in a material and all primitives using it call NotifyPrimitiveUpdated(). */
	uint32			bNeedPrimitiveUpdate : 1;
};

// ----------------------------------------------------------------------

FFloatMipLevel::FFloatMipLevel() 
{
	Invalidate();
}

int32 FFloatMipLevel::ComputeMip(const FStreamingTexture* StreamingTexture, float MipBias, bool bOptimal) const
{	
	check(MipBias >= 0);

	int32 MinAllowedMips = 1;
	int32 MaxAllowedMips = 16;

	if(StreamingTexture)
	{
		MinAllowedMips = StreamingTexture->MinAllowedMips;
		MaxAllowedMips = 
#if STATS
			bOptimal ? StreamingTexture->MaxAllowedOptimalMips : 
#endif
			StreamingTexture->MaxAllowedMips;
		// Landscape textures should not be affected by MipBias
		if (StreamingTexture->LODGroup == TEXTUREGROUP_Terrain_Heightmap || 
			StreamingTexture->LODGroup == TEXTUREGROUP_Terrain_Weightmap)
		{
			MipBias = 0.0f;
		}
	}

	// bias the clamped mip level
	int32 LocalMipLevel = (int32)(FMath::Min(MipLevel, (float)MaxAllowedMips) - MipBias);

	LocalMipLevel = FMath::Max(LocalMipLevel, MinAllowedMips);

	return LocalMipLevel;
}

bool FFloatMipLevel::operator>=(const FFloatMipLevel& rhs) const
{
	return MipLevel >= rhs.MipLevel;
}

FFloatMipLevel FFloatMipLevel::FromScreenSizeInTexels(float ScreenSizeInTexels)
{
	//check(ScreenSizeInTexels >= 0);  @todo: should we figure out why this is negative?
	const float ScreenSizeInTexelsClamped = FMath::Max(0.f, ScreenSizeInTexels);

	FFloatMipLevel Ret;

	// int
//	Ret.MipLevel = 1 + FMath::CeilLogTwo( FMath::TruncToInt(ScreenSizeInTexels) );
	// float
	Ret.MipLevel = 1.0f + FMath::Log2(ScreenSizeInTexelsClamped);

	return Ret;
}

FFloatMipLevel FFloatMipLevel::FromMipLevel(int32 InMipLevel)
{
	FFloatMipLevel Ret;

	Ret.MipLevel = (float)InMipLevel;

	return Ret;
}

void FFloatMipLevel::Invalidate()
{
	MipLevel = -1.0f;
}

/*-----------------------------------------------------------------------------
	Texture tracking.
-----------------------------------------------------------------------------*/

/** Turn on ENABLE_TEXTURE_TRACKING and setup GTrackedTextures to track specific textures through the streaming system. */
#define ENABLE_TEXTURE_TRACKING !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define ENABLE_TEXTURE_LOGGING 1
#if ENABLE_TEXTURE_TRACKING
struct FTrackedTextureEvent
{
	FTrackedTextureEvent( TCHAR* InTextureName=NULL )
	:	TextureName(InTextureName)
	,	NumResidentMips(0)
	,	NumRequestedMips(0)
	,	WantedMips(0)
	,	StreamingStatus(0)
	,	Timestamp(0.0f)
	,	StreamType(StreamType_Other)
	,	BoostFactor(1.0f)
	{
	}
	FTrackedTextureEvent( const FString& InTextureNameString )
	:	NumResidentMips(0)
	,	NumRequestedMips(0)
	,	WantedMips(0)
	,	StreamingStatus(0)
	,	Timestamp(0.0f)
	,	StreamType(StreamType_Other)
	,	BoostFactor(1.0f)
	{
		TextureName = new TCHAR[InTextureNameString.Len() + 1];
		FMemory::Memcpy( TextureName, *InTextureNameString, (InTextureNameString.Len() + 1)*sizeof(TCHAR) );
	}
	/** Partial name of the texture (not case-sensitive). */
	TCHAR*			TextureName;
	/** Number of mip-levels currently in memory. */
	int32			NumResidentMips;
	/** Number of mip-levels requested. */
	int32			NumRequestedMips;
	/** Number of wanted mips. */
	int32			WantedMips;
	/** Number of wanted mips, as calculated by the heuristics for dynamic primitives. */
	FFloatMipLevel	DynamicWantedMips;
	/** Streaming status. */
	int32			StreamingStatus;
	/** Timestamp, in seconds from startup. */
	float			Timestamp;
	/** Which streaming heuristic this texture uses. */
	ETextureStreamingType	StreamType;
	/** Currently used boost factor for the streaming distance. */
	float			BoostFactor;
};
/** List of textures to track (using stristr for name comparison). */
TArray<FString> GTrackedTextureNames;
bool GTrackedTexturesInitialized = false;
#define NUM_TRACKEDTEXTUREEVENTS 512
FTrackedTextureEvent GTrackedTextureEvents[NUM_TRACKEDTEXTUREEVENTS];
int32 GTrackedTextureEventIndex = -1;
TArray<FTrackedTextureEvent> GTrackedTextures;

/**
 * Initializes the texture tracking. Called when GTrackedTexturesInitialized is false.
 */
void TrackTextureInit()
{
	if ( GConfig && GConfig->Num() > 0 )
	{
		GTrackedTextureNames.Empty();
		GTrackedTexturesInitialized = true;
		GConfig->GetArray( TEXT("TextureTracking"), TEXT("TextureName"), GTrackedTextureNames, GEngineIni );
	}
}

/**
 * Adds a (partial) texture name to track in the streaming system and updates the .ini setting.
 *
 * @param TextureName	Partial name of a new texture to track (not case-sensitive)
 * @return				true if the name was added
 */
bool TrackTexture( const FString& TextureName )
{
	if ( GConfig && TextureName.Len() > 0 )
	{
		for ( int32 TrackedTextureIndex=0; TrackedTextureIndex < GTrackedTextureNames.Num(); ++TrackedTextureIndex )
		{
			const FString& TrackedTextureName = GTrackedTextureNames[TrackedTextureIndex];
			if ( FCString::Stricmp(*TextureName, *TrackedTextureName) == 0 )
			{
				return false;	
			}
		}

		GTrackedTextureNames.Add( *TextureName );
		GConfig->SetArray( TEXT("TextureTracking"), TEXT("TextureName"), GTrackedTextureNames, GEngineIni );
		return true;
	}
	return false;
}

/**
 * Removes a texture name from being tracked in the streaming system and updates the .ini setting.
 * The name must match an existing tracking name, but isn't case-sensitive.
 *
 * @param TextureName	Name of a new texture to stop tracking (not case-sensitive)
 * @return				true if the name was added
 */
bool UntrackTexture( const FString& TextureName )
{
	if ( GConfig && TextureName.Len() > 0 )
	{
		int32 TrackedTextureIndex = 0;
		for ( ; TrackedTextureIndex < GTrackedTextureNames.Num(); ++TrackedTextureIndex )
		{
			const FString& TrackedTextureName = GTrackedTextureNames[TrackedTextureIndex];
			if ( FCString::Stricmp(*TextureName, *TrackedTextureName) == 0 )
			{
				break;
			}
		}
		if ( TrackedTextureIndex < GTrackedTextureNames.Num() )
		{
			GTrackedTextureNames.RemoveAt( TrackedTextureIndex );
			GConfig->SetArray( TEXT("TextureTracking"), TEXT("TextureName"), GTrackedTextureNames, GEngineIni );

			return true;
		}
	}
	return false;
}

/**
 * Lists all currently tracked texture names in the specified log.
 *
 * @param Ar			Desired output log
 * @param NumTextures	Maximum number of tracked texture names to output. Outputs all if NumTextures <= 0.
 */
void ListTrackedTextures( FOutputDevice& Ar, int32 NumTextures )
{
	NumTextures = (NumTextures > 0) ? FMath::Min(NumTextures, GTrackedTextureNames.Num()) : GTrackedTextureNames.Num();
	for ( int32 TrackedTextureIndex=0; TrackedTextureIndex < NumTextures; ++TrackedTextureIndex )
	{
		const FString& TrackedTextureName = GTrackedTextureNames[TrackedTextureIndex];
		Ar.Log( TrackedTextureName );
	}
	Ar.Logf( TEXT("%d textures are being tracked."), NumTextures );
}

/**
 * Checks a texture and tracks it if its name contains any of the tracked textures names (GTrackedTextureNames).
 *
 * @param Texture						Texture to check
 * @param bForceMipLEvelsToBeResident	Whether all mip-levels in the texture are forced to be resident
 * @param Manager                       can be 0
 */
bool TrackTextureEvent( FStreamingTexture* StreamingTexture, UTexture2D* Texture, bool bForceMipLevelsToBeResident, const FStreamingManagerTexture* Manager)
{
	// Whether the texture is currently being destroyed
	const bool bIsDestroying = (StreamingTexture == 0);
	const bool bEnableLogging = ENABLE_TEXTURE_LOGGING;

	// Initialize the tracking system, if necessary.
	if ( GTrackedTexturesInitialized == false )
	{
		TrackTextureInit();
	}

	float MipBias = Manager ? Manager->ThreadSettings.MipBias : 0;

	int32 NumTrackedTextures = GTrackedTextureNames.Num();
	if ( NumTrackedTextures )
	{
		// See if it matches any of the texture names that we're tracking.
		FString TextureNameString = Texture->GetFullName();
		const TCHAR* TextureName = *TextureNameString;
		for ( int32 TrackedTextureIndex=0; TrackedTextureIndex < NumTrackedTextures; ++TrackedTextureIndex )
		{
			const FString& TrackedTextureName = GTrackedTextureNames[TrackedTextureIndex];
			if ( FCString::Stristr(TextureName, *TrackedTextureName) != NULL )
			{
				if ( bEnableLogging )
				{
					// Find the last event for this particular texture.
					FTrackedTextureEvent* LastEvent = NULL;
					for ( int32 LastEventIndex=0; LastEventIndex < GTrackedTextures.Num(); ++LastEventIndex )
					{
						FTrackedTextureEvent* Event = &GTrackedTextures[LastEventIndex];
						if ( FCString::Strcmp(TextureName, Event->TextureName) == 0 )
						{
							LastEvent = Event;
							break;
						}
					}
					// Didn't find any recorded event?
					if ( LastEvent == NULL )
					{
						int32 NewIndex = GTrackedTextures.Add(FTrackedTextureEvent(TextureNameString));
						LastEvent = &GTrackedTextures[NewIndex];
					}

					int32 StreamingStatus	= Texture->PendingMipChangeRequestStatus.GetValue();
					int32 WantedMips		= Texture->RequestedMips;
					FFloatMipLevel DynamicWantedMips;
					float BoostFactor		= 1.0f;
					ETextureStreamingType StreamType = bForceMipLevelsToBeResident ? StreamType_Forced : StreamType_Other;
					if ( StreamingTexture )
					{
						WantedMips			= StreamingTexture->WantedMips;
						DynamicWantedMips	= FFloatMipLevel::FromScreenSizeInTexels(StreamingTexture->DynamicScreenSize);
						StreamType			= StreamingTexture->GetStreamingType();
						BoostFactor			= StreamingTexture->BoostFactor;
					}

					if ( LastEvent->NumResidentMips != Texture->ResidentMips ||
						 LastEvent->NumRequestedMips != Texture->RequestedMips ||
						 LastEvent->StreamingStatus != StreamingStatus ||
						 LastEvent->WantedMips != WantedMips ||
						 LastEvent->DynamicWantedMips.ComputeMip(StreamingTexture, MipBias, false) != DynamicWantedMips.ComputeMip(StreamingTexture, MipBias, false) ||
						 LastEvent->StreamType != StreamType ||
						 LastEvent->BoostFactor != BoostFactor ||
						 bIsDestroying )
					{
						GTrackedTextureEventIndex		= (GTrackedTextureEventIndex + 1) % NUM_TRACKEDTEXTUREEVENTS;
						FTrackedTextureEvent& NewEvent	= GTrackedTextureEvents[GTrackedTextureEventIndex];
						NewEvent.TextureName			= LastEvent->TextureName;
						NewEvent.NumResidentMips		= LastEvent->NumResidentMips	= Texture->ResidentMips;
						NewEvent.NumRequestedMips		= LastEvent->NumRequestedMips	= Texture->RequestedMips;
						NewEvent.WantedMips				= LastEvent->WantedMips			= WantedMips;
						NewEvent.DynamicWantedMips		= LastEvent->DynamicWantedMips	= DynamicWantedMips;
						NewEvent.StreamingStatus		= LastEvent->StreamingStatus	= StreamingStatus;
						NewEvent.Timestamp				= LastEvent->Timestamp			= float(FPlatformTime::Seconds() - GStartTime);
						NewEvent.StreamType				= LastEvent->StreamType			= StreamType;
						NewEvent.BoostFactor			= LastEvent->BoostFactor		= BoostFactor;
						UE_LOG(LogContentStreaming, Log, TEXT("Texture: \"%s\", ResidentMips: %d/%d, RequestedMips: %d, WantedMips: %d, DynamicWantedMips: %d, StreamingStatus: %d, StreamType: %s, Boost: %.1f (%s)"),
							TextureName, LastEvent->NumResidentMips, Texture->GetNumMips(), bIsDestroying ? 0 : LastEvent->NumRequestedMips,
							LastEvent->WantedMips, LastEvent->DynamicWantedMips.ComputeMip(StreamingTexture, MipBias, false), LastEvent->StreamingStatus, GStreamTypeNames[StreamType],
							BoostFactor, bIsDestroying ? TEXT("DESTROYED") : TEXT("updated") );
					}
				}
				return true;
			}
		}
	}
	return false;
}
#else
bool TrackTexture( const FString& /*TextureName*/ )
{
	return false;
}
bool UntrackTexture( const FString& /*TextureName*/ )
{
	return false;
}
void ListTrackedTextures( FOutputDevice& /*Ar*/, int32 /*NumTextures*/ )
{
}
bool TrackTextureEvent( FStreamingTexture* StreamingTexture, UTexture2D* Texture, bool bForceMipLevelsToBeResident, const FStreamingManagerTexture* Manager )
{
	return false;
}
#endif

/*-----------------------------------------------------------------------------
	Asynchronous texture streaming.
-----------------------------------------------------------------------------*/

#if STATS
struct FTextureStreamingStats
{
	FTextureStreamingStats( UTexture2D* InTexture2D, ETextureStreamingType InType, int32 InResidentMips, int32 InWantedMips, int32 InMostResidentMips, int32 InResidentSize, int32 InWantedSize, int32 InMaxSize, int32 InMostResidentSize, float InBoostFactor, float InPriority, int32 InTextureIndex )
	:	TextureName( InTexture2D->GetFullName() )
	,	SizeX( InTexture2D->GetSizeX() )
	,	SizeY( InTexture2D->GetSizeY() )
	,	NumMips( InTexture2D->GetNumMips() )
	,	LODBias( InTexture2D->GetCachedLODBias() )
	,	LastRenderTime( FMath::Clamp( InTexture2D->Resource ? (FApp::GetCurrentTime() - InTexture2D->Resource->LastRenderTime) : 1000000.0, 0.0, 1000000.0) )
	,	StreamType( InType )
	,	ResidentMips( InResidentMips )
	,	WantedMips( InWantedMips )
	,	MostResidentMips( InMostResidentMips )
	,	ResidentSize( InResidentSize )
	,	WantedSize( InWantedSize )
	,	MaxSize( InMaxSize )
	,	MostResidentSize( InMostResidentSize )
	,	BoostFactor( InBoostFactor )
	,	Priority( InPriority )
	,	TextureIndex( InTextureIndex )
	{
	}
	/** Mirror of UTexture2D::GetName() */
	FString		TextureName;
	/** Mirror of UTexture2D::SizeX */
	int32		SizeX;
	/** Mirror of UTexture2D::SizeY */
	int32		SizeY;
	/** Mirror of UTexture2D::Mips.Num() */
	int32		NumMips;
	/** Mirror of UTexture2D::GetCachedLODBias() */
	int32		LODBias;
	/** How many seconds since it was last rendered: FApp::GetCurrentTime() - UTexture2D::Resource->LastRenderTime */
	float		LastRenderTime;
	/** What streaming heuristics were primarily used for this texture. */
	ETextureStreamingType	StreamType;
	/** Number of resident mip levels. */
	int32		ResidentMips;
	/** Number of wanted mip levels. */
	int32		WantedMips;
	/** Most number of mip-levels this texture has ever had resident in memory. */
	int32		MostResidentMips;
	/** Number of bytes currently in memory. */
	int32		ResidentSize;
	/** Number of bytes we want in memory. */
	int32		WantedSize;
	/** Number of bytes we could potentially stream in. */
	int32		MaxSize;
	/** Most number of bytes this texture has ever had resident in memory. */
	int32		MostResidentSize;
	/** Temporary boost of the streaming distance factor. */
	float		BoostFactor;
	/** Texture priority */
	float		Priority;
	/** Index into the FStreamingManagerTexture::StreamingTextures array. */
	int32		TextureIndex;
};
#endif

/**
 * Helper struct for temporary information for one frame of processing texture streaming.
 */
struct FStreamingContext
{
	FStreamingContext( bool bProcessEverything, UTexture2D* IndividualStreamingTexture, bool bInCollectTextureStats )
	{
		Reset( bProcessEverything, IndividualStreamingTexture, bInCollectTextureStats );
	}

	/**
	 * Initializes all variables for the one frame.
	 * @param bProcessEverything			If true, process all resources with no throttling limits
	 * @param IndividualStreamingTexture	A specific texture to be fully processed this frame, or NULL
	 * @param bInCollectTextureStats		Whether to fill in the TextureStats array this frame
	 */
	void Reset( bool bProcessEverything, UTexture2D* IndividualStreamingTexture, bool bInCollectTextureStats )
	{
		bCollectTextureStats							= bInCollectTextureStats;
		ThisFrameTotalRequestSize						= 0;
		ThisFrameTotalLightmapRequestSize				= 0;
		ThisFrameNumStreamingTextures					= 0;
		ThisFrameNumRequestsInCancelationPhase			= 0;
		ThisFrameNumRequestsInUpdatePhase				= 0;
		ThisFrameNumRequestsInFinalizePhase				= 0;
		ThisFrameTotalIntermediateTexturesSize			= 0;
		ThisFrameNumIntermediateTextures				= 0;
		ThisFrameTotalStreamingTexturesSize				= 0;
		ThisFrameTotalStreamingTexturesMaxSize			= 0;
		ThisFrameTotalLightmapMemorySize				= 0;
		ThisFrameTotalLightmapDiskSize					= 0;
		ThisFrameTotalMipCountIncreaseRequestsInFlight	= 0;
		ThisFrameOptimalWantedSize						= 0;
		ThisFrameTotalStaticTextureHeuristicSize		= 0;
		ThisFrameTotalDynamicTextureHeuristicSize		= 0;
		ThisFrameTotalLastRenderHeuristicSize			= 0;
		ThisFrameTotalForcedHeuristicSize				= 0;

		STAT( TextureStats.Empty() );

		AllocatedMemorySize	= INDEX_NONE;
		AvailableMemorySize	= INDEX_NONE;
		PendingMemoryAdjustment = INDEX_NONE;
		
		// Available texture memory, if supported by RHI. This stat is async as the renderer allocates the memory in its own thread so we
		// only query once and roughly adjust the values as needed.
		FTextureMemoryStats Stats;
		RHIGetTextureMemoryStats(Stats);
		
		bRHISupportsMemoryStats = Stats.IsUsingLimitedPoolSize();

		// Update stats if supported.
		if( bRHISupportsMemoryStats )
		{
			AllocatedMemorySize	= Stats.AllocatedMemorySize;
			AvailableMemorySize	= Stats.ComputeAvailableMemorySize();
			PendingMemoryAdjustment = Stats.PendingMemoryAdjustment;
	
			// set total size for the pool (used to available)
			SET_MEMORY_STAT(STAT_TexturePoolAllocatedSize, AllocatedMemorySize);
			SET_MEMORY_STAT(STAT_TexturePoolSize, Stats.TexturePoolSize);
			SET_MEMORY_STAT(MCR_TexturePool, Stats.TexturePoolSize);
		}
		else
		{
			SET_MEMORY_STAT(STAT_TexturePoolAllocatedSize,0);
			SET_MEMORY_STAT(STAT_TexturePoolSize, 0);
			SET_MEMORY_STAT(MCR_TexturePool, 0);
		}
	}

	/**
	 * Adds in the stats from another context.
	 *
	 * @param Other		Context to add stats from
	 */
	void AddStats( const FStreamingContext& Other )
	{
		ThisFrameTotalRequestSize						+= Other.ThisFrameTotalRequestSize;						
		ThisFrameTotalLightmapRequestSize				+= Other.ThisFrameTotalLightmapRequestSize;
		ThisFrameNumStreamingTextures					+= Other.ThisFrameNumStreamingTextures;
		ThisFrameNumRequestsInCancelationPhase			+= Other.ThisFrameNumRequestsInCancelationPhase;
		ThisFrameNumRequestsInUpdatePhase				+= Other.ThisFrameNumRequestsInUpdatePhase;
		ThisFrameNumRequestsInFinalizePhase				+= Other.ThisFrameNumRequestsInFinalizePhase;
		ThisFrameTotalIntermediateTexturesSize			+= Other.ThisFrameTotalIntermediateTexturesSize;
		ThisFrameNumIntermediateTextures				+= Other.ThisFrameNumIntermediateTextures;
		ThisFrameTotalStreamingTexturesSize				+= Other.ThisFrameTotalStreamingTexturesSize;
		ThisFrameTotalStreamingTexturesMaxSize			+= Other.ThisFrameTotalStreamingTexturesMaxSize;
		ThisFrameTotalLightmapMemorySize				+= Other.ThisFrameTotalLightmapMemorySize;
		ThisFrameTotalLightmapDiskSize					+= Other.ThisFrameTotalLightmapDiskSize;
		ThisFrameTotalMipCountIncreaseRequestsInFlight	+= Other.ThisFrameTotalMipCountIncreaseRequestsInFlight;
		ThisFrameOptimalWantedSize						+= Other.ThisFrameOptimalWantedSize;
		ThisFrameTotalStaticTextureHeuristicSize		+= Other.ThisFrameTotalStaticTextureHeuristicSize;
		ThisFrameTotalDynamicTextureHeuristicSize		+= Other.ThisFrameTotalDynamicTextureHeuristicSize;
		ThisFrameTotalLastRenderHeuristicSize			+= Other.ThisFrameTotalLastRenderHeuristicSize;
		ThisFrameTotalForcedHeuristicSize				+= Other.ThisFrameTotalForcedHeuristicSize;
		bCollectTextureStats							= bCollectTextureStats || Other.bCollectTextureStats;
		STAT( TextureStats.Append( Other.TextureStats ) );
	}

	/** Whether the platform RHI supports texture memory stats. */
	bool bRHISupportsMemoryStats;
	/** Currently allocated number of bytes in the texture pool, or INDEX_NONE if bRHISupportsMemoryStats is false. */
	int64 AllocatedMemorySize;
	/** Currently available number of bytes in the texture pool, or INDEX_NONE if bRHISupportsMemoryStats is false. */
	int64 AvailableMemorySize;
	/** Pending Adjustments to allocated texture memory, due to async reallocations, etc. */
	int64 PendingMemoryAdjustment;
	/** Whether to fill in TextureStats this frame. */
	bool bCollectTextureStats;

#if STATS
	/** Stats for all textures. */
	TArray<FTextureStreamingStats>	TextureStats;
#endif

	// Stats for this frame.
	uint64 ThisFrameTotalRequestSize;
	uint64 ThisFrameTotalLightmapRequestSize;
	uint64 ThisFrameNumStreamingTextures;
	uint64 ThisFrameNumRequestsInCancelationPhase;
	uint64 ThisFrameNumRequestsInUpdatePhase;
	uint64 ThisFrameNumRequestsInFinalizePhase;
	uint64 ThisFrameTotalIntermediateTexturesSize;
	uint64 ThisFrameNumIntermediateTextures;
	uint64 ThisFrameTotalStreamingTexturesSize;
	uint64 ThisFrameTotalStreamingTexturesMaxSize;
	uint64 ThisFrameTotalLightmapMemorySize;
	uint64 ThisFrameTotalLightmapDiskSize;
	uint64 ThisFrameTotalMipCountIncreaseRequestsInFlight;
	uint64 ThisFrameOptimalWantedSize;
	/** Number of bytes using StaticTexture heuristics this frame, currently in memory. */
	uint64 ThisFrameTotalStaticTextureHeuristicSize;
	/** Number of bytes using DynmicTexture heuristics this frame, currently in memory. */
	uint64 ThisFrameTotalDynamicTextureHeuristicSize;
	/** Number of bytes using LastRenderTime heuristics this frame, currently in memory. */
	uint64 ThisFrameTotalLastRenderHeuristicSize;
	/** Number of bytes using ForcedIntoMemory heuristics this frame, currently in memory. */
	uint64 ThisFrameTotalForcedHeuristicSize;
};


/** Async work class for calculating priorities for all textures. */
// this could implement a better abandon, but give how it is used, it does that anyway via the abort mechanism
class FAsyncTextureStreaming : public FNonAbandonableTask
{
public:
	FAsyncTextureStreaming( FStreamingManagerTexture* InStreamingManager )
	:	StreamingManager( *InStreamingManager )
	,	ThreadContext( false, NULL, false )
	,	bAbort( false )
	{
		Reset(false);
	}

	/** Resets the state to start a new async job. */
	void Reset( bool bCollectTextureStats )
	{
		bAbort = false;
		ThreadContext.Reset( false, NULL, bCollectTextureStats );
		ThreadStats.Reset();
		PrioritizedTextures.Empty( StreamingManager.StreamingTextures.Num() );
	}

	/** Notifies the async work that it should abort the thread ASAP. */
	void Abort()
	{
		bAbort = true;
	}

	/** Whether the async work is being aborted. Can be used in conjunction with IsDone() to see if it has finished. */
	bool IsAborted() const
	{
		return bAbort;
	}

	/** Statistics for the async work. */
	struct FAsyncStats
	{
		FAsyncStats()
		{
			Reset();
		}
		/** Resets the statistics to zero. */
		void Reset()
		{
			TotalResidentSize = 0;
			STAT( TotalRequiredSize = 0 );
			TempStreamingSize = 0;
			PendingStreamInSize = 0;
			PendingStreamOutSize = 0;
			WantedInSize = 0;
			WantedOutSize = 0;
			NumWantingTextures = 0;
		}
		/** Total number of bytes currently in memory */
		int64 TotalResidentSize;
		/** Total number of bytes required, using PerfectWantedMips */
		STAT( int64 TotalRequiredSize );
		/** Currently being streamed in, in bytes. */
		SIZE_T PendingStreamInSize;
		/** Currently being streamed out, in bytes. */
		SIZE_T PendingStreamOutSize;
		/** How much we want to stream in, in bytes. */
		int64 WantedInSize;
		/** How much we want to stream out, in bytes.  */
		int64 WantedOutSize;
		/** How much memory we're temporarily using for streaming in/out, in bytes. */
		int64 TempStreamingSize;
		/** Number of textures that want more mips. */
		int64 NumWantingTextures;
	};

	/** Returns the resulting priorities, matching the FStreamingManagerTexture::StreamingTextures array. */
	const TArray<FTexturePriority>& GetPrioritizedTextures() const
	{
		return PrioritizedTextures;
	}

	/** Returns the context (temporary info) used for the async work. */
	const FStreamingContext& GetContext() const
	{
		return ThreadContext;
	}

	/** Returns the thread statistics. */
	const FAsyncStats& GetStats() const
	{
		return ThreadStats;
	}

private:
	friend class FAsyncTask<FAsyncTextureStreaming>;
	/** Performs the async work. */
	void DoWork()
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FAsyncTextureStreaming::DoWork"), STAT_AsyncTextureStreaming_DoWork, STATGROUP_StreamingDetails);
		PrioritizedTextures.Empty( StreamingManager.StreamingTextures.Num() );

		// Calculate DynamicWantedMips and DynamicMinDistanceSq for all dynamic textures.
		//@TODO: This is not thread-safe because it looks up UTexture2D to get to the FStreamingTexture...
//		StreamingManager.CalcDynamicWantedMips();

		// Number of textures that want more mips.
		ThreadStats.NumWantingTextures = 0;

		for ( int32 Index=0; Index < StreamingManager.StreamingTextures.Num() && !IsAborted(); ++Index )
		{
			FStreamingTexture& StreamingTexture = StreamingManager.StreamingTextures[ Index ];

			int64 ResidentTextureSize = StreamingTexture.GetSize( StreamingTexture.ResidentMips );
			ThreadStats.TotalResidentSize += ResidentTextureSize;

			StreamingTexture.bUsesStaticHeuristics = false;
			StreamingTexture.bUsesDynamicHeuristics = (StreamingTexture.DynamicScreenSize > 0.0f) ? true : false;
			StreamingTexture.bUsesLastRenderHeuristics = false;
			StreamingTexture.bUsesForcedHeuristics = false;
			StreamingTexture.bUsesOrphanedHeuristics = false;

			if ( StreamingTexture.bReadyForStreaming )
			{
				// Figure out max number of miplevels allowed by LOD code.
				StreamingManager.CalcMinMaxMips( StreamingTexture );

				// Determine how many mips this texture should have in memory.
				StreamingManager.CalcWantedMips( StreamingTexture );

				if ( StreamingTexture.WantedMips > StreamingTexture.ResidentMips )
				{
					ThreadStats.NumWantingTextures++;
				}

				bool bTrackedTexture = TrackTextureEvent( &StreamingTexture, StreamingTexture.Texture, StreamingTexture.bForceFullyLoad, &StreamingManager );

				// Add to sort list, if it wants to stream in or could potentially stream out.
				if ( StreamingTexture.WantedMips > StreamingTexture.ResidentMips || StreamingTexture.ResidentMips > StreamingTexture.MinAllowedMips )
				{
					FTexturePriority* TexturePriority = new (PrioritizedTextures) FTexturePriority( StreamingTexture.CalcPriority(), Index );
				}

				// Accumulate streaming numbers.
				int64 WantedTextureSize = StreamingTexture.GetSize( StreamingTexture.WantedMips );
				if ( StreamingTexture.bInFlight )
				{
					int64 RequestedTextureSize = StreamingTexture.GetSize( StreamingTexture.RequestedMips );
					ThreadStats.TempStreamingSize += ResidentTextureSize;	//@TODO: 0 for in-place reallocations.
					if ( StreamingTexture.RequestedMips > StreamingTexture.ResidentMips )
					{
						ThreadStats.PendingStreamInSize += FMath::Abs(RequestedTextureSize - ResidentTextureSize);
					}
					else
					{
						ThreadStats.PendingStreamOutSize += FMath::Abs(RequestedTextureSize - ResidentTextureSize);
					}
				}
				else
				{
					if ( StreamingTexture.WantedMips > StreamingTexture.ResidentMips )
					{
						ThreadStats.WantedInSize += FMath::Abs(WantedTextureSize - ResidentTextureSize);
					}
					else
					{
						// Counting on shrinking reallocation.
						ThreadStats.WantedOutSize += FMath::Abs(WantedTextureSize - ResidentTextureSize);
					}
				}
			}

			STAT( int32 PerfectWantedTextureSize = StreamingTexture.GetSize( StreamingTexture.PerfectWantedMips ) );
			STAT( ThreadStats.TotalRequiredSize += PerfectWantedTextureSize );
			StreamingManager.UpdateFrameStats( ThreadContext, StreamingTexture, Index );

			// Reset the boost factor
			StreamingTexture.BoostFactor = 1.0f;
		}

		// Sort the candidates.
		struct FCompareTexturePriority
		{
			FORCEINLINE bool operator()( const FTexturePriority& A, const FTexturePriority& B ) const
			{
				if ( A.Priority > B.Priority )
				{
					return true;
				}
				else if ( A.Priority == B.Priority )
				{
					return ( A.TextureIndex < B.TextureIndex );
				}
				return false;
			}
		};
		PrioritizedTextures.Sort( FCompareTexturePriority() );
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncTextureStreaming, STATGROUP_ThreadPoolAsyncTasks);
	}

	/** Reference to the owning streaming manager, for accessing the thread-safe data. */
	FStreamingManagerTexture&	StreamingManager;
	/** Resulting priorities, matching the FStreamingManagerTexture::StreamingTextures array. */
	TArray<FTexturePriority>	PrioritizedTextures;
	/** Context (temporary info) used for the async work. */
	FStreamingContext			ThreadContext;
	/** Thread statistics. */
	FAsyncStats				ThreadStats;
	/** Whether the async work should abort its processing. */
	volatile bool				bAbort;
};


/*-----------------------------------------------------------------------------
	IStreamingManager implementation.
-----------------------------------------------------------------------------*/

static FStreamingManagerCollection* StreamingManagerCollection = NULL;

FStreamingManagerCollection& IStreamingManager::Get()
{
	if (StreamingManagerCollection == NULL)
	{
		StreamingManagerCollection = new FStreamingManagerCollection();
	}
	return *StreamingManagerCollection;
}

void IStreamingManager::Shutdown()
{
	if (StreamingManagerCollection != NULL)
	{
		delete StreamingManagerCollection;
		StreamingManagerCollection = (FStreamingManagerCollection*)-1;//Force Error if manager used after shutdown
	}
}

bool IStreamingManager::HasShutdown()
{
	return StreamingManagerCollection == (FStreamingManagerCollection*)-1;
}

/**
 * Adds the passed in view information to the static array.
 *
 * @param ViewInfos				[in/out] Array to add the view to
 * @param ViewOrigin			View origin
 * @param ScreenSize			Screen size
 * @param FOVScreenSize			Screen size taking FOV into account
 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other regular locations
 * @param Duration				How long the streaming system should keep checking this location (in seconds). 0 means just for the next Tick.
 * @param InActorToBoost		Optional pointer to an actor who's textures should have their streaming priority boosted
 */
void IStreamingManager::AddViewInfoToArray( TArray<FStreamingViewInfo> &ViewInfos, const FVector& ViewOrigin, float ScreenSize, float FOVScreenSize, float BoostFactor, bool bOverrideLocation, float Duration, TWeakObjectPtr<AActor> InActorToBoost )
{
	// Check for duplicates and existing overrides.
	bool bShouldAddView = true;
	for ( int32 ViewIndex=0; ViewIndex < ViewInfos.Num(); ++ViewIndex )
	{
		FStreamingViewInfo& ViewInfo = ViewInfos[ ViewIndex ];
		if ( ViewOrigin.Equals( ViewInfo.ViewOrigin, 0.5f ) &&
			FMath::IsNearlyEqual( ScreenSize, ViewInfo.ScreenSize ) &&
			FMath::IsNearlyEqual( FOVScreenSize, ViewInfo.FOVScreenSize ) &&
			ViewInfo.bOverrideLocation == bOverrideLocation )
		{
			// Update duration
			ViewInfo.Duration = Duration;
			// Overwrite BoostFactor if it isn't default 1.0
			ViewInfo.BoostFactor = FMath::IsNearlyEqual(BoostFactor, 1.0f) ? ViewInfo.BoostFactor : BoostFactor;
			bShouldAddView = false;
		}
	}

	if ( bShouldAddView )
	{
		new (ViewInfos) FStreamingViewInfo( ViewOrigin, ScreenSize, FOVScreenSize, BoostFactor, bOverrideLocation, Duration, InActorToBoost );
	}
}

/**
 * Remove view infos with the same location from the given array.
 *
 * @param ViewInfos				[in/out] Array to remove the view from
 * @param ViewOrigin			View origin
 */
void IStreamingManager::RemoveViewInfoFromArray( TArray<FStreamingViewInfo> &ViewInfos, const FVector& ViewOrigin )
{
	for ( int32 ViewIndex=0; ViewIndex < ViewInfos.Num(); ++ViewIndex )
	{
		FStreamingViewInfo& ViewInfo = ViewInfos[ ViewIndex ];
		if ( ViewOrigin.Equals( ViewInfo.ViewOrigin, 0.5f ) )
		{
			ViewInfos.RemoveAtSwap( ViewIndex-- );
		}
	}
}

#if STREAMING_LOG_VIEWCHANGES
TArray<FStreamingViewInfo> GPrevViewLocations;
#endif

/**
 * Sets up the CurrentViewInfos array based on PendingViewInfos, LastingViewInfos and SlaveLocations.
 * Removes out-dated LastingViewInfos.
 *
 * @param DeltaTime		Time since last call in seconds
 */
void IStreamingManager::SetupViewInfos( float DeltaTime )
{
	// Reset CurrentViewInfos
	CurrentViewInfos.Empty( PendingViewInfos.Num() + LastingViewInfos.Num() + SlaveLocations.Num() );

	bool bHaveMultiplePlayerViews = (PendingViewInfos.Num() > 1) ? true : false;

	// Add the slave locations.
	float ScreenSize = 1280.0f;
	float FOVScreenSize = ScreenSize / FMath::Tan( 80.0f * float(PI) / 360.0f );
	if ( PendingViewInfos.Num() > 0 )
	{
		ScreenSize = PendingViewInfos[0].ScreenSize;
		FOVScreenSize = PendingViewInfos[0].FOVScreenSize;
	}
	else if ( LastingViewInfos.Num() > 0 )
	{
		ScreenSize = LastingViewInfos[0].ScreenSize;
		FOVScreenSize = LastingViewInfos[0].FOVScreenSize;
	}
	// Add them to the appropriate array (pending views or lasting views).
	for ( int32 SlaveLocationIndex=0; SlaveLocationIndex < SlaveLocations.Num(); SlaveLocationIndex++ )
	{
		const FSlaveLocation& SlaveLocation = SlaveLocations[ SlaveLocationIndex ];
		AddViewInformation( SlaveLocation.Location, ScreenSize, FOVScreenSize, SlaveLocation.BoostFactor, SlaveLocation.bOverrideLocation, SlaveLocation.Duration );
	}

	// Apply a split-screen factor if we have multiple players on the same machine, and they currently have individual views.
	float SplitScreenFactor = 1.0f;
	
	if ( bHaveMultiplePlayerViews && GEngine->IsSplitScreen(NULL) )
	{
		SplitScreenFactor = 0.75f;
	}

	// Should we use override views this frame? (If we have both a fullscreen movie and an override view.)
	bool bUseOverrideViews = false;
	bool bIsMoviePlaying = false;//GetMoviePlayer()->IsMovieCurrentlyPlaying();
	if ( bIsMoviePlaying )
	{
		// Check if we have any override views.
		for ( int32 ViewIndex=0; !bUseOverrideViews && ViewIndex < LastingViewInfos.Num(); ++ViewIndex )
		{
			const FStreamingViewInfo& ViewInfo = LastingViewInfos[ ViewIndex ];
			if ( ViewInfo.bOverrideLocation )
			{
				bUseOverrideViews = true;
			}
		}
		for ( int32 ViewIndex=0; !bUseOverrideViews && ViewIndex < PendingViewInfos.Num(); ++ViewIndex )
		{
			const FStreamingViewInfo& ViewInfo = PendingViewInfos[ ViewIndex ];
			if ( ViewInfo.bOverrideLocation )
			{
				bUseOverrideViews = true;
			}
		}
	}

	// Add the lasting views.
	for ( int32 ViewIndex=0; ViewIndex < LastingViewInfos.Num(); ++ViewIndex )
	{
		const FStreamingViewInfo& ViewInfo = LastingViewInfos[ ViewIndex ];
		if ( bUseOverrideViews == ViewInfo.bOverrideLocation )
		{
			AddViewInfoToArray( CurrentViewInfos, ViewInfo.ViewOrigin, ViewInfo.ScreenSize * SplitScreenFactor, ViewInfo.FOVScreenSize, ViewInfo.BoostFactor, ViewInfo.bOverrideLocation, ViewInfo.Duration, ViewInfo.ActorToBoost );
		}
	}

	// Add the regular views.
	for ( int32 ViewIndex=0; ViewIndex < PendingViewInfos.Num(); ++ViewIndex )
	{
		const FStreamingViewInfo& ViewInfo = PendingViewInfos[ ViewIndex ];
		if ( bUseOverrideViews == ViewInfo.bOverrideLocation )
		{
			AddViewInfoToArray( CurrentViewInfos, ViewInfo.ViewOrigin, ViewInfo.ScreenSize * SplitScreenFactor, ViewInfo.FOVScreenSize, ViewInfo.BoostFactor, ViewInfo.bOverrideLocation, ViewInfo.Duration, ViewInfo.ActorToBoost );
		}
	}

	// Update duration for the lasting views, removing out-dated ones.
	for ( int32 ViewIndex=0; ViewIndex < LastingViewInfos.Num(); ++ViewIndex )
	{
		FStreamingViewInfo& ViewInfo = LastingViewInfos[ ViewIndex ];
		ViewInfo.Duration -= DeltaTime;

		// Remove old override locations.
		if ( ViewInfo.Duration <= 0.0f )
		{
			LastingViewInfos.RemoveAtSwap( ViewIndex-- );
		}
	}

#if STREAMING_LOG_VIEWCHANGES
	{
		// Check if we're adding any new locations.
		for ( int32 ViewIndex = 0; ViewIndex < CurrentViewInfos.Num(); ++ViewIndex )
		{
			FStreamingViewInfo& ViewInfo = CurrentViewInfos( ViewIndex );
			bool bFound = false;
			for ( int32 PrevView=0; PrevView < GPrevViewLocations.Num(); ++PrevView )
			{
				if ( (ViewInfo.ViewOrigin - GPrevViewLocations(PrevView).ViewOrigin).Size() < 100.0f )
				{
					bFound = true;
					break;
				}
			}
			if ( !bFound )
			{
				UE_LOG(LogContentStreaming, Log, TEXT("Adding location: X=%.1f, Y=%.1f, Z=%.1f (override=%d, boost=%.1f)"), ViewInfo.ViewOrigin.X, ViewInfo.ViewOrigin.Y, ViewInfo.ViewOrigin.Z, ViewInfo.bOverrideLocation, ViewInfo.BoostFactor );
			}
		}

		// Check if we're removing any locations.
		for ( int32 PrevView=0; PrevView < GPrevViewLocations.Num(); ++PrevView )
		{
			bool bFound = false;
			for ( int32 ViewIndex = 0; ViewIndex < CurrentViewInfos.Num(); ++ViewIndex )
			{
				FStreamingViewInfo& ViewInfo = CurrentViewInfos( ViewIndex );
				if ( (ViewInfo.ViewOrigin - GPrevViewLocations(PrevView).ViewOrigin).Size() < 100.0f )
				{
					bFound = true;
					break;
				}
			}
			if ( !bFound )
			{
				FStreamingViewInfo& PrevViewInfo = GPrevViewLocations(PrevView);
				UE_LOG(LogContentStreaming, Log, TEXT("Removing location: X=%.1f, Y=%.1f, Z=%.1f (override=%d, boost=%.1f)"), PrevViewInfo.ViewOrigin.X, PrevViewInfo.ViewOrigin.Y, PrevViewInfo.ViewOrigin.Z, PrevViewInfo.bOverrideLocation, PrevViewInfo.BoostFactor );
			}
		}

		// Save the locations.
		GPrevViewLocations.Empty(CurrentViewInfos.Num());
		for ( int32 ViewIndex = 0; ViewIndex < CurrentViewInfos.Num(); ++ViewIndex )
		{
			FStreamingViewInfo& ViewInfo = CurrentViewInfos( ViewIndex );
			GPrevViewLocations.Add( ViewInfo );
		}
	}
#endif
}

/**
 * Adds the passed in view information to the static array.
 *
 * @param ViewOrigin			View origin
 * @param ScreenSize			Screen size
 * @param FOVScreenSize			Screen size taking FOV into account
 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other regular locations
 * @param Duration				How long the streaming system should keep checking this location (in seconds). 0 means just for the next Tick.
 * @param InActorToBoost		Optional pointer to an actor who's textures should have their streaming priority boosted
 */
void IStreamingManager::AddViewInformation( const FVector& ViewOrigin, float ScreenSize, float FOVScreenSize, float BoostFactor/*=1.0f*/, bool bOverrideLocation/*=false*/, float Duration/*=0.0f*/, TWeakObjectPtr<AActor> InActorToBoost /*=NULL*/ )
{
	// Is this a reasonable location?
	if ( FMath::Abs(ViewOrigin.X) < (1.0e+20f) && FMath::Abs(ViewOrigin.Y) < (1.0e+20f) && FMath::Abs(ViewOrigin.Z) < (1.0e+20f) )
	{
		BoostFactor *= CVarStreamingBoost.GetValueOnGameThread();

		if ( bPendingRemoveViews )
		{
			bPendingRemoveViews = false;

			// Remove out-dated override views and empty the PendingViewInfos/SlaveLocation arrays to be populated again during next frame.
			RemoveStreamingViews( RemoveStreamingViews_Normal );
		}

		// Remove a lasting location if we're given the same location again but with 0 duration.
		if ( Duration <= 0.0f )
		{
			RemoveViewInfoFromArray( LastingViewInfos, ViewOrigin );
		}

		// Should we remember this location for a while?
		if ( Duration > 0.0f )
		{
			AddViewInfoToArray( LastingViewInfos, ViewOrigin, ScreenSize, FOVScreenSize, BoostFactor, bOverrideLocation, Duration, InActorToBoost );
		}
		else
		{
			AddViewInfoToArray( PendingViewInfos, ViewOrigin, ScreenSize, FOVScreenSize, BoostFactor, bOverrideLocation, 0.0f, InActorToBoost );
		}
	}
	else
	{
		int SetDebugBreakpointHere = 0;
	}
}

/**
 * Queue up view "slave" locations to the streaming system. These locations will be added properly at the next call to AddViewInformation,
 * re-using the screensize and FOV settings.
 *
 * @param SlaveLocation			World-space view origin
 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other locations
 * @param Duration				How long the streaming system should keep checking this location (in seconds). 0 means just for the next Tick.
 */
void IStreamingManager::AddViewSlaveLocation( const FVector& SlaveLocation, float BoostFactor/*=1.0f*/, bool bOverrideLocation/*=false*/, float Duration/*=0.0f*/ )
{
	BoostFactor *= CVarStreamingBoost.GetValueOnGameThread();

	if ( bPendingRemoveViews )
	{
		bPendingRemoveViews = false;

		// Remove out-dated override views and empty the PendingViewInfos/SlaveLocation arrays to be populated again during next frame.
		RemoveStreamingViews( RemoveStreamingViews_Normal );
	}

	new (SlaveLocations) FSlaveLocation(SlaveLocation, BoostFactor, bOverrideLocation, Duration );
}

/**
 * Removes streaming views from the streaming manager. This is also called by Tick().
 *
 * @param RemovalType	What types of views to remove (all or just the normal views)
 */
void IStreamingManager::RemoveStreamingViews( ERemoveStreamingViews RemovalType )
{
	PendingViewInfos.Empty();
	SlaveLocations.Empty();
	if ( RemovalType == RemoveStreamingViews_All )
	{
		LastingViewInfos.Empty();
	}
}

/**
 * Calls UpdateResourceStreaming(), and does per-frame cleaning. Call once per frame.
 *
 * @param DeltaTime				Time since last call in seconds
 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
 */
void IStreamingManager::Tick( float DeltaTime, bool bProcessEverything/*=false*/ )
{
	UpdateResourceStreaming( DeltaTime, bProcessEverything );

	// Trigger a call to RemoveStreamingViews( RemoveStreamingViews_Normal ) next time a view is added.
	bPendingRemoveViews = true;
}


/*-----------------------------------------------------------------------------
	FStreamingManagerCollection implementation.
-----------------------------------------------------------------------------*/

FStreamingManagerCollection::FStreamingManagerCollection()
:	NumIterations(1)
,	DisableResourceStreamingCount(0)
,	LoadMapTimeLimit( 5.0f )
,   TextureStreamingManager( NULL )
{
#if PLATFORM_SUPPORTS_TEXTURE_STREAMING
	// Disable texture streaming if that was requested (needs to happen before the call to ProcessNewlyLoadedUObjects, as that can load textures)
	if( FParse::Param( FCommandLine::Get(), TEXT( "NoTextureStreaming" ) ) )
	{
		CVarSetTextureStreaming.AsVariable()->Set(0, ECVF_SetByCommandline);
	}
#endif

	AddOrRemoveTextureStreamingManagerIfNeeded(true);

	AudioStreamingManager = new FAudioStreamingManager();
	AddStreamingManager( AudioStreamingManager );
}

/**
 * Sets the number of iterations to use for the next time UpdateResourceStreaming is being called. This 
 * is being reset to 1 afterwards.
 *
 * @param InNumIterations	Number of iterations to perform the next time UpdateResourceStreaming is being called.
 */
void FStreamingManagerCollection::SetNumIterationsForNextFrame( int32 InNumIterations )
{
	NumIterations = InNumIterations;
}

/**
 * Calls UpdateResourceStreaming(), and does per-frame cleaning. Call once per frame.
 *
 * @param DeltaTime				Time since last call in seconds
 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
 */
void FStreamingManagerCollection::Tick( float DeltaTime, bool bProcessEverything )
{
	AddOrRemoveTextureStreamingManagerIfNeeded();

	IStreamingManager::Tick(DeltaTime, bProcessEverything);
}

/**
 * Updates streaming, taking into account all current view infos. Can be called multiple times per frame.
 *
 * @param DeltaTime				Time since last call in seconds
 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
 */
void FStreamingManagerCollection::UpdateResourceStreaming( float DeltaTime, bool bProcessEverything/*=false*/ )
{
	SetupViewInfos( DeltaTime );

	// only allow this if its not disabled
	if (DisableResourceStreamingCount == 0)
	{
		for( int32 Iteration=0; Iteration<NumIterations; Iteration++ )
		{
			// Flush rendering commands in the case of multiple iterations to sync up resource streaming
			// with the GPU/ rendering thread.
			if( Iteration > 0 )
			{
				FlushRenderingCommands();
			}

			// Route to streaming managers.
			for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
			{
				IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
				StreamingManager->UpdateResourceStreaming( DeltaTime, bProcessEverything );
			}
		}

		// Reset number of iterations to 1 for next frame.
		NumIterations = 1;
	}
}

/**
 * Streams in/out all resources that wants to and blocks until it's done.
 *
 * @param TimeLimit					Maximum number of seconds to wait for streaming I/O. If zero, uses .ini setting
 * @return							Number of streaming requests still in flight, if the time limit was reached before they were finished.
 */
int32 FStreamingManagerCollection::StreamAllResources( float TimeLimit/*=0.0f*/ )
{
	// Disable mip-fading for upcoming texture updates.
	float PrevMipLevelFadingState = GEnableMipLevelFading;
	GEnableMipLevelFading = -1.0f;

	FlushRenderingCommands();

	if (FMath::IsNearlyZero(TimeLimit))
	{
		TimeLimit = LoadMapTimeLimit;
	}

	// Update resource streaming, making sure we process all textures.
	UpdateResourceStreaming(0, true);

	// Block till requests are finished, or time limit was reached.
	int32 NumPendingRequests = BlockTillAllRequestsFinished(TimeLimit, true);

	GEnableMipLevelFading = PrevMipLevelFadingState;

	return NumPendingRequests;
}

/**
 * Blocks till all pending requests are fulfilled.
 *
 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
 * @param bLogResults	Whether to dump the results to the log.
 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
 */
int32 FStreamingManagerCollection::BlockTillAllRequestsFinished( float TimeLimit /*= 0.0f*/, bool bLogResults /*= false*/ )
{
	int32 NumPendingRequests = 0;

	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		NumPendingRequests += StreamingManager->BlockTillAllRequestsFinished( TimeLimit, bLogResults );
	}

	return NumPendingRequests;
}

/** Returns the number of resources that currently wants to be streamed in. */
int32 FStreamingManagerCollection::GetNumWantingResources() const
{
	int32 NumWantingResources = 0;

	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		NumWantingResources += StreamingManager->GetNumWantingResources();
	}

	return NumWantingResources;
}

/**
 * Returns the current ID for GetNumWantingResources().
 * The ID is bumped every time NumWantingResources is updated by the streaming system (every few frames).
 * Can be used to verify that any changes have been fully examined, by comparing current ID with
 * what it was when the changes were made.
 */
int32 FStreamingManagerCollection::GetNumWantingResourcesID() const
{
	int32 NumWantingResourcesCounter = MAX_int32;

	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		NumWantingResourcesCounter = FMath::Min( NumWantingResourcesCounter, StreamingManager->GetNumWantingResourcesID() );
	}

	return NumWantingResourcesCounter;
}

/**
 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
 */
void FStreamingManagerCollection::CancelForcedResources()
{
	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		StreamingManager->CancelForcedResources();
	}
}

/**
 * Notifies managers of "level" change.
 */
void FStreamingManagerCollection::NotifyLevelChange()
{
	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		StreamingManager->NotifyLevelChange();
	}
}

bool FStreamingManagerCollection::IsStreamingEnabled() const
{
	return DisableResourceStreamingCount == 0;
}

bool FStreamingManagerCollection::IsTextureStreamingEnabled() const
{
	return TextureStreamingManager != 0;
}

ITextureStreamingManager& FStreamingManagerCollection::GetTextureStreamingManager() const
{
	check(TextureStreamingManager != 0);
	return *TextureStreamingManager;
}

IAudioStreamingManager& FStreamingManagerCollection::GetAudioStreamingManager() const
{
	check(AudioStreamingManager);
	return *AudioStreamingManager;
}

/** Don't stream world resources for the next NumFrames. */
void FStreamingManagerCollection::SetDisregardWorldResourcesForFrames(int32 NumFrames )
{
	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		StreamingManager->SetDisregardWorldResourcesForFrames(NumFrames);
	}
}

/**
 * Adds a streaming manager to the array of managers to route function calls to.
 *
 * @param StreamingManager	Streaming manager to add
 */
void FStreamingManagerCollection::AddStreamingManager( IStreamingManager* StreamingManager )
{
	StreamingManagers.Add( StreamingManager );
}

/**
 * Removes a streaming manager from the array of managers to route function calls to.
 *
 * @param StreamingManager	Streaming manager to remove
 */
void FStreamingManagerCollection::RemoveStreamingManager( IStreamingManager* StreamingManager )
{
	StreamingManagers.Remove( StreamingManager );
}

/**
 * Disables resource streaming. Enable with EnableResourceStreaming. Disable/enable can be called multiple times nested
 */
void FStreamingManagerCollection::DisableResourceStreaming()
{
	// push on a disable
	FPlatformAtomics::InterlockedIncrement(&DisableResourceStreamingCount);
}

/**
 * Enables resource streaming, previously disabled with enableResourceStreaming. Disable/enable can be called multiple times nested
 * (this will only actually enable when all disables are matched with enables)
 */
void FStreamingManagerCollection::EnableResourceStreaming()
{
	// pop off a disable
	FPlatformAtomics::InterlockedDecrement(&DisableResourceStreamingCount);

	checkf(DisableResourceStreamingCount >= 0, TEXT("Mismatched number of calls to FStreamingManagerCollection::DisableResourceStreaming/EnableResourceStreaming"));
}

/**
 * Allows the streaming manager to process exec commands.
 *
 * @param Cmd	Exec command
 * @param Ar	Output device for feedback
 * @return		true if the command was handled
 */
bool FStreamingManagerCollection::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) 
{
	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		if ( StreamingManager->Exec( InWorld, Cmd, Ar ) )
		{
			return true;
		}
	}
	return false;
}

/** Adds a ULevel to the streaming manager. */
void FStreamingManagerCollection::AddLevel( ULevel* Level )
{
#if STREAMING_LOG_LEVELS
	UE_LOG(LogContentStreaming, Log, TEXT("FStreamingManagerCollection::AddLevel(\"%s\")"), *Level->GetOutermost()->GetName());
#endif

	if ( Level->bTextureStreamingBuilt == false )
	{
		// If it hasn't been done yet for this level, build its texture streaming data now.
		// This will also add the level.
		Level->BuildStreamingData();
	}
	else
	{
		AddPreparedLevel( Level );
	}
}

/** Removes a ULevel from the streaming manager. */
void FStreamingManagerCollection::RemoveLevel( ULevel* Level )
{
#if STREAMING_LOG_LEVELS
	UE_LOG(LogContentStreaming, Log, TEXT("FStreamingManagerCollection::RemoveLevel(\"%s\")"), *Level->GetOutermost()->GetName());
#endif

	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		StreamingManager->RemoveLevel( Level );
	}
}

/** Adds a ULevel that has already built StreamingData to the streaming manager. */
void FStreamingManagerCollection::AddPreparedLevel( class ULevel* Level )
{
#if STREAMING_LOG_LEVELS
	UE_LOG(LogContentStreaming, Log, TEXT("FStreamingManagerCollection::AddPreparedLevel(\"%s\")"), *Level->GetOutermost()->GetName());
#endif

	check( Level->bTextureStreamingBuilt );

	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		StreamingManager->AddPreparedLevel( Level );
	}
}

/** Called when an actor is spawned. */
void FStreamingManagerCollection::NotifyActorSpawned( AActor* Actor )
{
	STAT( double StartTime = FPlatformTime::Seconds() );

	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		StreamingManager->NotifyActorSpawned( Actor );
	}
	STAT( GStreamingDynamicPrimitivesTime += FPlatformTime::Seconds() - StartTime );
}

/** Called when a spawned actor is destroyed. */
void FStreamingManagerCollection::NotifyActorDestroyed( AActor* Actor )
{
	STAT( double StartTime = FPlatformTime::Seconds() );

	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		StreamingManager->NotifyActorDestroyed( Actor );
	}
	STAT( GStreamingDynamicPrimitivesTime += FPlatformTime::Seconds() - StartTime );
}

/**
 * Called when a primitive is attached to an actor or another component.
 * Replaces previous info, if the primitive was already attached.
 *
 * @param InPrimitive	Newly attached dynamic/spawned primitive
 */
void FStreamingManagerCollection::NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
{
	STAT( double StartTime = FPlatformTime::Seconds() );

	// Distance-based heuristics only handle mesh components
	if ( Primitive->IsA( UMeshComponent::StaticClass() ) )
	{
		for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
		{
			IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
			StreamingManager->NotifyPrimitiveAttached( Primitive, DynamicType );
		}
	}
	STAT( GStreamingDynamicPrimitivesTime += FPlatformTime::Seconds() - StartTime );
}

/** Called when a primitive is detached from an actor or another component. */
void FStreamingManagerCollection::NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive )
{
	STAT( double StartTime = FPlatformTime::Seconds() );

	// Distance-based heuristics only handle mesh components
	if ( Primitive->IsA( UMeshComponent::StaticClass() ) )
	{
		// Route to streaming managers.
		for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
		{
			IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
			StreamingManager->NotifyPrimitiveDetached( Primitive );
		}
	}
	STAT( GStreamingDynamicPrimitivesTime += FPlatformTime::Seconds() - StartTime );
}

/**
 * Called when a primitive has had its textured changed.
 * Only affects primitives that were already attached.
 * Replaces previous info.
 */
void FStreamingManagerCollection::NotifyPrimitiveUpdated( const UPrimitiveComponent* Primitive )
{
	STAT( double StartTime = FPlatformTime::Seconds() );

	if ( Primitive->IsA( UMeshComponent::StaticClass() ) )
	{
		// Route to streaming managers.
		for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
		{
			IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
			StreamingManager->NotifyPrimitiveUpdated( Primitive );
		}
	}
	STAT( GStreamingDynamicPrimitivesTime += FPlatformTime::Seconds() - StartTime );
}

void FStreamingManagerCollection::AddOrRemoveTextureStreamingManagerIfNeeded(bool bIsInit)
{
	bool bUseTextureStreaming = false;

#if PLATFORM_SUPPORTS_TEXTURE_STREAMING
	{
		bUseTextureStreaming = CVarSetTextureStreaming.GetValueOnGameThread() != 0;
	}

	if( !GRHISupportsTextureStreaming || IsRunningDedicatedServer() )
	{
		bUseTextureStreaming = false;
	}
#endif

	if ( bUseTextureStreaming )
	{
		//Add the texture streaming manager if it's needed.
		if( !TextureStreamingManager )
		{
			GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("LoadMapTimeLimit"), LoadMapTimeLimit, GEngineIni );
			// Create the streaming manager and add the default streamers.
			TextureStreamingManager = new FStreamingManagerTexture();
			TextureStreamingManager->AddTextureStreamingHandler( new FStreamingHandlerTextureStatic() );
			TextureStreamingManager->AddTextureStreamingHandler( new FStreamingHandlerTextureLevelForced() );
			AddStreamingManager( TextureStreamingManager );		
				
			//Need to work out if all textures should be streamable and added to the texture streaming manager.
			//This works but may be more heavy handed than necessary.
			if( !bIsInit )
			{
				for( TObjectIterator<UTexture2D>It; It; ++It )
				{
					It->UpdateResource();
				}
			}
		}
	}
	else
	{
		//Remove the texture streaming manager if needed.
		if( TextureStreamingManager )
		{
			for( TObjectIterator<UTexture2D>It; It; ++It )
			{
				if( It->bIsStreamable )
				{
					It->UpdateResource();
				}
			}
			TextureStreamingManager->BlockTillAllRequestsFinished();

			RemoveStreamingManager(TextureStreamingManager);
			delete TextureStreamingManager;
			TextureStreamingManager = NULL;
		}
	}
}

/*-----------------------------------------------------------------------------
	FStreamingManagerTexture implementation.
-----------------------------------------------------------------------------*/

/** Constructor, initializing all members and  */
FStreamingManagerTexture::FStreamingManagerTexture()
:	CurrentUpdateStreamingTextureIndex(0)
,	bTriggerDumpTextureGroupStats( false )
,	bDetailedDumpTextureGroupStats( false )
,	bTriggerInvestigateTexture( false )
,	AsyncWork( NULL )
,	ProcessingStage( 0 )
,	NumTextureProcessingStages(5)
,	MaxTempMemoryUsed( 5*1024*1024 )
,	bUseDynamicStreaming( false )
,	BoostPlayerTextures( 3.0f )
,	MemoryMargin(0)
,	MinEvictSize(0)
,	IndividualStreamingTexture(NULL)
,	NumStreamingTextures(0)
,	NumRequestsInCancelationPhase(0)
,	NumRequestsInUpdatePhase(0)
,	NumRequestsInFinalizePhase(0)
,	TotalIntermediateTexturesSize(0)
,	NumIntermediateTextures(0)
,	TotalStreamingTexturesSize(0)
,	TotalStreamingTexturesMaxSize(0)
,	TotalLightmapMemorySize(0)
,	TotalLightmapDiskSize(0)
,	TotalMipCountIncreaseRequestsInFlight(0)
,	TotalOptimalWantedSize(0)
,	TotalStaticTextureHeuristicSize(0)
,	TotalDynamicTextureHeuristicSize(0)
,	TotalLastRenderHeuristicSize(0)
,	TotalForcedHeuristicSize(0)
,	OriginalTexturePoolSize(0)
,	PreviousPoolSizeTimestamp(0.0)
,	PreviousPoolSizeSetting(-1)
,	bCollectTextureStats(false)
,	bReportTextureStats(false)
,	bPauseTextureStreaming(false)
{
	// Read settings from ini file.
	int32 TempInt;
	verify( GConfig->GetInt( TEXT("TextureStreaming"), TEXT("MemoryMargin"),				TempInt,						GEngineIni ) );
	MemoryMargin = TempInt;
	verify( GConfig->GetInt( TEXT("TextureStreaming"), TEXT("MinEvictSize"),				TempInt,						GEngineIni ) );
	MinEvictSize = TempInt;

	verify( GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("LightmapStreamingFactor"),			GLightmapStreamingFactor,		GEngineIni ) );
	verify( GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("ShadowmapStreamingFactor"),			GShadowmapStreamingFactor,		GEngineIni ) );

	int32 PoolSizeIniSetting = 0;
	GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSize"), PoolSizeIniSetting, GEngineIni);
	GConfig->GetBool(TEXT("TextureStreaming"), TEXT("UseDynamicStreaming"), bUseDynamicStreaming, GEngineIni);
	GConfig->GetFloat( TEXT("TextureStreaming"), TEXT("BoostPlayerTextures"), BoostPlayerTextures, GEngineIni );
	GConfig->GetBool(TEXT("TextureStreaming"), TEXT("NeverStreamOutTextures"), GNeverStreamOutTextures, GEngineIni);

	// Read pool size from the CVar
	static const auto CVarStreamingTexturePoolSize = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Streaming.PoolSize"));
	int32 PoolSizeCVar = CVarStreamingTexturePoolSize ? CVarStreamingTexturePoolSize->GetValueOnGameThread() : -1;

	if( PoolSizeCVar != -1 )
	{
		OriginalTexturePoolSize =  int64(PoolSizeCVar) * 1024ll * 1024ll;
		GTexturePoolSize = OriginalTexturePoolSize;
	}	
	else if ( GPoolSizeVRAMPercentage )
	{
		// If GPoolSizeVRAMPercentage is set, the pool size has already been calculated and we're not reading it from the .ini
		OriginalTexturePoolSize = GTexturePoolSize;
	}
	else
	{
		// Don't use a texture pool if it's not read from .ini or calculated at startup
		OriginalTexturePoolSize = 0;
		GTexturePoolSize = 0;
	}

	// -NeverStreamOutTextures
	if (FParse::Param(FCommandLine::Get(), TEXT("NeverStreamOutTextures")))
	{
		GNeverStreamOutTextures = true;
	}
	if (GIsEditor)
	{
		// this would not be good or useful in the editor
		GNeverStreamOutTextures = false;

		// Use unlimited texture streaming in the editor. Quality is more important than stutter.
		GTexturePoolSize = 0;
	}
	if (GNeverStreamOutTextures)
	{
		UE_LOG(LogContentStreaming, Log, TEXT("Textures will NEVER stream out!"));
	}

	UE_LOG(LogContentStreaming,Log,TEXT("Texture pool size is %fMB"),GTexturePoolSize/1024.f/1024.f);

	// Convert from MByte to byte.
	MinEvictSize *= 1024 * 1024;
	MemoryMargin *= 1024 * 1024;

	// Allow half of the memory margin for temporary streaming waste.
	MaxTempMemoryUsed = MemoryMargin / 2;

#if STATS
	NumLatencySamples = 0;
	LatencySampleIndex = 0;
	LatencyAverage = 0.0f;
	LatencyMaximum = 0.0f;
	for ( int32 LatencyIndex=0; LatencyIndex < NUM_LATENCYSAMPLES; ++LatencyIndex )
	{
		LatencySamples[ LatencyIndex ] = 0.0f;
	}
#endif

#if STATS_FAST
	MaxStreamingTexturesSize = 0;
	MaxOptimalTextureSize = 0;
	MaxStreamingOverBudget = MIN_int64;
	MaxTexturePoolAllocatedSize = 0;
	MinLargestHoleSize = OriginalTexturePoolSize;
	MaxNumWantingTextures = 0;
#endif

	for ( int32 LODGroup=0; LODGroup < TEXTUREGROUP_MAX; ++LODGroup )
	{
		const FTextureLODGroup& TexGroup = UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetTextureLODGroup(TextureGroup(LODGroup));
		ThreadSettings.NumStreamedMips[LODGroup] = TexGroup.NumStreamedMips;
	}

	// setup the streaming resource flush function pointer
	GFlushStreamingFunc = &FlushResourceStreaming;

	ProcessingStage = 0;
	AsyncWork = new FAsyncTask<FAsyncTextureStreaming>(this);
}

FStreamingManagerTexture::~FStreamingManagerTexture()
{
	AsyncWork->EnsureCompletion();
	delete AsyncWork;
}

/**
 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
 */
void FStreamingManagerTexture::CancelForcedResources()
{
	float CurrentTime = float(FPlatformTime::Seconds() - GStartTime);

	// Update textures that are Forced on a timer.
	for ( int32 TextureIndex=0; TextureIndex < StreamingTextures.Num(); ++TextureIndex )
	{
		FStreamingTexture& StreamingTexture = StreamingTextures[ TextureIndex ];

		// Make sure this streaming texture hasn't been marked for removal.
		if ( StreamingTexture.Texture )
		{
			// Remove any prestream requests from textures
			float TimeLeft = StreamingTexture.Texture->ForceMipLevelsToBeResidentTimestamp - CurrentTime;
			if ( TimeLeft > 0.0f )
			{
				StreamingTexture.Texture->SetForceMipLevelsToBeResident( -1.0f );
				StreamingTexture.InstanceRemovedTimestamp = -FLT_MAX;
				if ( StreamingTexture.Texture->Resource )
				{
					StreamingTexture.Texture->InvalidateLastRenderTimeForStreaming();
				}
#if STREAMING_LOG_CANCELFORCED
				UE_LOG(LogContentStreaming, Log, TEXT("Canceling forced texture: %s (had %.1f seconds left)"), *StreamingTexture.Texture->GetFullName(), TimeLeft );
#endif
			}
		}
	}

	// Reset the streaming system, so it picks up any changes to UTexture2D right away.
	ProcessingStage = 0;
}

/**
 * Notifies manager of "level" change so it can prioritize character textures for a few frames.
 */
void FStreamingManagerTexture::NotifyLevelChange()
{
}

/** Don't stream world resources for the next NumFrames. */
void FStreamingManagerTexture::SetDisregardWorldResourcesForFrames( int32 NumFrames )
{
	//@TODO: We could perhaps increase the priority factor for character textures...
}

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

/**
 *	Helper struct to compare two FTextureSortElement objects.
 **/
struct FTextureStreamingCompare
{
	/** 
	 *	Called by Sort() to compare two elements.
	 *	@param Texture1		- First object to compare
	 *	@param Texture2		- Second object to compare
	 *	@return				- Negative value if Texture1 should be sorted earlier in the array than Texture2, zero if arbitrary order, positive if opposite order.
	 */
	bool operator()( const FTextureSortElement& Texture1, const FTextureSortElement& Texture2 ) const
	{
		// Character textures get lower priority (sorted later in the array).
		if ( Texture1.bIsCharacterTexture != Texture2.bIsCharacterTexture )
		{
			return Texture1.bIsCharacterTexture < Texture2.bIsCharacterTexture;
		}

		// Larger textures get higher priority (sorted earlier in the array).
		if ( Texture2.Size - Texture1.Size )
		{
			return Texture2.Size < Texture1.Size;
		}

		// Then sort on baseaddress, so that textures at the end of the texture pool gets higher priority (sorted earlier in the array).
		// (It's faster to defrag the texture pool when the holes are at the end.)
		return Texture2.TextureDataAddress < Texture1.TextureDataAddress;
	}
};

/**
 *	Renderthread function: Try to stream out texture mip-levels to free up more memory.
 *	@param InCandidateTextures	- Array of possible textures to shrink
 *	@param RequiredMemorySize	- Amount of memory to try to free up, in bytes
 *	@param bSucceeded			- [out] Upon return, whether it succeeded or not
 **/
void Renderthread_StreamOutTextureData(FRHICommandListImmediate& RHICmdList, TArray<FTextureSortElement>* InCandidateTextures, int64 RequiredMemorySize, volatile bool* bSucceeded)
{
	*bSucceeded = false;
	// only for debugging?
	FTextureMemoryStats OldStats;
	RHICmdList.GetTextureMemoryStats(OldStats);

	// Makes sure that texture memory can get freed up right away.
	RHICmdList.BlockUntilGPUIdle();

	FTextureSortElement* CandidateTextures = InCandidateTextures->GetData();

	// Sort the candidates.
	InCandidateTextures->Sort( FTextureStreamingCompare() );

	// Attempt to shrink enough candidates to free up the required memory size. One mip-level at a time.
	int64 SavedMemory = 0;
	bool bKeepShrinking = true;
	bool bShrinkCharacterTextures = false;	// Don't shrink any character textures the first loop.
	while ( SavedMemory < RequiredMemorySize && bKeepShrinking )
	{
		// If we can't shrink anything in the inner-loop, stop the outer-loop as well.
		bKeepShrinking = !bShrinkCharacterTextures;

		for ( int32 TextureIndex=0; TextureIndex < InCandidateTextures->Num() && SavedMemory < RequiredMemorySize; ++TextureIndex )
		{
			FTextureSortElement& SortElement = CandidateTextures[TextureIndex];
			int32 NewMipCount = SortElement.Texture->ResidentMips - 1;
			if ( (!SortElement.bIsCharacterTexture || bShrinkCharacterTextures) && NewMipCount >= SortElement.NumRequiredResidentMips )
			{
				FTexture2DResource* Resource = (FTexture2DResource*) SortElement.Texture->Resource;

				bool bReallocationSucceeded = Resource->TryReallocate( SortElement.Texture->ResidentMips, NewMipCount );
				if ( bReallocationSucceeded )
				{
					// Start using the new one.
					int64 OldSize = SortElement.Size;
					int64 NewSize = SortElement.Texture->CalcTextureMemorySize(NewMipCount);
					int64 Savings = OldSize - NewSize;

					// Set up UTexture2D
					SortElement.Texture->ResidentMips = NewMipCount;
					SortElement.Texture->RequestedMips = NewMipCount;

					// Ok, we found at least one we could shrink. Lets try to find more! :)
					bKeepShrinking = true;

					SavedMemory += Savings;
				}
			}
		}

		// Start shrinking character textures as well, if we have to.
		bShrinkCharacterTextures = true;
	}

	// only for debugging?
	FTextureMemoryStats NewStats;
	RHIGetTextureMemoryStats(NewStats);

	UE_LOG(LogContentStreaming, Log, TEXT("Streaming out texture memory! Saved %.2f MB."), float(SavedMemory)/1024.0f/1024.0f);

	// Return the result.
	*bSucceeded = SavedMemory >= RequiredMemorySize;
}

/**
 *	Try to stream out texture mip-levels to free up more memory.
 *	@param RequiredMemorySize	- Additional texture memory required
 *	@return						- Whether it succeeded or not
 **/
bool FStreamingManagerTexture::StreamOutTextureData( int64 RequiredMemorySize )
{
	RequiredMemorySize = FMath::Max<int64>(RequiredMemorySize, MinEvictSize);

	// Array of candidates for reducing mip-levels.
	TArray<FTextureSortElement> CandidateTextures;
	CandidateTextures.Reserve( StreamingTextures.Num() );

	// Don't stream out character textures (to begin with)
	float CurrentTime = float(FPlatformTime::Seconds() - GStartTime);

	// Collect all textures will be considered for streaming out.
	for (int32 StreamingIndex = 0; StreamingIndex < StreamingTextures.Num(); ++StreamingIndex)
	{
		FStreamingTexture& StreamingTexture = StreamingTextures[StreamingIndex];
		if (StreamingTexture.Texture)
		{
			UTexture2D* Texture = StreamingTexture.Texture;

			// Skyboxes should not stream out.
			if ( Texture->LODGroup == TEXTUREGROUP_Skybox )
			{
				continue;
			}

			// Number of mip-levels that must be resident due to mip-tails and UTexture2D::GetMinTextureResidentMipCount().
			int32 NumMips = Texture->GetNumMips();
			int32 MipTailBaseIndex = Texture->GetMipTailBaseIndex();
			int32 NumRequiredResidentMips = (MipTailBaseIndex >= 0) ? FMath::Max<int32>(NumMips - MipTailBaseIndex, 0) : 0;
			NumRequiredResidentMips = FMath::Max<int32>(NumRequiredResidentMips, UTexture2D::GetMinTextureResidentMipCount());

			// Only consider streamable textures that have enough miplevels, and who are currently ready for streaming.
			if ( FStreamingTexture::IsStreamingTexture(Texture) && FStreamingTexture::IsReadyForStreaming(Texture) && Texture->ResidentMips > NumRequiredResidentMips  )
			{
				// We can't stream out mip-tails.
				int32 CurrentBaseMip = NumMips - Texture->ResidentMips;
				if ( MipTailBaseIndex < 0 || CurrentBaseMip < MipTailBaseIndex )
				{
					// Figure out whether texture should be forced resident based on bools and forced resident time.
					bool bForceMipLevelsToBeResident = (Texture->ShouldMipLevelsBeForcedResident() || Texture->ForceMipLevelsToBeResidentTimestamp >= CurrentTime);
					if ( bForceMipLevelsToBeResident == false && Texture->Resource )
					{
						// Don't try to stream out if the texture is currently being busy being streamed in/out.
						bool bSafeToStream = (Texture->UpdateStreamingStatus() == false);
						if ( bSafeToStream )
						{
							bool bIsCharacterTexture =	Texture->LODGroup == TEXTUREGROUP_Character ||
														Texture->LODGroup == TEXTUREGROUP_CharacterSpecular ||
														Texture->LODGroup == TEXTUREGROUP_CharacterNormalMap;
							uint32 TextureDataAddress = 0;
							CandidateTextures.Add( FTextureSortElement(Texture, Texture->CalcTextureMemorySize(Texture->ResidentMips), bIsCharacterTexture ? 1 : 0, TextureDataAddress, NumRequiredResidentMips) );
						}
					}
				}
			}
		}
	}

	volatile bool bSucceeded = false;

	// Queue up the process on the render thread.
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		StreamOutTextureDataCommand,
		TArray<FTextureSortElement>*,CandidateTextures,&CandidateTextures,
		int64,RequiredMemorySize,RequiredMemorySize,
		volatile bool*,bSucceeded,&bSucceeded,
	{
		Renderthread_StreamOutTextureData(RHICmdList, CandidateTextures, RequiredMemorySize, bSucceeded);
	});

	// Block until the command has finished executing.
	FlushRenderingCommands();

	// Reset the streaming system, so it picks up any changes to UTexture2D ResidentMips and RequestedMips.
	ProcessingStage = 0;

	return bSucceeded;
}

/**
 * Adds new textures and level data on the gamethread (while the worker thread isn't active).
 */
void FStreamingManagerTexture::UpdateThreadData()
{
	// Add new textures.
	StreamingTextures.Reserve( StreamingTextures.Num() + PendingStreamingTextures.Num() );
	for ( int32 TextureIndex=0; TextureIndex < PendingStreamingTextures.Num(); ++TextureIndex )
	{
		UTexture2D* Texture = PendingStreamingTextures[ TextureIndex ];
		FStreamingTexture* StreamingTexture = new (StreamingTextures) FStreamingTexture( Texture );
		StreamingTexture->SetStreamingIndex( StreamingTextures.Num() - 1 );
	}
	PendingStreamingTextures.Empty();

	// Remove old levels. Note: Don't try to access the ULevel object, it may have been deleted already!
	for ( int32 LevelIndex=0; LevelIndex < ThreadSettings.LevelData.Num(); ++LevelIndex )
	{
		FLevelData& LevelData = ThreadSettings.LevelData[ LevelIndex ];
		if ( LevelData.Value.bRemove )
		{
			ThreadSettings.LevelData.RemoveAtSwap( LevelIndex-- );
		}
	}

	// Add or remove pending spawned primitives.
	for ( TMap<const UPrimitiveComponent*,FPendingPrimitiveType>::TIterator It(PendingSpawnedPrimitives); It; ++It )
	{
		const UPrimitiveComponent* Primitive = It.Key();
		FPendingPrimitiveType PendingType = It.Value();

		if ( PendingType.bShouldTrack )
		{
			AddDynamicPrimitive( Primitive, PendingType.DynamicType );
		}
		else
		{
			// Remove from our Primitive-to-Instance mapping.
			RemoveDynamicPrimitive( Primitive, PendingType.DynamicType );
		}
	}
	PendingSpawnedPrimitives.Empty();

	// Add new levels.
	for ( int32 LevelIndex=0; LevelIndex < PendingLevels.Num(); ++LevelIndex )
	{
		ULevel* Level = PendingLevels[ LevelIndex ];
		FLevelData* LevelData = new (ThreadSettings.LevelData) FLevelData( Level );
		FThreadLevelData& ThreadLevelData = LevelData->Value;

		// Increase the the force-fully-load refcount.
		for ( TMap<UTexture2D*,bool>::TIterator It(Level->ForceStreamTextures); It; ++It )
		{
			UTexture2D* Texture2D = It.Key();
			if ( Texture2D && FStreamingTexture::IsStreamingTexture( Texture2D ) )
			{
				FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
				check( StreamingTexture.ForceLoadRefCount >= 0 );
				StreamingTexture.ForceLoadRefCount++;
			}
		}

		// Copy all texture instances into ThreadLevelData.
		//Note: ULevel could save it in this form, except it's harder to debug.
		ThreadLevelData.ThreadTextureInstances.Empty( Level->TextureToInstancesMap.Num() );
		for ( TMap<UTexture2D*,TArray<FStreamableTextureInstance> >::TIterator It(Level->TextureToInstancesMap); It; ++It )
		{
			// Convert to SIMD-friendly form
			const UTexture2D* Texture2D = It.Key();
			if ( Texture2D )
			{
				TArray<FStreamableTextureInstance>& TextureInstances = It.Value();
				TArray<FStreamableTextureInstance4>& TextureInstances4 = ThreadLevelData.ThreadTextureInstances.Add( Texture2D, TArray<FStreamableTextureInstance4>() );
				TextureInstances4.Reserve( (TextureInstances.Num() + 3) / 4 );

				for ( int32 InstanceIndex=0; InstanceIndex < TextureInstances.Num(); ++InstanceIndex )
				{
					if ( (InstanceIndex & 3) == 0 )
					{
						TextureInstances4.Add(FStreamableTextureInstance4());
					}
					const FStreamableTextureInstance& Instance = TextureInstances[ InstanceIndex ];
					FStreamableTextureInstance4& Instance4 = TextureInstances4[ InstanceIndex/4 ];
					Instance4.BoundingSphereX[ InstanceIndex & 3 ] = Instance.BoundingSphere.Center.X;
					Instance4.BoundingSphereY[ InstanceIndex & 3 ] = Instance.BoundingSphere.Center.Y;
					Instance4.BoundingSphereZ[ InstanceIndex & 3 ] = Instance.BoundingSphere.Center.Z;
					Instance4.BoundingSphereRadius[ InstanceIndex & 3 ] = Instance.BoundingSphere.W;
					Instance4.TexelFactor[ InstanceIndex & 3 ] = Instance.TexelFactor;
					ensureMsgf(!FMath::IsNearlyZero(Instance.TexelFactor), TEXT("Texture instance %d has a texel factor of zero: %s"), TextureInstances.Num(), *Texture2D->GetPathName());
				}

				// Note: The old instance array must remain, in case a level is added/removed and then added again.
			}
		}
	}
	PendingLevels.Empty();

	// If any views specified an actor to boost, do it now
	for (int32 i = 0; i < CurrentViewInfos.Num(); ++i)
	{
		if (CurrentViewInfos[i].ActorToBoost.IsValid())
		{
			BoostTextures(CurrentViewInfos[i].ActorToBoost.Get(false), BoostPlayerTextures);
		}
	}

	// Cache the view information.
	ThreadSettings.ThreadViewInfos = CurrentViewInfos;
	
	ThreadSettings.MipBias = FMath::Max(CVarStreamingMipBias.GetValueOnGameThread(), 0.0f);

	// Update the thread-safe cache information for dynamic primitives.
	UpdateDynamicPrimitiveCache();
}

/**
 * Temporarily boosts the streaming distance factor by the specified number.
 * This factor is automatically reset to 1.0 after it's been used for mip-calculations.
 */
void FStreamingManagerTexture::BoostTextures( AActor* Actor, float BoostFactor )
{
	if ( Actor )
	{
		TArray<UTexture*> Textures;
		Textures.Empty( 32 );

		TInlineComponentArray<UPrimitiveComponent*> Components;
		Actor->GetComponents(Components);

		for(int32 ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
		{
			UPrimitiveComponent* Primitive = Components[ComponentIndex];
			if ( Primitive->IsRegistered() && Primitive->IsA(UMeshComponent::StaticClass()) )
			{
				Textures.Reset();
				Primitive->GetUsedTextures( Textures, EMaterialQualityLevel::Num );
				for ( int32 TextureIndex=0; TextureIndex < Textures.Num(); ++TextureIndex )
				{
					UTexture2D* Texture2D = Cast<UTexture2D>( Textures[ TextureIndex ] );
					if ( Texture2D && IsManagedStreamingTexture(Texture2D) )
					{
						FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
						StreamingTexture.BoostFactor = FMath::Max( StreamingTexture.BoostFactor, BoostFactor );
					}
				}
			}
		}
	}
}

/**
 * Updates the thread-safe cache information for dynamic primitives.
 */
void FStreamingManagerTexture::UpdateDynamicPrimitiveCache()
{
	STAT( double StartTime = FPlatformTime::Seconds() );

	for ( TMap<const UPrimitiveComponent*,FSpawnedPrimitiveData>::TIterator It(ThreadSettings.SpawnedPrimitives); It; ++It )
	{
		const UPrimitiveComponent* Primitive = It.Key();
		FSpawnedPrimitiveData& PrimitiveData = It.Value();
		bool bIsAttachedNow = Primitive->IsRegistered();
		if ( !PrimitiveData.bPendingUpdate && PrimitiveData.bAttached && !bIsAttachedNow )
		{
			// Mark the texture instances so we don't immediately switch to LastRenderTime unnecessarily.
			SetInstanceRemovedTimestamp( PrimitiveData );
		}
		PrimitiveData.BoundingSphere = Primitive->Bounds.GetSphere();
		PrimitiveData.bAttached = bIsAttachedNow;
	}
	STAT( GStreamingDynamicPrimitivesTime += FPlatformTime::Seconds() - StartTime );
}

bool FStreamingManagerTexture::AddDynamicPrimitive( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
{
	//@TODO: Uncomment this check, if possible.
//	check(Primitive->IsValidComponent());

	int32 NumTexturesAdded = 0;

	if (Primitive && Primitive->IsRegistered())
	{
		TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
		Primitive->GetStreamingTextureInfoWithNULLRemoval( TextureInstanceInfos );

		FSpawnedPrimitiveData* PrimitiveData = NULL;
		TArray<FSpawnedTextureInstance>* TextureInstances = NULL;
		for ( int32 InstanceIndex=0; InstanceIndex < TextureInstanceInfos.Num(); ++InstanceIndex )
		{
			FStreamingTexturePrimitiveInfo& Info = TextureInstanceInfos[InstanceIndex];
			UTexture2D* Texture2D = Cast<UTexture2D>(Info.Texture);
			if ( Texture2D && IsManagedStreamingTexture(Texture2D) && Info.TexelFactor > 0.0f && Info.Bounds.W > 0.0f && ensure(FMath::IsFinite(Info.TexelFactor)) )
			{
				// Create instance array on first use
				if ( TextureInstances == NULL )
				{
					PrimitiveData = &ThreadSettings.SpawnedPrimitives.Add( Primitive, FSpawnedPrimitiveData() );
					TextureInstances = &PrimitiveData->TextureInstances;
				}
				// Check against duplicate instances
				bool bExistedAlready = false;
				for ( int32 ExistingTextureIndex=0; ExistingTextureIndex < TextureInstances->Num(); ++ExistingTextureIndex )
				{
					const FSpawnedTextureInstance& ExistingInstance = (*TextureInstances)[ExistingTextureIndex];
					if ( ExistingInstance.Texture2D == Texture2D &&
						 FMath::IsNearlyEqual(ExistingInstance.TexelFactor, Info.TexelFactor) && 
						 FMath::IsNearlyEqual(ExistingInstance.InvOriginalRadius, 1.0f/Info.Bounds.W) )
					{
						bExistedAlready = true;
						break;
					}
				}
				if ( !bExistedAlready )
				{
					FSpawnedTextureInstance* TextureInstance = new (*TextureInstances) FSpawnedTextureInstance(Texture2D, Info.TexelFactor, Info.Bounds.W);
				}
			}
		}
		if ( TextureInstances )
		{
			PrimitiveData->BoundingSphere	= Primitive->Bounds.GetSphere();
			PrimitiveData->bAttached		= true;
			PrimitiveData->bPendingUpdate	= false;
			PrimitiveData->DynamicType		= DynamicType;
			NumTexturesAdded += TextureInstances->Num();
		}
	}

#if STREAMING_LOG_DYNAMIC
#if 0
	//@DEBUG: To set a breakpoint on a specific asset (e.g., a particular skeletal or static mesh)
	UObject* AssociatedAsset = Primitive->AdditionalStatObject();
	if ((AssociatedAsset != nullptr) && AssociatedAsset->GetName().Contains(TEXT("SpecificMeshName")) )
	{
		static int32 Q=0;
		Q++;
	}
#endif

	UE_LOG(LogContentStreaming, Log, TEXT("AddDynamicPrimitive(0x%08x \"%s\"), IsRegistered=%d, NumTextures=%d"), SIZE_T(Primitive), *Primitive->GetReadableName(), Primitive->IsRegistered(), NumTexturesAdded);
#endif

	if ( NumTexturesAdded > 0 )
	{
		return true;
	}
	else
	{
		// It didn't have any interesting textures.
		return false;
	}
}

bool FStreamingManagerTexture::RemoveDynamicPrimitive( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
{
	// Remove from our Primitive-to-Instance mapping.
	return ThreadSettings.SpawnedPrimitives.Remove( Primitive ) > 0;
}

/** Adds a ULevel to the streaming manager. */
void FStreamingManagerTexture::AddPreparedLevel( ULevel* Level )
{
	PendingLevels.AddUnique( Level );

	if ( bUseDynamicStreaming )
	{
		// Add all dynamic primitives from the ULevel.
		for ( TMap<TWeakObjectPtr<UPrimitiveComponent>,TArray<FDynamicTextureInstance> >::TIterator It(Level->DynamicTextureInstances); It; ++It )
		{
			UPrimitiveComponent* Primitive = It.Key().Get();
			TArray<FDynamicTextureInstance>& TextureInstances = It.Value();
			NotifyPrimitiveAttached( Primitive, DPT_Level );

			//@TODO: This variable is deprecated, remove from serialization.
			TextureInstances.Empty();
		}
	}
}

/** Removes a ULevel from the streaming manager. */
void FStreamingManagerTexture::RemoveLevel( ULevel* Level )
{
	// Remove from pending new levels, if it's been added very recently (this frame)...
	PendingLevels.Remove( Level );

	// Mark the level for removal (will take effect when we're syncing threads).
	FLevelData* LevelData = ThreadSettings.LevelData.FindByKey( Level );
	if ( LevelData && !LevelData->Value.bRemove )
	{
		FThreadLevelData& ThreadLevelData = LevelData->Value;
		ThreadLevelData.bRemove = true;

		// Mark all textures with a timestamp. They're about to lose their location-based heuristic and we don't want them to
		// start using LastRenderTime heuristic for a few seconds until they are garbage collected!
		for( TMap<UTexture2D*,TArray<FStreamableTextureInstance> >::TIterator It(Level->TextureToInstancesMap); It; ++It )
		{
			const UTexture2D* Texture2D = It.Key();
			if ( Texture2D && IsManagedStreamingTexture( Texture2D ) )
			{
				FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
				StreamingTexture.InstanceRemovedTimestamp = FApp::GetCurrentTime();
			}
		}

		// Decrease the the force-fully-load refcount. 
		// Note: Only update ForceLoadRefCount if the level was in the ThreadSettings and therefore bumped ForceLoadRefCount earlier.
		// Note: We do this immediately, since ULevel may be being deleted.
		for ( TMap<UTexture2D*,bool>::TIterator It(Level->ForceStreamTextures); It; ++It )
		{
			UTexture2D* Texture2D = It.Key();
			if ( Texture2D && IsManagedStreamingTexture( Texture2D ) )
			{
				FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
				// Note: This refcount should always be positive, but for some reason it may not be when BuildStreamingData() is called in the editor...
				if ( StreamingTexture.ForceLoadRefCount > 0 )
				{
					StreamingTexture.ForceLoadRefCount--;
				}
			}
		}
	}

	if ( bUseDynamicStreaming )
	{
		// Mark all dynamic primitives from the ULevel (they'll be removed completely next sync).
		for ( TMap<TWeakObjectPtr<UPrimitiveComponent>,TArray<FDynamicTextureInstance> >::TIterator It(Level->DynamicTextureInstances); It; ++It )
		{
			UPrimitiveComponent* Primitive = It.Key().GetEvenIfUnreachable();
			NotifyPrimitiveDetached( Primitive );
		}
	}
}

/**
 * Adds a new texture to the streaming manager.
 */
void FStreamingManagerTexture::AddStreamingTexture( UTexture2D* Texture )
{
	// Adds the new texture to the Pending list, to avoid reallocation of the thread-safe StreamingTextures array.
	check( Texture->StreamingIndex == -1 );
	int32 Index = PendingStreamingTextures.Add( Texture );
	Texture->StreamingIndex = Index;
}

/**
 * Removes a texture from the streaming manager.
 */
void FStreamingManagerTexture::RemoveStreamingTexture( UTexture2D* Texture )
{
	// Removes the texture from the Pending list or marks the StreamingTextures slot as unused.

	// Remove it from the Pending list.
	int32 Index = Texture->StreamingIndex;
	if ( Index >= 0 && Index < PendingStreamingTextures.Num() )
	{
		UTexture2D* ExistingPendingTexture = PendingStreamingTextures[ Index ];
		if ( ExistingPendingTexture == Texture )
		{
			PendingStreamingTextures.RemoveAtSwap( Index );
			if ( Index != PendingStreamingTextures.Num() )
			{
				UTexture2D* SwappedPendingTexture = PendingStreamingTextures[ Index ];
				SwappedPendingTexture->StreamingIndex = Index;
			}
			Texture->StreamingIndex = -1;
		}
	}

	Index = Texture->StreamingIndex;
	if ( Index >= 0 && Index < StreamingTextures.Num() )
	{
		FStreamingTexture& ExistingStreamingTexture = StreamingTextures[ Index ];
		if ( ExistingStreamingTexture.Texture == Texture )
		{
			// If using the new priority system, mark StreamingTextures slot as unused.
			ExistingStreamingTexture.Texture = NULL;
			Texture->StreamingIndex = -1;
		}
	}

//	checkSlow( Texture->StreamingIndex == -1 );	// The texture should have been in one of the two arrays!
	Texture->StreamingIndex = -1;
}

/** Called when an actor is spawned. */
void FStreamingManagerTexture::NotifyActorSpawned( AActor* Actor )
{
	if ( bUseDynamicStreaming )
	{
		TInlineComponentArray<UPrimitiveComponent*> Components;
		Actor->GetComponents(Components);

		for(int32 ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
		{
			UPrimitiveComponent* Primitive = Components[ComponentIndex];
			if (Primitive->IsRegistered() && Primitive->IsA(UMeshComponent::StaticClass()))
			{
				NotifyPrimitiveAttached( Primitive, DPT_Spawned );
			}
		}
	}
}

/** Called when a spawned primitive is deleted. */
void FStreamingManagerTexture::NotifyActorDestroyed( AActor* Actor )
{
	TInlineComponentArray<UPrimitiveComponent*> Components;
	Actor->GetComponents(Components);

	for(int32 ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
	{
		UPrimitiveComponent* Primitive = Components[ComponentIndex];
		if ( Primitive->IsRegistered() )
		{
			NotifyPrimitiveDetached( Primitive );
		}
	}
}

/**
 * Called when a primitive is attached to an actor or another component.
 * Replaces previous info, if the primitive was already attached.
 *
 * @param InPrimitive	Newly attached dynamic/spawned primitive
 */
void FStreamingManagerTexture::NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
{
	// Only add it if it's a UMeshComponent, since we only track those in UMeshComponent::BeginDestroy().
	if ( bUseDynamicStreaming && Primitive && Primitive->IsA(UMeshComponent::StaticClass()) )
	{
#if STREAMING_LOG_DYNAMIC
		UE_LOG(LogContentStreaming, Log, TEXT("NotifyPrimitiveAttached(0x%08x \"%s\"), IsRegistered=%d"), SIZE_T(Primitive), *Primitive->GetReadableName(), Primitive->IsRegistered());
#endif

		// If we already have a pending update, keep its current dynamic type.
		FPendingPrimitiveType* PendingType = PendingSpawnedPrimitives.Find( Primitive );
		if ( PendingType )
		{
			DynamicType = PendingType->DynamicType;
		}

		// If we're already tracking this primitive, keep its current dynamic type.
		FSpawnedPrimitiveData* PrimitiveData = ThreadSettings.SpawnedPrimitives.Find( Primitive );
		if ( PrimitiveData )
		{
			DynamicType = PrimitiveData->DynamicType;
			// Set the pending update flag, to indicate that it shouldn't be processed until it's been fully updated.
			PrimitiveData->bPendingUpdate = true;
		}

		// Mark the primitive for add.
		PendingSpawnedPrimitives.Add( Primitive, FPendingPrimitiveType(DynamicType, true) );
	}
}

/**
 * Called when a primitive is detached from an actor or another component.
 * Note: We should not be accessing the primitive or the UTexture2D after this call!
 */
void FStreamingManagerTexture::NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive )
{
	if ( bUseDynamicStreaming && Primitive )
	{
#if STREAMING_LOG_DYNAMIC
		UE_LOG(LogContentStreaming, Log, TEXT("NotifyPrimitiveDetached(0x%08x \"%s\"), IsRegistered=%d"), SIZE_T(Primitive), *Primitive->GetReadableName(), Primitive->IsRegistered());
#endif

		// Did we already mark this as detached?
		FPendingPrimitiveType* CurrentPendingType = PendingSpawnedPrimitives.Find( Primitive );
		if ( CurrentPendingType && CurrentPendingType->bShouldTrack == false )
		{
			// Don't touch anything.
			return;
		}

		FSpawnedPrimitiveData* PrimitiveData = ThreadSettings.SpawnedPrimitives.Find( Primitive );
		if ( PrimitiveData )
		{
			bool bWasAlreadyPendingUpdate = PrimitiveData->bPendingUpdate;

			// Mark the primitive as detached, if we're already using it.
			PrimitiveData->bAttached = false;

			// Set the pending update flag, to indicate that it shouldn't be processed until it's been fully updated.
			PrimitiveData->bPendingUpdate = true;

			// Queue the primitive for removal.
			PendingSpawnedPrimitives.Add( Primitive, FPendingPrimitiveType(PrimitiveData->DynamicType, false) );

			// Was it already marked for a full refresh?
			if ( bWasAlreadyPendingUpdate )
			{
				// Don't touch it. The original primitive could've been detached (destroyed) at some point recently.
				return;
			}

			SetInstanceRemovedTimestamp( *PrimitiveData );
		}
		else
		{
			PendingSpawnedPrimitives.Remove( Primitive );
		}
	}
}

/**
 * Mark the textures instances with a timestamp. They're about to lose their location-based heuristic and we don't want them to
 * start using LastRenderTime heuristic for a few seconds until they are garbage collected!
 *
 * @param PrimitiveData		Our data structure for the spawned primitive that is being detached.
 */
void FStreamingManagerTexture::SetInstanceRemovedTimestamp( FSpawnedPrimitiveData& PrimitiveData )
{
	for ( int32 InstanceIndex=0; InstanceIndex < PrimitiveData.TextureInstances.Num(); ++InstanceIndex )
	{
		UTexture2D* Texture2D = PrimitiveData.TextureInstances[ InstanceIndex ].Texture2D;
		if ( Texture2D && IsManagedStreamingTexture(Texture2D) )
		{
			FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
			StreamingTexture.InstanceRemovedTimestamp = FApp::GetCurrentTime();
		}
	}
}

/**
 * Called when a primitive has had its textured changed.
 * Only affects primitives that were already attached.
 * Replaces previous info.
 */
void FStreamingManagerTexture::NotifyPrimitiveUpdated( const UPrimitiveComponent* Primitive )
{
	// Only update it if we're currently tracking it, and it's not about to be removed.
	FPendingPrimitiveType* PendingType = PendingSpawnedPrimitives.Find(Primitive);
	bool bIsCurrentlyTracked = PendingType || ThreadSettings.SpawnedPrimitives.Contains(Primitive);
	bool bIsBeingRemoved = PendingType && !PendingType->bShouldTrack;
	if ( bIsCurrentlyTracked && !bIsBeingRemoved )
	{
#if STREAMING_LOG_DYNAMIC
		UE_LOG(LogContentStreaming, Log, TEXT("NotifyPrimitiveUpdated(0x%08x \"%s\"), IsRegistered=%d"), SIZE_T(Primitive), *Primitive->GetReadableName(), Primitive->IsRegistered());
#endif

		// Reattach the primitive, replacing previous info.
		NotifyPrimitiveAttached( Primitive, DPT_Spawned );
	}
}

/**
 * Called when a LastRenderTime primitive is attached to an actor or another component.
 * Modifies the LastRenderTimeRefCount for the textures used, so that those textures can
 * use both distance-based and LastRenderTime heuristics.
 *
 * @param Primitive		Newly attached dynamic/spawned primitive
 */
void FStreamingManagerTexture::NotifyTimedPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
{
	if ( Primitive  && Primitive->IsRegistered())
	{
		TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
		Primitive->GetStreamingTextureInfoWithNULLRemoval( TextureInstanceInfos );

		FSpawnedPrimitiveData* PrimitiveData = NULL;
		TArray<FSpawnedTextureInstance>* TextureInstances = NULL;
		for ( int32 InstanceIndex=0; InstanceIndex < TextureInstanceInfos.Num(); ++InstanceIndex )
		{
			FStreamingTexturePrimitiveInfo& Info = TextureInstanceInfos[InstanceIndex];
			UTexture2D* Texture2D = Cast<UTexture2D>(Info.Texture);
			if ( Texture2D && IsManagedStreamingTexture(Texture2D) )
			{
				// Note: Doesn't have to be cycle-perfect for thread safety.
				FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
				StreamingTexture.LastRenderTimeRefCount++;
				StreamingTexture.LastRenderTimeRefCountTimestamp = FApp::GetCurrentTime();
			}
		}
	}
}

/**
 * Called when a LastRenderTime primitive is detached from an actor or another component.
 * Modifies the LastRenderTimeRefCount for the textures used, so that those textures can
 * use both distance-based and LastRenderTime heuristics.
 *
 * @param Primitive		Newly detached dynamic/spawned primitive
 */
void FStreamingManagerTexture::NotifyTimedPrimitiveDetached( const UPrimitiveComponent* Primitive )
{
	if ( Primitive )
	{
		TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
		Primitive->GetStreamingTextureInfoWithNULLRemoval( TextureInstanceInfos );

		FSpawnedPrimitiveData* PrimitiveData = NULL;
		TArray<FSpawnedTextureInstance>* TextureInstances = NULL;
		for ( int32 InstanceIndex=0; InstanceIndex < TextureInstanceInfos.Num(); ++InstanceIndex )
		{
			FStreamingTexturePrimitiveInfo& Info = TextureInstanceInfos[InstanceIndex];
			UTexture2D* Texture2D = Cast<UTexture2D>(Info.Texture);
			if ( Texture2D && IsManagedStreamingTexture(Texture2D) )
			{
				// Note: Doesn't have to be cycle-perfect for thread safety.
				FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
				if ( StreamingTexture.LastRenderTimeRefCount > 0 )
				{
					StreamingTexture.LastRenderTimeRefCount--;
					StreamingTexture.LastRenderTimeRefCountTimestamp = FApp::GetCurrentTime();
				}
			}
		}
	}
}

/**
 * Returns the corresponding FStreamingTexture for a UTexture2D.
 */
FStreamingTexture& FStreamingManagerTexture::GetStreamingTexture( const UTexture2D* Texture2D )
{
	return StreamingTextures[ Texture2D->StreamingIndex ];
}

/** Returns true if this is a streaming texture that is managed by the streaming manager. */
bool FStreamingManagerTexture::IsManagedStreamingTexture( const UTexture2D* Texture2D )
{
	return Texture2D->StreamingIndex >= 0 && Texture2D->StreamingIndex < StreamingTextures.Num() && FStreamingTexture::IsStreamingTexture( Texture2D );
}

/**
 * Updates streaming for an individual texture, taking into account all view infos.
 *
 * @param Texture	Texture to update
 */
void FStreamingManagerTexture::UpdateIndividualTexture( UTexture2D* Texture )
{
	if (IStreamingManager::Get().IsStreamingEnabled())
	{
		IndividualStreamingTexture = Texture;
		UpdateResourceStreaming( 0.0f );
		IndividualStreamingTexture = NULL;
	}
}

/**
 * Resets the streaming statistics to zero.
 */
void FStreamingManagerTexture::ResetStreamingStats()
{
	NumStreamingTextures								= 0;
	NumRequestsInCancelationPhase						= 0;
	NumRequestsInUpdatePhase							= 0;
	NumRequestsInFinalizePhase							= 0;
	TotalIntermediateTexturesSize						= 0;
	NumIntermediateTextures								= 0;
	TotalStreamingTexturesSize							= 0;
	TotalStreamingTexturesMaxSize						= 0;
	TotalLightmapMemorySize								= 0;
	TotalLightmapDiskSize								= 0;
	TotalMipCountIncreaseRequestsInFlight				= 0;
	TotalOptimalWantedSize								= 0;
	TotalStaticTextureHeuristicSize						= 0;
	TotalDynamicTextureHeuristicSize					= 0;
	TotalLastRenderHeuristicSize						= 0;
	TotalForcedHeuristicSize							= 0;
}

/**
 * Updates the streaming statistics with current frame's worth of stats.
 *
 * @param Context					Context for the current frame
 * @param bAllTexturesProcessed		Whether all processing is complete
 */
void FStreamingManagerTexture::UpdateStreamingStats( const FStreamingContext& Context, bool bAllTexturesProcessed )
{
	NumStreamingTextures					+= Context.ThisFrameNumStreamingTextures;
	NumRequestsInCancelationPhase			+= Context.ThisFrameNumRequestsInCancelationPhase;
	NumRequestsInUpdatePhase				+= Context.ThisFrameNumRequestsInUpdatePhase;
	NumRequestsInFinalizePhase				+= Context.ThisFrameNumRequestsInFinalizePhase;
	TotalIntermediateTexturesSize			= GStreamMemoryTracker.CalcTempMemory();
//	TotalIntermediateTexturesSize			+= Context.ThisFrameTotalIntermediateTexturesSize;
	NumIntermediateTextures					+= Context.ThisFrameNumIntermediateTextures;
	TotalStreamingTexturesSize				+= Context.ThisFrameTotalStreamingTexturesSize;
	TotalStreamingTexturesMaxSize			+= Context.ThisFrameTotalStreamingTexturesMaxSize;
	TotalLightmapMemorySize					+= Context.ThisFrameTotalLightmapMemorySize;
	TotalLightmapDiskSize					+= Context.ThisFrameTotalLightmapDiskSize;
	TotalMipCountIncreaseRequestsInFlight	+= Context.ThisFrameTotalMipCountIncreaseRequestsInFlight;
	TotalOptimalWantedSize					+= Context.ThisFrameOptimalWantedSize;
	TotalStaticTextureHeuristicSize			+= Context.ThisFrameTotalStaticTextureHeuristicSize;
	TotalDynamicTextureHeuristicSize		+= Context.ThisFrameTotalDynamicTextureHeuristicSize;
	TotalLastRenderHeuristicSize			+= Context.ThisFrameTotalLastRenderHeuristicSize;
	TotalForcedHeuristicSize				+= Context.ThisFrameTotalForcedHeuristicSize;


	// Set the stats on wrap-around. Reset happens independently to correctly handle resetting in the middle of iteration.
	if ( bAllTexturesProcessed )
	{
		FTextureMemoryStats Stats;
		RHIGetTextureMemoryStats(Stats);

		SET_DWORD_STAT( STAT_StreamingTextures,				NumStreamingTextures			);
		SET_DWORD_STAT( STAT_RequestsInCancelationPhase,	NumRequestsInCancelationPhase	);
		SET_DWORD_STAT( STAT_RequestsInUpdatePhase,			NumRequestsInUpdatePhase		);
		SET_DWORD_STAT( STAT_RequestsInFinalizePhase,		NumRequestsInFinalizePhase		);
		SET_DWORD_STAT( STAT_IntermediateTexturesSize,		TotalIntermediateTexturesSize	);
		SET_DWORD_STAT( STAT_IntermediateTextures,			NumIntermediateTextures			);
		SET_DWORD_STAT( STAT_StreamingTexturesMaxSize,		TotalStreamingTexturesMaxSize	);
		SET_DWORD_STAT( STAT_LightmapMemorySize,			TotalLightmapMemorySize			);
		SET_DWORD_STAT( STAT_LightmapDiskSize,				TotalLightmapDiskSize			);
		SET_FLOAT_STAT( STAT_StreamingBandwidth,			BandwidthAverage/1024.0f/1024.0f);
		SET_FLOAT_STAT( STAT_StreamingLatency,				LatencyAverage					);
		SET_MEMORY_STAT( STAT_StreamingTexturesSize,		TotalStreamingTexturesSize		);
		SET_MEMORY_STAT( STAT_OptimalTextureSize,			TotalOptimalWantedSize			);
		SET_MEMORY_STAT( STAT_TotalStaticTextureHeuristicSize,	TotalStaticTextureHeuristicSize	);
		SET_MEMORY_STAT( STAT_TotalDynamicHeuristicSize,		TotalDynamicTextureHeuristicSize );
		SET_MEMORY_STAT( STAT_TotalLastRenderHeuristicSize,		TotalLastRenderHeuristicSize	);
		SET_MEMORY_STAT( STAT_TotalForcedHeuristicSize,			TotalForcedHeuristicSize		);
#if 0 // this is a visualization thing, not a stats thing

#if STATS_FAST
		MaxStreamingTexturesSize = FMath::Max(MaxStreamingTexturesSize, GET_MEMORY_STAT(STAT_StreamingTexturesSize));
		MaxOptimalTextureSize = FMath::Max(MaxOptimalTextureSize, GET_MEMORY_STAT(STAT_OptimalTextureSize));
		MaxStreamingOverBudget = FMath::Max<int64>(MaxStreamingOverBudget, GET_MEMORY_STAT(STAT_StreamingOverBudget) - GET_MEMORY_STAT(STAT_StreamingUnderBudget));
		MaxTexturePoolAllocatedSize = FMath::Max(MaxTexturePoolAllocatedSize, GET_MEMORY_STAT(STAT_TexturePoolAllocatedSize));
#if STATS
		MinLargestHoleSize = FMath::Min(MinLargestHoleSize, GET_MEMORY_STAT(STAT_TexturePool_LargestHole));
#endif // STATS
		MaxNumWantingTextures = FMath::Max(MaxNumWantingTextures, GET_MEMORY_STAT(STAT_NumWantingTextures));
#endif
#endif
	}

	SET_DWORD_STAT(		STAT_RequestSizeCurrentFrame,		Context.ThisFrameTotalRequestSize		);
	INC_DWORD_STAT_BY(	STAT_RequestSizeTotal,				Context.ThisFrameTotalRequestSize		);
	INC_DWORD_STAT_BY(	STAT_LightmapRequestSizeTotal,		Context.ThisFrameTotalLightmapRequestSize );
}

/**
 * Not thread-safe: Updates a portion (as indicated by 'StageIndex') of all streaming textures,
 * allowing their streaming state to progress.
 *
 * @param Context			Context for the current stage (frame)
 * @param StageIndex		Current stage index
 * @param NumUpdateStages	Number of texture update stages
 */
void FStreamingManagerTexture::UpdateStreamingTextures( FStreamingContext& Context, int32 StageIndex, int32 NumUpdateStages )
{
	if ( StageIndex == 0 )
	{
		CurrentUpdateStreamingTextureIndex = 0;
	}
	int32 StartIndex = CurrentUpdateStreamingTextureIndex;
	int32 EndIndex = StreamingTextures.Num() * (StageIndex + 1) / NumUpdateStages;
	for ( int32 Index=StartIndex; Index < EndIndex; ++Index )
	{
		FStreamingTexture& StreamingTexture = StreamingTextures[ Index ];
		FPlatformMisc::Prefetch( &StreamingTexture + 1 );
		FPlatformMisc::Prefetch( StreamingTexture.Texture );
		FPlatformMisc::Prefetch( StreamingTexture.Texture, CACHE_LINE_SIZE );

		// Is this texture marked for removal?
		if ( StreamingTexture.Texture == NULL )
		{
			StreamingTextures.RemoveAtSwap( Index );
			if ( Index != StreamingTextures.Num() )
			{
				FStreamingTexture& SwappedTexture = StreamingTextures[ Index ];
				// Note: The swapped texture may also be pending deletion.
				if ( SwappedTexture.Texture )
				{
					SwappedTexture.Texture->StreamingIndex = Index;
				}
			}
			--Index;
			--EndIndex;
			continue;
		}

		StreamingTexture.UpdateCachedInfo();

		if ( StreamingTexture.bReadyForStreaming )
		{
			UpdateTextureStatus( StreamingTexture, Context );
		}
	}
	CurrentUpdateStreamingTextureIndex = EndIndex;
}



/**
 * Stream textures in/out, based on the priorities calculated by the async work.
 * @param bProcessEverything	Whether we're processing all textures in one go
 */
void FStreamingManagerTexture::StreamTextures( bool bProcessEverything )
{
	// Setup a context for this run.
	FStreamingContext Context( bProcessEverything, IndividualStreamingTexture, bCollectTextureStats );

	const TArray<FTexturePriority>& PrioritizedTextures = AsyncWork->GetTask().GetPrioritizedTextures();
	FAsyncTextureStreaming::FAsyncStats ThreadStats = AsyncWork->GetTask().GetStats();
	Context.AddStats( AsyncWork->GetTask().GetContext() );

#if STATS
	// Did we collect texture stats? Triggered by the ListStreamingTextures exec command.
	if ( Context.TextureStats.Num() > 0 )
	{
		// Reinitialize each time
		TextureStatsReport.Empty();

		// Sort textures by cost.
		struct FCompareFTextureStreamingStats
		{
			FORCEINLINE bool operator()( const FTextureStreamingStats& A, const FTextureStreamingStats& B ) const
			{
				if ( A.Priority > B.Priority )
				{
					return true;
				}
				else if ( A.Priority == B.Priority )
				{
					return ( A.TextureIndex < B.TextureIndex );
				}
				else
				{
					return false;
				}
			}
		};
		Context.TextureStats.Sort( FCompareFTextureStreamingStats() );
		int64 TotalCurrentSize	= 0;
		int64 TotalWantedSize	= 0;
		int64 TotalMaxSize		= 0;
		TextureStatsReport.Add( FString( TEXT(",Priority,Current,Wanted,Max,Largest Resident,Current Size (KB),Wanted Size (KB),Max Size (KB),Largest Resident Size (KB),Streaming Type,Last Rendered,BoostFactor,Name") ) );
		for( int32 TextureIndex=0; TextureIndex<Context.TextureStats.Num(); TextureIndex++ )
		{
			const FTextureStreamingStats& TextureStat = Context.TextureStats[TextureIndex];
			int32 LODBias			= TextureStat.LODBias;
			int32 CurrDroppedMips	= TextureStat.NumMips - TextureStat.ResidentMips;
			int32 WantedDroppedMips	= TextureStat.NumMips - TextureStat.WantedMips;
			int32 MostDroppedMips	= TextureStat.NumMips - TextureStat.MostResidentMips;
			TextureStatsReport.Add( FString::Printf( TEXT(",%.3f,%ix%i,%ix%i,%ix%i,%ix%i,%i,%i,%i,%i,%s,%3f sec,%.1f,%s"),
				TextureStat.Priority,
				FMath::Max(TextureStat.SizeX >> CurrDroppedMips, 1),
				FMath::Max(TextureStat.SizeY >> CurrDroppedMips, 1),
				FMath::Max(TextureStat.SizeX >> WantedDroppedMips, 1),
				FMath::Max(TextureStat.SizeY >> WantedDroppedMips, 1),
				FMath::Max(TextureStat.SizeX >> LODBias, 1),
				FMath::Max(TextureStat.SizeY >> LODBias, 1),
				FMath::Max(TextureStat.SizeX >> MostDroppedMips, 1),
				FMath::Max(TextureStat.SizeY >> MostDroppedMips, 1),
				TextureStat.ResidentSize/1024,
				TextureStat.WantedSize/1024,
				TextureStat.MaxSize/1024,
				TextureStat.MostResidentSize/1024,
				GStreamTypeNames[TextureStat.StreamType],
				TextureStat.LastRenderTime,
				TextureStat.BoostFactor,
				*TextureStat.TextureName
				) );
			TotalCurrentSize	+= TextureStat.ResidentSize;
			TotalWantedSize		+= TextureStat.WantedSize;
			TotalMaxSize		+= TextureStat.MaxSize;
		}
		TextureStatsReport.Add( FString::Printf( TEXT("Total size: Current= %d  Wanted=%d  Max= %d"), TotalCurrentSize/1024, TotalWantedSize/1024, TotalMaxSize/1024 ) );
		Context.TextureStats.Empty();

		if( bReportTextureStats )
		{
			if( TextureStatsReport.Num() > 0 )
			{
				UE_LOG(LogContentStreaming, Log,  TEXT("Listing collected stats for all streaming textures") );
				for( int32 ReportIndex = 0; ReportIndex < TextureStatsReport.Num(); ReportIndex++ )
				{
					UE_LOG(LogContentStreaming, Log, TEXT("%s"), *TextureStatsReport[ReportIndex]);
				}
				TextureStatsReport.Empty();
			}
			bReportTextureStats = false;
		}
		bCollectTextureStats = false;
	}
#endif

	FTextureMemoryStats Stats;
	RHIGetTextureMemoryStats(Stats);
	ThreadStats.PendingStreamInSize += (Stats.PendingMemoryAdjustment > 0) ? Stats.PendingMemoryAdjustment : 0;
	ThreadStats.PendingStreamOutSize += (Stats.PendingMemoryAdjustment < 0) ? -Stats.PendingMemoryAdjustment : 0;

	int64 AvailableNow, AvailableLater, TempMemoryUsed;
	if ( Stats.IsUsingLimitedPoolSize() )
	{
		ThreadStats.TempStreamingSize = GStreamMemoryTracker.CalcTempMemory();
		TempMemoryUsed = ThreadStats.TempStreamingSize;

		// Note: 'AvailableNow' and 'AvailableLater' are allowed to be negative.
		AvailableNow = GStreamMemoryTracker.CalcAvailableNow( Stats.ComputeAvailableMemorySize(), MemoryMargin );
		AvailableLater = GStreamMemoryTracker.CalcAvailableLater( Stats.ComputeAvailableMemorySize(), MemoryMargin );

		STAT( int64 NonStreamingUsage = Stats.AllocatedMemorySize - int64(ThreadStats.TotalResidentSize) - int64(ThreadStats.PendingStreamInSize) - int64(TempMemoryUsed) );
		STAT( int64 MemoryBudget = Stats.TexturePoolSize - NonStreamingUsage - int64(MemoryMargin) );
		STAT( int64 MemoryOverBudget = int64(ThreadStats.TotalRequiredSize) - MemoryBudget );
		SET_MEMORY_STAT( STAT_StreamingOverBudget,	FMath::Max(MemoryOverBudget,0ll) );
		SET_MEMORY_STAT( STAT_StreamingUnderBudget,	FMath::Max(-MemoryOverBudget,0ll) );
		SET_MEMORY_STAT( STAT_NonStreamingTexturesSize,	FMath::Max<int64>(NonStreamingUsage, 0) );
	}
	else
	{
		TempMemoryUsed = ThreadStats.TempStreamingSize;
		AvailableNow = MAX_int64;
		AvailableLater = MAX_int64;
		SET_MEMORY_STAT( STAT_StreamingOverBudget, 0 );
		SET_MEMORY_STAT( STAT_StreamingUnderBudget, 0 );
	}
	NumWantingResources = ThreadStats.NumWantingTextures;
	NumWantingResourcesCounter++;
	SET_DWORD_STAT( STAT_NumWantingTextures, ThreadStats.NumWantingTextures );

	if ( Stats.IsUsingLimitedPoolSize() && !bPauseTextureStreaming )
	{
		// Stream in high-priority textures if possible, and stream out as many low-priority textures as we need.
		// Keep track of how much temp memory we use separately, not letting it go over a certain threshold.
		int32 HighPrioIndex = 0;
		int32 LowPrioIndex = PrioritizedTextures.Num() - 1;

		FMemMark Mark(FMemStack::Get());
		FStreamingRequests StreamingRequests;
		int32 LowPrioIndex_UnwantedMips = LowPrioIndex;

		while ( HighPrioIndex <= LowPrioIndex && TempMemoryUsed < MaxTempMemoryUsed )
		{
			const FTexturePriority& TexturePriority = PrioritizedTextures[ HighPrioIndex ];
			FStreamingTexture& HighPrioTexture = StreamingTextures[ TexturePriority.TextureIndex ];
			bool bStreamInTexture = false;

			// Check that texture hasn't been marked for removal.
			if ( HighPrioTexture.Texture != NULL )
			{
				// Do we want to cancel unload? Do we want more mips than requested?
				if ( HighPrioTexture.bInFlight && HighPrioTexture.RequestedMips < HighPrioTexture.ResidentMips && HighPrioTexture.RequestedMips < HighPrioTexture.WantedMips )
				{
					// Should we cancel?
					int64 RequestedSize = HighPrioTexture.GetSize( HighPrioTexture.RequestedMips );
					int64 StreamSize = HighPrioTexture.GetSize( HighPrioTexture.ResidentMips ) - RequestedSize;
					if ( StreamSize > AvailableLater )
					{
						// Try to cancel.
						bool bCancelled = CancelStreamingRequest( HighPrioTexture );
						if ( bCancelled )
						{
							AvailableLater -= StreamSize;
						}
					}
				}

				// Would we like to stream in this texture?
				if ( HighPrioTexture.bInFlight == false && HighPrioTexture.WantedMips > HighPrioTexture.ResidentMips )
				{
					//@TODO: TempSize=0 when using in-place reallocation, but we're conservative here.
					int64 TempSize = HighPrioTexture.GetSize( HighPrioTexture.ResidentMips );
					int64 StreamSize = HighPrioTexture.GetSize( HighPrioTexture.WantedMips ) - TempSize;
					AvailableLater -= StreamSize;

					// Can we stream in this texture now?
					if ( StreamSize <= AvailableNow && TempMemoryUsed < MaxTempMemoryUsed )
					{
						AvailableNow -= StreamSize;
						TempMemoryUsed += TempSize;
						StartStreaming( HighPrioTexture, HighPrioTexture.WantedMips, Context, true );
					}
				}
			}

			// Do we need to stream something out?
			if ( AvailableLater < 0 )
			{
				// Try streaming out the unwanted mips, from LowPrioIndex_UnwantedMips to 0.
				LowPrioIndex_UnwantedMips = StreamoutTextures( StreamOut_UnwantedMips, AvailableLater, TempMemoryUsed, LowPrioIndex_UnwantedMips, 0, LowPrioIndex, PrioritizedTextures, StreamingRequests );
			}

			// Do we STILL need to stream something out?
			if ( AvailableLater < 0 )
			{
				// Try streaming out all mips, from LowPrioIndex to HighPrioIndex.
				// Note: AvailableLater, TempMemoryUsed, LowPriorityIndex and StreamingRequests may be altered by this call.
				int32 TriedIndex = StreamoutTextures( StreamOut_AllMips, AvailableLater, TempMemoryUsed, LowPrioIndex, HighPrioIndex, LowPrioIndex, PrioritizedTextures, StreamingRequests );
				if ( LowPrioIndex_UnwantedMips > TriedIndex )
				{
					LowPrioIndex_UnwantedMips = TriedIndex;
				}
			}
			HighPrioIndex++;
		}

		for ( int32 RequestIndex=0; RequestIndex < StreamingRequests.Num(); ++RequestIndex )
		{
			int32 StreamingTextureIndex = StreamingRequests[ RequestIndex ];
			FStreamingTexture& StreamingTexture = StreamingTextures[ StreamingTextureIndex ];
			StartStreaming( StreamingTexture, StreamingTexture.RequestedMips, Context, true );
		}

		Mark.Pop();
	}
	else if ( !bPauseTextureStreaming )
	{
		// Simple streaming when no texture pool with limited memory size is used.
		StreamTexturesUnlimited( Context, PrioritizedTextures, TempMemoryUsed );
	}

	UpdateStreamingStats( Context, true );

	STAT( StreamingTimes[ProcessingStage] += FPlatformTime::Seconds() );
}

/**
 * Try to stream out textures based on the specified logic.
 *
 * @param StreamoutLogic		The logic to use for streaming out
 * @param AvailableLater		[in/out] Estimated amount of memory that will be free at a later time (after all pending stream in/out)
 * @param TempMemoryUsed		[in/out] Estimated amount of temp memory required for streaming
 * @param StartIndex			First priority index to try
 * @param StopIndex				Last priority index to try
 * @param LowPrioIndex			[in/out] The lowest-priority texture that can stream out
 * @param PrioritizedTextures	Indices to the working set of streaming textures, sorted from highest priority to lowest
 * @param StreamingRequests		[in/out] Indices to textures that are going to be streamed this frame
 * @return						The last priority index considered (may be sooner that StopIndex)
 */
int32 FStreamingManagerTexture::StreamoutTextures( FStreamoutLogic StreamoutLogic, int64& AvailableLater, int64& TempMemoryUsed, int32 StartIndex, int32 StopIndex, int32& LowPrioIndex, const TArray<FTexturePriority>& PrioritizedTextures, FStreamingRequests& StreamingRequests )
{
	// Only bump LowPrioIndex if we're starting from there.
	bool bBumpLowPrioIndex = (StartIndex == LowPrioIndex);

	int32 Index = StartIndex;
	for ( ; AvailableLater < 0 && Index > StopIndex && TempMemoryUsed < MaxTempMemoryUsed; --Index )
	{
		const FTexturePriority& TexturePriority = PrioritizedTextures[ Index ];
		FStreamingTexture& StreamingTexture = StreamingTextures[ TexturePriority.TextureIndex ];

		// Check that this texture hasn't been marked for removal, and that we can perform streaming actions on it.
		if ( StreamingTexture.Texture != NULL && StreamingTexture.bReadyForStreaming )
		{
			if ( StreamingTexture.bInFlight )
			{
				// Cancel load operation?
				bool bIsLoadRequest = StreamingTexture.RequestedMips > StreamingTexture.ResidentMips;
				if ( bIsLoadRequest &&
					 (StreamoutLogic == StreamOut_AllMips ||
					 (StreamoutLogic == StreamOut_UnwantedMips && StreamingTexture.RequestedMips > StreamingTexture.WantedMips) ) )
				{
					// Try to cancel.
					bool bCancelled = CancelStreamingRequest( StreamingTexture );
					if ( bCancelled )
					{
						int64 CurrentSize = StreamingTexture.GetSize( StreamingTexture.ResidentMips );
						int64 StreamSize = StreamingTexture.GetSize( StreamingTexture.RequestedMips ) - CurrentSize;
						AvailableLater += StreamSize;
					}
				}
			}
			else
			{
				int32 WantedMips = ( StreamoutLogic == StreamOut_AllMips ) ?  StreamingTexture.MinAllowedMips : StreamingTexture.WantedMips;

				// Note: When first attempting streamout, RequestedMips == ResidentMips. After the first attempt, it will be what we attempted last.
				int32 CurrentMips = StreamingTexture.RequestedMips;

				// Can we stream out this texture?
				if ( WantedMips < CurrentMips )
				{
					//@TODO: Temp memory used=0 when using in-place reallocation. Though this is conservative and should be fine.
					int64 CurrentSize = StreamingTexture.GetSize( CurrentMips );
					int64 StreamSize = CurrentSize - StreamingTexture.GetSize( WantedMips );
					AvailableLater += StreamSize;
					TempMemoryUsed += CurrentSize;

					// If we haven't added it to the request array yet, do so now.
					if ( StreamingTexture.RequestedMips == StreamingTexture.ResidentMips )
					{
						StreamingRequests.Add( TexturePriority.TextureIndex );
					}
					StreamingTexture.RequestedMips = WantedMips;
				}

				// Can stream out more from this texture later, if we need to?
				if ( StreamingTexture.RequestedMips > StreamingTexture.MinAllowedMips )
				{
					bBumpLowPrioIndex = false;
				}
			}
		}

		// Bump the low prio index when we can't do anything more with the lowest-priority textures.
		if ( bBumpLowPrioIndex )
		{
			LowPrioIndex--;
		}
	}

	return Index;
}


/**
 * Stream textures in/out, when no texture pool with limited size is used by the platform.
 *
 * @param Context				Context for the current stage
 * @param PrioritizedTextures	Array of prioritized textures to process
 * @param TempMemoryUsed		Current amount of temporary memory used by the streaming system, in bytes
 */
void FStreamingManagerTexture::StreamTexturesUnlimited( FStreamingContext& Context, const TArray<FTexturePriority>& PrioritizedTextures, int64 TempMemoryUsed )
{
	for ( int32 PrioIndex=0; PrioIndex < PrioritizedTextures.Num() && TempMemoryUsed < MaxTempMemoryUsed; ++PrioIndex )
	{
		const FTexturePriority& TexturePriority = PrioritizedTextures[ PrioIndex ];
		FStreamingTexture& StreamingTexture = StreamingTextures[ TexturePriority.TextureIndex ];

		// Has this texture been marked for removal?
		if ( StreamingTexture.Texture == NULL )
		{
			// Ignore it.
		}
		// Cancel load operation?
		else if ( StreamingTexture.bInFlight && StreamingTexture.RequestedMips > StreamingTexture.ResidentMips && StreamingTexture.RequestedMips > StreamingTexture.WantedMips )
		{
			if (!GNeverStreamOutTextures)
			{
				CancelStreamingRequest( StreamingTexture );
			}
		}
		// Cancel unload operation?
		else if ( StreamingTexture.bInFlight && StreamingTexture.RequestedMips < StreamingTexture.ResidentMips && StreamingTexture.RequestedMips < StreamingTexture.WantedMips )
		{
			CancelStreamingRequest( StreamingTexture );
		}
		// Stream this texture?
		else if ( StreamingTexture.bInFlight == false && StreamingTexture.ResidentMips != StreamingTexture.WantedMips )
		{
			if (!GNeverStreamOutTextures || StreamingTexture.ResidentMips < StreamingTexture.WantedMips)
			{
				//@TODO: Temp memory used=0 when using in-place reallocation.
				int64 CurrentSize = StreamingTexture.GetSize( StreamingTexture.ResidentMips );
				TempMemoryUsed += CurrentSize;
				StartStreaming( StreamingTexture, StreamingTexture.WantedMips, Context, true );
			}
		}
	}
}

void FStreamingManagerTexture::CheckUserSettings()
{	
	int32 PoolSizeSetting = CVarStreamingPoolSize.GetValueOnGameThread();
	int32 FixedPoolSizeSetting = CVarStreamingUseFixedPoolSize.GetValueOnGameThread();

	if (FixedPoolSizeSetting == 0)
	{
		int64 TexturePoolSize = 0;
		if (PoolSizeSetting == -1)
		{
			FTextureMemoryStats Stats;
			RHIGetTextureMemoryStats(Stats);
			if ( GPoolSizeVRAMPercentage > 0 && Stats.TotalGraphicsMemory > 0 )
			{
				int64 TotalGraphicsMemory = Stats.TotalGraphicsMemory;
				if ( GCurrentRendertargetMemorySize > 0 )
				{
					TotalGraphicsMemory -= int64(GCurrentRendertargetMemorySize) * 1024;
				}
				float PoolSize = float(GPoolSizeVRAMPercentage) * 0.01f * float(TotalGraphicsMemory);

				// Truncate the pool size to MB (but still count in bytes)
				TexturePoolSize = int64(FGenericPlatformMath::TruncToFloat(PoolSize / 1024.0f / 1024.0f)) * 1024 * 1024;
			}
			else
			{
				TexturePoolSize = OriginalTexturePoolSize;
			}
		}
		else
		{
			int64 PoolSize = int64(PoolSizeSetting) * 1024ll * 1024ll;
			TexturePoolSize = PoolSize;
		}

		// Only adjust the pool size once every 10 seconds, but immediately in some other cases.
		if ( PoolSizeSetting != PreviousPoolSizeSetting ||
			 TexturePoolSize > GTexturePoolSize ||
			 (FApp::GetCurrentTime() - PreviousPoolSizeTimestamp) > 10.0 )
		{
			if ( TexturePoolSize != GTexturePoolSize )
			{
				UE_LOG(LogContentStreaming,Log,TEXT("Texture pool size now %d MB"), int(TexturePoolSize/1024/1024));
			}
			PreviousPoolSizeSetting = PoolSizeSetting;
			PreviousPoolSizeTimestamp = FApp::GetCurrentTime();
			GTexturePoolSize = TexturePoolSize;
		}
	}
}

/**
 * Main function for the texture streaming system, based on texture priorities and asynchronous processing.
 * Updates streaming, taking into account all view infos.
 *
 * @param DeltaTime				Time since last call in seconds
 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
 */
void FStreamingManagerTexture::UpdateResourceStreaming( float DeltaTime, bool bProcessEverything/*=false*/ )
{
	SCOPE_CYCLE_COUNTER(STAT_GameThreadUpdateTime);

#if STREAMING_LOG_VIEWCHANGES
	static bool bWasLocationOveridden = false;
	bool bIsLocationOverriden = false;
	for ( int32 ViewIndex=0; ViewIndex < CurrentViewInfos.Num(); ++ViewIndex )
	{
		FStreamingViewInfo& ViewInfo = CurrentViewInfos( ViewIndex );
		if ( ViewInfo.bOverrideLocation )
		{
			bIsLocationOverriden = true;
			break;
		}
	}
	if ( bIsLocationOverriden != bWasLocationOveridden )
	{
		UE_LOG(LogContentStreaming, Log, TEXT("Texture streaming view location is now %s."), bIsLocationOverriden ? TEXT("OVERRIDEN") : TEXT("normal") );
		bWasLocationOveridden = bIsLocationOverriden;
	}
#endif

	int32 OldNumTextureProcessingStages = NumTextureProcessingStages;
	if ( bProcessEverything || IndividualStreamingTexture )
	{
		AsyncWork->EnsureCompletion();
		ProcessingStage = 0;
		NumTextureProcessingStages = 1;
	}

#if STATS
	if ( ProcessingStage == 0 )
	{
		STAT( StreamingTimes.Empty( NumTextureProcessingStages ) );
		STAT( StreamingTimes.AddZeroed( NumTextureProcessingStages ) );
	}
	STAT( StreamingTimes[ProcessingStage] -= FPlatformTime::Seconds() );
#endif

	// Init.
	if ( ProcessingStage == 0 )
	{
		// Is the AsyncWork is running for some reason? (E.g. we reset the system by simply setting ProcessingStage to 0.)
		if ( AsyncWork->IsDone() == false )
		{
			AsyncWork->EnsureCompletion();
		}

		CheckUserSettings();

		ResetStreamingStats();
		UpdateThreadData();

		//@TODO: Move this to the worker thread once it's thread-safe.
		//Note: Must be done with updated primitives (right after UpdateThreadData), because we can't ignore a primitive just because it moved.
		CalcDynamicWantedMips();

		if ( bTriggerDumpTextureGroupStats )
		{
			DumpTextureGroupStats( bDetailedDumpTextureGroupStats );
		}
		if ( bTriggerInvestigateTexture )
		{
			InvestigateTexture( InvestigateTextureName );
		}
	}

	// Non-threaded data collection.
	int32 NumDataCollectionStages = FMath::Max( NumTextureProcessingStages - 1, 1 );
	if ( ProcessingStage < NumDataCollectionStages )
	{
		// Setup a context for this run. Note that we only (potentially) collect texture stats from the AsyncWork.
		FStreamingContext Context( bProcessEverything, IndividualStreamingTexture, false );
		UpdateStreamingTextures( Context, ProcessingStage, NumDataCollectionStages );
		UpdateStreamingStats( Context, false );
	}

	// Start async task after the last data collection stage (if we're not paused).
	if ( ProcessingStage == NumDataCollectionStages - 1 && bPauseTextureStreaming == false )
	{
		// Is the AsyncWork is running for some reason? (E.g. we reset the system by simply setting ProcessingStage to 0.)
		if ( AsyncWork->IsDone() == false )
		{
			AsyncWork->EnsureCompletion();
		}

		AsyncWork->GetTask().Reset(bCollectTextureStats);
		if ( NumTextureProcessingStages > 1 )
		{
			AsyncWork->StartBackgroundTask();
		}
		else
		{
			// Perform the work synchronously on this thread.
			AsyncWork->StartSynchronousTask();
		}
	}

	// Are we still in the non-threaded data collection stages?
	if ( ProcessingStage < NumTextureProcessingStages - 1 )
	{
		STAT( StreamingTimes[ProcessingStage] += FPlatformTime::Seconds() );
		ProcessingStage++;
	}
	// Are we waiting for the async work to finish?
	else if ( AsyncWork->IsDone() == false )
	{
		STAT( StreamingTimes[ProcessingStage] += FPlatformTime::Seconds() );
	}
	else
	{
		// All priorities have been calculated, do all streaming.
		StreamTextures( bProcessEverything );
		ProcessingStage = 0;
	}

	NumTextureProcessingStages = OldNumTextureProcessingStages;
	SET_FLOAT_STAT( STAT_DynamicStreamingTotal, float(GStreamingDynamicPrimitivesTime) );
}

/**
 * Blocks till all pending requests are fulfilled.
 *
 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
 * @param bLogResults	Whether to dump the results to the log.
 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
 */
int32 FStreamingManagerTexture::BlockTillAllRequestsFinished( float TimeLimit /*= 0.0f*/, bool bLogResults /*= false*/ )
{
	double StartTime = FPlatformTime::Seconds();
	float ElapsedTime = 0.0f;

	FMemMark Mark(FMemStack::Get());

	int32 NumPendingUpdates = 0;
	int32 MaxPendingUpdates = 0;

	// Add all textures to the initial pending array.
	TArray<int32, TMemStackAllocator<> > PendingTextures[2];
	PendingTextures[0].Empty( StreamingTextures.Num() );
	for ( int32 TextureIndex=0; TextureIndex < StreamingTextures.Num(); ++TextureIndex )
	{
		PendingTextures[0].Add( TextureIndex );
	}

	int32 CurrentArray = 0;
	do 
	{
		// Flush rendering commands.
		FlushRenderingCommands();

		// Update the textures in the current pending array, and add the ones that are still pending to the other array.
		PendingTextures[1 - CurrentArray].Empty( StreamingTextures.Num() );
		for ( int32 Index=0; Index < PendingTextures[CurrentArray].Num(); ++Index )
		{
			int32 TextureIndex = PendingTextures[CurrentArray][ Index ];
			FStreamingTexture& StreamingTexture = StreamingTextures[ TextureIndex ];

			// Make sure this streaming texture hasn't been marked for removal.
			if ( StreamingTexture.Texture )
			{
				if ( StreamingTexture.Texture->UpdateStreamingStatus() )
				{
					PendingTextures[ 1 - CurrentArray ].Add( TextureIndex );
				}
				TrackTextureEvent( &StreamingTexture, StreamingTexture.Texture, StreamingTexture.bForceFullyLoad, this );
			}
		}

		// Swap arrays.
		CurrentArray = 1 - CurrentArray;

		NumPendingUpdates = PendingTextures[ CurrentArray ].Num();
		MaxPendingUpdates = FMath::Max( MaxPendingUpdates, NumPendingUpdates );

		// Check for time limit.
		ElapsedTime = float(FPlatformTime::Seconds() - StartTime);
		if ( TimeLimit > 0.0f && ElapsedTime > TimeLimit )
		{
			break;
		}

		if ( NumPendingUpdates )
		{
			FPlatformProcess::Sleep( 0.010f );
		}
	} while ( NumPendingUpdates );

	Mark.Pop();

	if ( bLogResults )
	{
		UE_LOG(LogContentStreaming, Log, TEXT("Blocking on texture streaming: %.1f ms (%d textures updated, %d still pending)"), ElapsedTime*1000.0f, MaxPendingUpdates, NumPendingUpdates );
#if STREAMING_LOG_VIEWCHANGES
		for ( int32 ViewIndex=0; ViewIndex < CurrentViewInfos.Num(); ++ViewIndex )
		{
			FStreamingViewInfo& ViewInfo = CurrentViewInfos( ViewIndex );
			if ( ViewInfo.bOverrideLocation )
			{
				UE_LOG(LogContentStreaming, Log, TEXT("Texture streaming view: X=%1.f, Y=%.1f, Z=%.1f (Override=%d, Boost=%.1f)"), ViewInfo.ViewOrigin.X, ViewInfo.ViewOrigin.Y, ViewInfo.ViewOrigin.Z, ViewInfo.bOverrideLocation, ViewInfo.BoostFactor );
				break;
			}
		}
#endif
	}
	return NumPendingUpdates;
}

/**
 * Adds a textures streaming handler to the array of handlers used to determine which
 * miplevels need to be streamed in/ out.
 *
 * @param TextureStreamingHandler	Handler to add
 */
void FStreamingManagerTexture::AddTextureStreamingHandler( FStreamingHandlerTextureBase* TextureStreamingHandler )
{
	AsyncWork->EnsureCompletion();
	TextureStreamingHandlers.Add( TextureStreamingHandler );
}

/**
 * Removes a textures streaming handler from the array of handlers used to determine which
 * miplevels need to be streamed in/ out.
 *
 * @param TextureStreamingHandler	Handler to remove
 */
void FStreamingManagerTexture::RemoveTextureStreamingHandler( FStreamingHandlerTextureBase* TextureStreamingHandler )
{
	AsyncWork->EnsureCompletion();
	TextureStreamingHandlers.Remove( TextureStreamingHandler );
}

/**
 * Calculates the minimum and maximum number of mip-levels for a streaming texture.
 */
void FStreamingManagerTexture::CalcMinMaxMips( FStreamingTexture& StreamingTexture )
{
	int32 TextureLODBias = StreamingTexture.TextureLODBias;

	// Figure out whether texture should be forced resident based on bools and forced resident time.
	if( StreamingTexture.bForceFullyLoad )
	{
		// If the texture has cinematic high-res mip-levels, allow them to be streamed in.
		TextureLODBias = FMath::Max( TextureLODBias - StreamingTexture.NumCinematicMipLevels, 0 );
	}

	// We only stream in skybox textures as you are always within the bounding box and those textures
	// tend to be huge. A streaming fudge factor fluctuating around 1 will cause them to be streamed in
	// and out, making it likely for the allocator to fragment.
	if( StreamingTexture.LODGroup == TEXTUREGROUP_Skybox )
	{
		StreamingTexture.bForceFullyLoad = true;
	}

	// Don't stream in all referenced textures but rather only those that have been rendered in the last 5 minutes if
	// we only stream in textures. This means you still might see texture popping, but the option is designed to avoid
	// hitching due to CPU overhead, which is still taken care off by the 5 minute rule.
	static auto CVarOnlyStreamInTextures = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OnlyStreamInTextures"));
	if( CVarOnlyStreamInTextures->GetValueOnAnyThread() != 0 )
	{
		float SecondsSinceLastRender = StreamingTexture.LastRenderTime;
		if( SecondsSinceLastRender < 300 )
		{
			StreamingTexture.bForceFullyLoad = true;
		}
	}

	// Calculate the min/max allowed mips.
	UTexture2D::CalcAllowedMips(
		StreamingTexture.MipCount,
		StreamingTexture.NumNonStreamingMips,
		TextureLODBias,
		StreamingTexture.MinAllowedMips,
		StreamingTexture.MaxAllowedMips );

	// Take the reduced texture pool in to account.
	STAT( StreamingTexture.MaxAllowedOptimalMips = StreamingTexture.MaxAllowedMips );
	if ( GIsOperatingWithReducedTexturePool )
	{				
		int32 MaxTextureMipCount = FMath::Max( GMaxTextureMipCount - 2, 0 );
		StreamingTexture.MaxAllowedMips = FMath::Min( StreamingTexture.MaxAllowedMips, MaxTextureMipCount );
	}

	// Should this texture be fully streamed in?
	if ( StreamingTexture.bForceFullyLoad )
	{
		StreamingTexture.MinAllowedMips = StreamingTexture.MaxAllowedMips;
	}
	// Check if the texture LOD group restricts the number of streaming mip-levels (in absolute terms).
	else if ( ThreadSettings.NumStreamedMips[StreamingTexture.LODGroup] >= 0 )
	{
		StreamingTexture.MinAllowedMips = FMath::Clamp( StreamingTexture.MipCount - ThreadSettings.NumStreamedMips[StreamingTexture.LODGroup], StreamingTexture.MinAllowedMips, StreamingTexture.MaxAllowedMips );
	}

	check( StreamingTexture.MinAllowedMips > 0 && StreamingTexture.MinAllowedMips <= StreamingTexture.MipCount );
	check( StreamingTexture.MaxAllowedMips >= StreamingTexture.MinAllowedMips && StreamingTexture.MaxAllowedMips <= StreamingTexture.MipCount );
}

/**
 * Updates this frame's STATs by one texture.
 */
void FStreamingManagerTexture::UpdateFrameStats( FStreamingContext& Context, FStreamingTexture& StreamingTexture, int32 TextureIndex )
{
#if STATS_FAST || STATS
	STAT(int64 ResidentSize = StreamingTexture.GetSize(StreamingTexture.ResidentMips));
	STAT(Context.ThisFrameTotalStreamingTexturesSize += ResidentSize);
	STAT(int32 PerfectWantedMips = FMath::Clamp(StreamingTexture.PerfectWantedMips, StreamingTexture.MinAllowedMips, StreamingTexture.MaxAllowedOptimalMips) );
	STAT(int64 PerfectWantedSize = StreamingTexture.GetSize( PerfectWantedMips ));
	STAT(int64 MostResidentSize = StreamingTexture.GetSize( StreamingTexture.MostResidentMips ) );
	STAT(Context.ThisFrameOptimalWantedSize += PerfectWantedSize );
#endif

#if STATS
	int64 PotentialSize = StreamingTexture.GetSize(StreamingTexture.MaxAllowedMips);
	ETextureStreamingType StreamType = StreamingTexture.GetStreamingType();
	switch ( StreamType )
	{
		case StreamType_Forced:
			Context.ThisFrameTotalForcedHeuristicSize += PerfectWantedSize;
			break;
		case StreamType_LastRenderTime:
			Context.ThisFrameTotalLastRenderHeuristicSize += PerfectWantedSize;
			break;
		case StreamType_Dynamic:
			Context.ThisFrameTotalDynamicTextureHeuristicSize += PerfectWantedSize;
			break;
		case StreamType_Static:
			Context.ThisFrameTotalStaticTextureHeuristicSize += PerfectWantedSize;
			break;
		case StreamType_Orphaned:
		case StreamType_Other:
			break;
	}
	if ( Context.bCollectTextureStats )
	{
		FString TextureName = StreamingTexture.Texture->GetFullName();
		if ( CollectTextureStatsName.Len() == 0 || TextureName.Contains( CollectTextureStatsName) )
		{
			new (Context.TextureStats) FTextureStreamingStats( StreamingTexture.Texture, StreamType, StreamingTexture.ResidentMips, PerfectWantedMips, StreamingTexture.MostResidentMips, ResidentSize, PerfectWantedSize, PotentialSize, MostResidentSize, StreamingTexture.BoostFactor, StreamingTexture.CalcPriority(), TextureIndex );
		}
	}
	Context.ThisFrameNumStreamingTextures++;
	Context.ThisFrameTotalStreamingTexturesMaxSize += PotentialSize;
	Context.ThisFrameTotalLightmapMemorySize += StreamingTexture.bIsStreamingLightmap ? ResidentSize : 0;
	Context.ThisFrameTotalLightmapDiskSize += StreamingTexture.bIsStreamingLightmap ? PotentialSize : 0;
#endif
}

/**
 * Calculates the number of mip-levels we would like to have in memory for a texture.
 */
void FStreamingManagerTexture::CalcWantedMips( FStreamingTexture& StreamingTexture )
{
	FFloatMipLevel WantedMips;
	
	if(StreamingTexture.bUsesDynamicHeuristics)
	{
		WantedMips = FFloatMipLevel::FromScreenSizeInTexels(StreamingTexture.DynamicScreenSize);
	}

	float MinDistance = StreamingTexture.bUsesDynamicHeuristics ? FMath::Sqrt(StreamingTexture.DynamicMinDistanceSq) : FLT_MAX;
	STAT(FFloatMipLevel PerfectWantedMips = WantedMips);

	// Figure out miplevels to request based on handlers.
	if ( StreamingTexture.MinAllowedMips != StreamingTexture.MaxAllowedMips )
	{
		// Iterate over all handlers and figure out the maximum requested number of mips.
		for( int32 HandlerIndex=0; HandlerIndex<TextureStreamingHandlers.Num(); HandlerIndex++ )
		{
			FStreamingHandlerTextureBase* TextureStreamingHandler = TextureStreamingHandlers[HandlerIndex];
			float HandlerDistance = FLT_MAX;
			FFloatMipLevel HandlerWantedMips = TextureStreamingHandler->GetWantedMips( *this, StreamingTexture, HandlerDistance );
			WantedMips = FMath::Max( WantedMips, HandlerWantedMips );
			STAT( PerfectWantedMips = FMath::Max( PerfectWantedMips, HandlerWantedMips ) );
			MinDistance = FMath::Min( MinDistance, HandlerDistance );
		}

		bool bShouldAlsoUseLastRenderTime = false;
		if ( StreamingTexture.LastRenderTimeRefCount > 0 || (FApp::GetCurrentTime() - StreamingTexture.LastRenderTimeRefCountTimestamp) < 91.0 )
		{
			bShouldAlsoUseLastRenderTime = true;

#if UE_BUILD_DEBUG
			//@DEBUG: To be able to set a break-point here.
			if ( WantedMips.ComputeMip(&StreamingTexture, ThreadSettings.MipBias, false) != INDEX_NONE )
			{
				int32 Q=0;
			}
#endif
		}

		// Not handled by any handler, use fallback handlers.
		if ( !WantedMips.IsHandled() || bShouldAlsoUseLastRenderTime )
		{
			// Try the handler for textures that has recently lost instance locations.
			{
				float HandlerDistance = FLT_MAX;
				FFloatMipLevel HandlerWantedMips = GetWantedMipsForOrphanedTexture(StreamingTexture, HandlerDistance);
				WantedMips = FMath::Max(WantedMips, HandlerWantedMips);
				MinDistance = FMath::Min(MinDistance, HandlerDistance);
				STAT(PerfectWantedMips = FMath::Max(PerfectWantedMips, HandlerWantedMips));
			}

			// Still wasn't handled? Use the LastRenderTime handler as last resort.
			if (!WantedMips.IsHandled() || bShouldAlsoUseLastRenderTime )
			{
				// Fallback handler used if texture is not handled by any other handlers. Guaranteed to handle texture and not return INDEX_NONE.
				FStreamingHandlerTextureLastRender FallbackStreamingHandler;
				float HandlerDistance = FLT_MAX;
				FFloatMipLevel HandlerWantedMips = FallbackStreamingHandler.GetWantedMips( *this, StreamingTexture, HandlerDistance );
				WantedMips = FMath::Max( WantedMips, HandlerWantedMips );
				MinDistance = FMath::Min( MinDistance, HandlerDistance );
				STAT( PerfectWantedMips = FMath::Max( PerfectWantedMips, HandlerWantedMips ) );
			}
		}
	}
	// Stream in all allowed miplevels if requested.
	else
	{
		STAT( PerfectWantedMips = FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedOptimalMips) );
		WantedMips = FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedMips);
	}

	StreamingTexture.WantedMips = WantedMips.ComputeMip(&StreamingTexture, ThreadSettings.MipBias, false);
	StreamingTexture.MinDistance = MinDistance;
	STAT( StreamingTexture.PerfectWantedMips = PerfectWantedMips.ComputeMip(&StreamingTexture, ThreadSettings.MipBias, true) );
}

/**
 * Fallback handler to catch textures that have been orphaned recently.
 * This handler prevents massive spike in texture memory usage.
 * Orphaned textures were previously streamed based on distance but those instance locations have been removed -
 * for instance because a ULevel is being unloaded. These textures are still active and in memory until they are garbage collected,
 * so we must ensure that they do not start using the LastRenderTime fallback handler and suddenly stream in all their mips -
 * just to be destroyed shortly after by a garbage collect.
 */
FFloatMipLevel FStreamingManagerTexture::GetWantedMipsForOrphanedTexture( FStreamingTexture& StreamingTexture, float& Distance )
{
	FFloatMipLevel WantedMips;

	// Did we recently remove instance locations for this texture?
	const float TimeSinceInstanceWasRemoved = float(FApp::GetCurrentTime() - StreamingTexture.InstanceRemovedTimestamp);

	// Was it less than 91 seconds ago?
	if ( TimeSinceInstanceWasRemoved < 91.0f )
	{
		const float TimeSinceTextureWasRendered = StreamingTexture.LastRenderTime;

		// Check if it hasn't been rendered since the instances were removed (with five second margin).
		if ( (TimeSinceTextureWasRendered - TimeSinceInstanceWasRemoved) > -5.0f )
		{
			// It may be garbage-collected soon. Stream out the highest mip, if currently loaded.
			WantedMips = FFloatMipLevel::FromMipLevel(FMath::Min( StreamingTexture.ResidentMips, StreamingTexture.MaxAllowedMips - 1 ));
			Distance = 1000.0f;
			StreamingTexture.bUsesOrphanedHeuristics = true;
		}
	}

	return WantedMips;
}

/**
 * Updates the I/O state of a texture (allowing it to progress to the next stage) and some stats.
 */
void FStreamingManagerTexture::UpdateTextureStatus( FStreamingTexture& StreamingTexture, FStreamingContext& Context )
{
	UTexture2D* Texture = StreamingTexture.Texture;

	// Update streaming status. A return value of false means that we're done streaming this texture
	// so we can potentially request another change.
	StreamingTexture.bInFlight		= Texture->UpdateStreamingStatus( true );
	StreamingTexture.ResidentMips	= Texture->ResidentMips;
	StreamingTexture.RequestedMips	= Texture->RequestedMips;
	int32		RequestStatus			= Texture->PendingMipChangeRequestStatus.GetValue();
	bool	bHasCancelationPending	= Texture->bHasCancelationPending;

	if( bHasCancelationPending )
	{
		Context.ThisFrameNumRequestsInCancelationPhase++;
	}
	else if( RequestStatus >= TexState_ReadyFor_Finalization )
	{
		Context.ThisFrameNumRequestsInUpdatePhase++;
	}
	else if( RequestStatus == TexState_InProgress_Finalization )
	{
		Context.ThisFrameNumRequestsInFinalizePhase++;
	}

	// Request is in flight so there is an intermediate texture with RequestedMips miplevels.
	if( RequestStatus > 0 )
	{
		Context.ThisFrameTotalIntermediateTexturesSize += StreamingTexture.GetSize(StreamingTexture.RequestedMips);
		Context.ThisFrameNumIntermediateTextures++;
		// Update texture increase request stats.
		if( StreamingTexture.RequestedMips > StreamingTexture.ResidentMips )
		{
			Context.ThisFrameTotalMipCountIncreaseRequestsInFlight++;
		}
	}

	STAT( UpdateLatencyStats( Texture, StreamingTexture.WantedMips, StreamingTexture.bInFlight ) );

	if ( StreamingTexture.bInFlight == false )
	{
		check( RequestStatus == TexState_ReadyFor_Requests );
	}
}

/**
 * Starts streaming in/out a texture.
 *
 * @param StreamingTexture			Texture to start to stream in/out
 * @param RequestMips				Number of mips we want in memory for this texture.
 * @param Context					Context for the current frame
 * @param bIgnoreMemoryConstraints	Whether to ignore memory constraints and always start streaming
 * @return							true if the texture is now in flight
 */
bool FStreamingManagerTexture::StartStreaming( FStreamingTexture& StreamingTexture, int32 RequestMips, FStreamingContext& Context, bool bIgnoreMemoryConstraints )
{
	UTexture2D* Texture = StreamingTexture.Texture;
	FTexture2DResource* Texture2DResource = (FTexture2DResource*) Texture->Resource;

//	StreamingTexture.WantedMips = RequestMips;

	if ( RequestMips != StreamingTexture.ResidentMips && StreamingTexture.bReadyForStreaming )
	{
		bool bCanRequestTextureIncrease = true;

		bool bIsLoadingRequest = RequestMips > StreamingTexture.ResidentMips;

		bCanRequestTextureIncrease = bIgnoreMemoryConstraints;

		// Only request change if it's a decrease or we can request an increase.
		if ( !bIsLoadingRequest || bCanRequestTextureIncrease )						
		{
			if ( Texture->PendingMipChangeRequestStatus.GetValue() == TexState_ReadyFor_Requests &&
				 RequestMips != StreamingTexture.Texture->ResidentMips )
			{
				// Keep track of current allocations this frame.
				if ( bIsLoadingRequest )
				{
					// Manually update size as allocations are deferred/ happening in rendering thread.
					int64 CurrentRequestSize			= StreamingTexture.GetSize(RequestMips);
					int64 StreamInSize					= CurrentRequestSize - StreamingTexture.GetSize(StreamingTexture.ResidentMips);
					Context.ThisFrameTotalRequestSize	+= StreamInSize;
					Context.ThisFrameTotalLightmapRequestSize += StreamingTexture.bIsStreamingLightmap ? StreamInSize : 0;
					Context.AvailableMemorySize			-= CurrentRequestSize;
				}

				check(!Texture->bHasCancelationPending);
				// Set new requested mip count.
				Texture->RequestedMips = RequestMips;
				StreamingTexture.RequestedMips = RequestMips;
				// Enqueue command to update mip count.
				bool bShouldPrioritizeAsyncIORequest = StreamingTexture.bForceFullyLoad;
				Texture2DResource->BeginUpdateMipCount( bShouldPrioritizeAsyncIORequest );
				StreamingTexture.bInFlight = true;
				TrackTextureEvent( &StreamingTexture, StreamingTexture.Texture, StreamingTexture.bForceFullyLoad, this );
			}
			else
			{
				// Did UpdateStreamingTextures() miss a texture? Should never happen!
				UE_LOG(LogContentStreaming, Warning, TEXT("BeginUpdateMipCount failure! PendingMipChangeRequestStatus == %d, Resident=%d, Requested=%d, Wanted=%d"), Texture->PendingMipChangeRequestStatus.GetValue(), Texture->ResidentMips, Texture->RequestedMips, StreamingTexture.WantedMips );
			}

			// Mark as unavailable for further streaming actions this frame.
			StreamingTexture.bReadyForStreaming = false;
		}
	}

	return StreamingTexture.bInFlight;
}

/**
 * Cancels the current streaming request for the specified texture.
 *
 * @param StreamingTexture		Texture to cancel streaming for
 * @return						true if a streaming request was canceled
 */
bool FStreamingManagerTexture::CancelStreamingRequest( FStreamingTexture& StreamingTexture )
{
	// Mark as unavailable for further streaming action this frame.
	StreamingTexture.bReadyForStreaming = false;

	StreamingTexture.RequestedMips = StreamingTexture.ResidentMips;
	return StreamingTexture.Texture->CancelPendingMipChangeRequest();
}

#if STATS_FAST
bool FStreamingManagerTexture::HandleDumpTextureStreamingStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	Ar.Logf( TEXT("Current Texture Streaming Stats") );
	Ar.Logf( TEXT("  Textures In Memory, Current (KB) = %f"), MaxStreamingTexturesSize / 1024.0f);
	Ar.Logf( TEXT("  Textures In Memory, Target (KB) =  %f"), MaxOptimalTextureSize / 1024.0f );
	Ar.Logf( TEXT("  Over Budget (KB) =                 %f"), MaxStreamingOverBudget / 1024.0f );
	Ar.Logf( TEXT("  Pool Memory Used (KB) =            %f"), MaxTexturePoolAllocatedSize / 1024.0f );
	Ar.Logf( TEXT("  Largest free memory hole (KB) =    %f"), MinLargestHoleSize / 1024.0f );
	Ar.Logf( TEXT("  Num Wanting Textures =             %d"), MaxNumWantingTextures );
	MaxStreamingTexturesSize = 0;
	MaxOptimalTextureSize = 0;
	MaxStreamingOverBudget = MIN_int64;
	MaxTexturePoolAllocatedSize = 0;
	MinLargestHoleSize = OriginalTexturePoolSize;
	MaxNumWantingTextures = 0;
	return true;
}
#endif // STATS_FAST

#if STATS
/**
 * Updates the streaming latency STAT for a texture.
 *
 * @param Texture		Texture to update for
 * @param WantedMips	Number of desired mip-levels for the texture
 * @param bInFlight		Whether the texture is currently being streamed
 */
void FStreamingManagerTexture::UpdateLatencyStats( UTexture2D* Texture, int32 WantedMips, bool bInFlight )
{
	// Is the texture currently not updating?
	if ( bInFlight == false )
	{
		// Did we measure the latency time?
		if ( Texture->Timer > 0.0f )
		{
			double TotalLatency = LatencyAverage*NumLatencySamples - LatencySamples[LatencySampleIndex] + Texture->Timer;
			LatencySamples[LatencySampleIndex] = Texture->Timer;
			LatencySampleIndex = (LatencySampleIndex + 1) % NUM_LATENCYSAMPLES;
			NumLatencySamples = ( NumLatencySamples == NUM_LATENCYSAMPLES ) ? NumLatencySamples : (NumLatencySamples+1);
			LatencyAverage = TotalLatency / NumLatencySamples;
			LatencyMaximum = FMath::Max(LatencyMaximum, Texture->Timer);
		}

		// Have we detected that the texture should stream in more mips?
		if ( WantedMips > Texture->ResidentMips )
		{
			// Set the start time. Make it negative so we can differentiate it with a measured time.
			Texture->Timer = -float(FPlatformTime::Seconds() - GStartTime );
		}
		else
		{
			Texture->Timer = 0.0f;
		}
	}
}

bool FStreamingManagerTexture::HandleListStreamingTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Collect and report stats
	CollectTextureStatsName = FParse::Token(Cmd, 0);
	bCollectTextureStats = true;
	bReportTextureStats = true;
	return true;
}

bool FStreamingManagerTexture::HandleListStreamingTexturesCollectCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Collect stats, but disable automatic reporting
	CollectTextureStatsName = FParse::Token(Cmd, 0);
	bCollectTextureStats = true;
	bReportTextureStats = false;
	return true;
}

bool FStreamingManagerTexture::HandleListStreamingTexturesReportReadyCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( bCollectTextureStats )
	{
		if( TextureStatsReport.Num() > 0 )
		{
			return true;
		}
		return false;
	}
	return true;
}
bool FStreamingManagerTexture::HandleListStreamingTexturesReportCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// TextureStatsReport is assumed to have been populated already via ListStreamingTexturesCollect
	if( TextureStatsReport.Num() > 0 )
	{
		Ar.Logf( TEXT("Listing collected stats for all streaming textures") );
		for( int32 ReportIndex = 0; ReportIndex < TextureStatsReport.Num(); ReportIndex++ )
		{
			Ar.Logf(*(TextureStatsReport[ReportIndex]));
		}
		TextureStatsReport.Empty();
	}
	else
	{
		Ar.Logf( TEXT("No stats have been collected for streaming textures. Use ListStreamingTexturesCollect to do so.") );
	}
	return true;
}

#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
bool FStreamingManagerTexture::HandleLightmapStreamingFactorCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString FactorString(FParse::Token(Cmd, 0));
	float NewFactor = ( FactorString.Len() > 0 ) ? FCString::Atof(*FactorString) : GLightmapStreamingFactor;
	if ( NewFactor >= 0.0f )
	{
		GLightmapStreamingFactor = NewFactor;
	}
	Ar.Logf( TEXT("Lightmap streaming factor: %.3f (lower values makes streaming more aggressive)."), GLightmapStreamingFactor );
	return true;
}

bool FStreamingManagerTexture::HandleCancelTextureStreamingCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	UTexture2D::CancelPendingTextureStreaming();
	return true;
}

bool FStreamingManagerTexture::HandleShadowmapStreamingFactorCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString FactorString(FParse::Token(Cmd, 0));
	float NewFactor = ( FactorString.Len() > 0 ) ? FCString::Atof(*FactorString) : GShadowmapStreamingFactor;
	if ( NewFactor >= 0.0f )
	{
		GShadowmapStreamingFactor = NewFactor;
	}
	Ar.Logf( TEXT("Shadowmap streaming factor: %.3f (lower values makes streaming more aggressive)."), GShadowmapStreamingFactor );
	return true;
}

bool FStreamingManagerTexture::HandleNumStreamedMipsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString NumTextureString(FParse::Token(Cmd, 0));
	FString NumMipsString(FParse::Token(Cmd, 0));
	int32 LODGroup = ( NumTextureString.Len() > 0 ) ? FCString::Atoi(*NumTextureString) : MAX_int32;
	int32 NumMips = ( NumMipsString.Len() > 0 ) ? FCString::Atoi(*NumMipsString) : MAX_int32;
	if ( LODGroup >= 0 && LODGroup < TEXTUREGROUP_MAX )
	{
		FTextureLODGroup& TexGroup = UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetTextureLODGroup(TextureGroup(LODGroup));
		if ( NumMips >= -1 && NumMips <= MAX_TEXTURE_MIP_COUNT )
		{
			TexGroup.NumStreamedMips = NumMips;
		}
		Ar.Logf( TEXT("%s.NumStreamedMips = %d"), UTexture::GetTextureGroupString(TextureGroup(LODGroup)), TexGroup.NumStreamedMips );
	}
	else
	{
		Ar.Logf( TEXT("Usage: NumStreamedMips TextureGroupIndex <N>") );
	}
	return true;
}

bool FStreamingManagerTexture::HandleTrackTextureCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString TextureName(FParse::Token(Cmd, 0));
	if ( TrackTexture( TextureName ) )
	{
		Ar.Logf( TEXT("Textures containing \"%s\" are now tracked."), *TextureName );
	}
	return true;
}

bool FStreamingManagerTexture::HandleListTrackedTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString NumTextureString(FParse::Token(Cmd, 0));
	int32 NumTextures = ( NumTextureString.Len() > 0 ) ? FCString::Atoi(*NumTextureString) : -1;
	ListTrackedTextures( Ar, NumTextures );
	return true;
}
bool FStreamingManagerTexture::HandleDebugTrackedTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	int32 NumTrackedTextures = GTrackedTextureNames.Num();
	if ( NumTrackedTextures )
	{
		for (int32 StreamingIndex = 0; StreamingIndex < StreamingTextures.Num(); ++StreamingIndex)
		{
			FStreamingTexture& StreamingTexture = StreamingTextures[StreamingIndex];
			if (StreamingTexture.Texture)
			{
				// See if it matches any of the texture names that we're tracking.
				FString TextureNameString = StreamingTexture.Texture->GetFullName();
				const TCHAR* TextureName = *TextureNameString;
				for ( int32 TrackedTextureIndex=0; TrackedTextureIndex < NumTrackedTextures; ++TrackedTextureIndex )
				{
					const FString& TrackedTextureName = GTrackedTextureNames[TrackedTextureIndex];
					if ( FCString::Stristr(TextureName, *TrackedTextureName) != NULL )
					{
						FTrackedTextureEvent* LastEvent = NULL;
						for ( int32 LastEventIndex=0; LastEventIndex < GTrackedTextures.Num(); ++LastEventIndex )
						{
							FTrackedTextureEvent* Event = &GTrackedTextures[LastEventIndex];
							if ( FCString::Strcmp(TextureName, Event->TextureName) == 0 )
							{
								LastEvent = Event;
								break;
							}
						}

						if (LastEvent)
						{
							Ar.Logf(
								TEXT("Texture: \"%s\", ResidentMips: %d/%d, RequestedMips: %d, WantedMips: %d, DynamicWantedMips: %d, StreamingStatus: %d, StreamType: %s, Boost: %.1f"),
								TextureName,
								LastEvent->NumResidentMips,
								StreamingTexture.Texture->GetNumMips(),
								LastEvent->NumRequestedMips,
								LastEvent->WantedMips,
								LastEvent->DynamicWantedMips.ComputeMip(&StreamingTexture, ThreadSettings.MipBias, false),
								LastEvent->StreamingStatus,
								GStreamTypeNames[StreamingTexture.GetStreamingType()],
								StreamingTexture.BoostFactor
								);
						}
						else
						{
							Ar.Logf(TEXT("Texture: \"%s\", StreamType: %s, Boost: %.1f"),
								TextureName,
								GStreamTypeNames[StreamingTexture.GetStreamingType()],
								StreamingTexture.BoostFactor
								);
						}
						for( int32 HandlerIndex=0; HandlerIndex<TextureStreamingHandlers.Num(); HandlerIndex++ )
						{
							FStreamingHandlerTextureBase* TextureStreamingHandler = TextureStreamingHandlers[HandlerIndex];
							float HandlerDistance = FLT_MAX;
							FFloatMipLevel HandlerWantedMips = TextureStreamingHandler->GetWantedMips(*this, StreamingTexture, HandlerDistance);
							Ar.Logf(
								TEXT("    Handler %s: WantedMips: %d, PerfectWantedMips: %d, Distance: %f"),
								TextureStreamingHandler->HandlerName,
								HandlerWantedMips.ComputeMip(&StreamingTexture, ThreadSettings.MipBias, false),
								HandlerWantedMips.ComputeMip(&StreamingTexture, ThreadSettings.MipBias, true),
								HandlerDistance
								);
						}

						for ( int32 LevelIndex=0; LevelIndex < ThreadSettings.LevelData.Num(); ++LevelIndex )
						{
							FStreamingManagerTexture::FThreadLevelData& LevelData = ThreadSettings.LevelData[ LevelIndex ].Value;
							TArray<FStreamableTextureInstance4>* TextureInstances = LevelData.ThreadTextureInstances.Find( StreamingTexture.Texture );
							if ( TextureInstances )
							{
								for ( int32 InstanceIndex=0; InstanceIndex < TextureInstances->Num(); ++InstanceIndex )
								{
									const FStreamableTextureInstance4& TextureInstance = (*TextureInstances)[InstanceIndex];
									for (int32 i = 0; i < 4; i++)
									{
										Ar.Logf(
											TEXT("    Instance: %f,%f,%f Radius: %f TexelFactor: %f"),
											TextureInstance.BoundingSphereX[i],
											TextureInstance.BoundingSphereY[i],
											TextureInstance.BoundingSphereZ[i],
											TextureInstance.BoundingSphereRadius[i],
											TextureInstance.TexelFactor[i]
										);
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return true;
}

bool FStreamingManagerTexture::HandleUntrackTextureCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString TextureName(FParse::Token(Cmd, 0));
	if ( UntrackTexture( TextureName ) )
	{
		Ar.Logf( TEXT("Textures containing \"%s\" are no longer tracked."), *TextureName );
	}
	return true;
}

bool FStreamingManagerTexture::HandleStreamOutCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString Parameter(FParse::Token(Cmd, 0));
	int64 FreeMB = (Parameter.Len() > 0) ? FCString::Atoi(*Parameter) : 0;
	if ( FreeMB > 0 )
	{
		bool bSucceeded = StreamOutTextureData( FreeMB * 1024 * 1024 );
		Ar.Logf( TEXT("Tried to stream out %d MB of texture data: %s"), FreeMB, bSucceeded ? TEXT("Succeeded") : TEXT("Failed") );
	}
	else
	{
		Ar.Logf( TEXT("Usage: StreamOut <N> (in MB)") );
	}
	return true;
}

bool FStreamingManagerTexture::HandlePauseTextureStreamingCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	bPauseTextureStreaming = !bPauseTextureStreaming;
	Ar.Logf( TEXT("Texture streaming is now \"%s\"."), bPauseTextureStreaming ? TEXT("PAUSED") : TEXT("UNPAUSED") );
	return true;
}

bool FStreamingManagerTexture::HandleStreamingManagerMemoryCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	int32 OldStreamingTextures = 0;
	int32 NewStreamingTextures = StreamingTextures.Num() * sizeof( FStreamingTexture );
	for ( TObjectIterator<UTexture2D> It; It; ++It )
	{
		OldStreamingTextures += sizeof(TLinkedList<UTexture2D*>);
		NewStreamingTextures += sizeof(int32);
	}

	int32 OldSystem = 0;
	int32 OldSystemSlack = 0;
	for( int32 LevelIndex=0; LevelIndex<InWorld->GetNumLevels(); LevelIndex++ )
	{
		// Find instances of the texture in this level.
		ULevel* Level = InWorld->GetLevel(LevelIndex);
		for ( TMap<UTexture2D*,TArray<FStreamableTextureInstance> >::TConstIterator It(Level->TextureToInstancesMap); It; ++It )
		{
			const TArray<FStreamableTextureInstance>& TextureInstances = It.Value();
			OldSystem += TextureInstances.Num() * sizeof(FStreamableTextureInstance);
			OldSystemSlack += TextureInstances.GetSlack() * sizeof(FStreamableTextureInstance);
		}
	}

	int32 NewSystem = 0;
	int32 NewSystemSlack = 0;
	for ( int32 LevelIndex=0; LevelIndex < ThreadSettings.LevelData.Num(); ++LevelIndex )
	{
		const FLevelData& LevelData = ThreadSettings.LevelData[ LevelIndex ];
		for ( TMap<const UTexture2D*,TArray<FStreamableTextureInstance4> >::TConstIterator It(LevelData.Value.ThreadTextureInstances); It; ++It )
		{
			const TArray<FStreamableTextureInstance4>& TextureInstances = It.Value();
			NewSystem += TextureInstances.Num() * sizeof(FStreamableTextureInstance4);
			NewSystemSlack += TextureInstances.GetSlack() * sizeof(FStreamableTextureInstance4);
		}
	}
	Ar.Logf( TEXT("Old: %.2f KB used, %.2f KB slack, %.2f KB texture info"), OldSystem/1024.0f, OldSystemSlack/1024.0f, OldStreamingTextures/1024.0f );
	Ar.Logf( TEXT("New: %.2f KB used, %.2f KB slack, %.2f KB texture info"), NewSystem/1024.0f, NewSystemSlack/1024.0f, NewStreamingTextures/1024.0f );

	return true;
}

bool FStreamingManagerTexture::HandleTextureGroupsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	bDetailedDumpTextureGroupStats = FParse::Param(Cmd, TEXT("Detailed"));
	bTriggerDumpTextureGroupStats = true;
	return true;
}

bool FStreamingManagerTexture::HandleInvestigateTextureCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString TextureName(FParse::Token(Cmd, 0));
	if ( TextureName.Len() )
	{
		bTriggerInvestigateTexture = true;
		InvestigateTextureName = TextureName;
	}
	else
	{
		Ar.Logf( TEXT("Usage: InvestigateTexture <name>") );
	}
	return true;
}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
/**
 * Allows the streaming manager to process exec commands.
 * @param InWorld	world contexxt
 * @param Cmd		Exec command
 * @param Ar		Output device for feedback
 * @return			true if the command was handled
 */
bool FStreamingManagerTexture::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
#if STATS_FAST
	if (FParse::Command(&Cmd,TEXT("DumpTextureStreamingStats")))
	{
		return HandleDumpTextureStreamingStatsCommand( Cmd, Ar );
	}
#endif
#if STATS
	if (FParse::Command(&Cmd,TEXT("ListStreamingTextures")))
	{
		return HandleListStreamingTexturesCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("ListStreamingTexturesCollect")))
	{
		return HandleListStreamingTexturesCollectCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("ListStreamingTexturesReportReady")))
	{
		return HandleListStreamingTexturesReportReadyCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("ListStreamingTexturesReport")))
	{
		return HandleListStreamingTexturesReportCommand( Cmd, Ar );
	}
#endif
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (FParse::Command(&Cmd,TEXT("LightmapStreamingFactor")))
	{
		return HandleLightmapStreamingFactorCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("CancelTextureStreaming")))
	{
		return HandleCancelTextureStreamingCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("ShadowmapStreamingFactor")))
	{
		return HandleShadowmapStreamingFactorCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("NumStreamedMips")))
	{
		return HandleNumStreamedMipsCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("TrackTexture")))
	{
		return HandleTrackTextureCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("ListTrackedTextures")))
	{
		return HandleListTrackedTexturesCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("DebugTrackedTextures")))
	{
		return HandleDebugTrackedTexturesCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("UntrackTexture")))
	{
		return HandleUntrackTextureCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("StreamOut")))
	{
		return HandleStreamOutCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("PauseTextureStreaming")))
	{
		return HandlePauseTextureStreamingCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("StreamingManagerMemory")))
	{
		return HandleStreamingManagerMemoryCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd,TEXT("TextureGroups")))
	{
		return HandleTextureGroupsCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("InvestigateTexture")))
	{
		return HandleInvestigateTextureCommand( Cmd, Ar );
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	return false;
}

void FStreamingManagerTexture::DumpTextureGroupStats( bool bDetailedStats )
{
	bTriggerDumpTextureGroupStats = false;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	struct FTextureGroupStats
	{
		FTextureGroupStats()
		{
			FMemory::Memzero( this, sizeof(FTextureGroupStats) );
		}
		int32 NumTextures;
		int32 NumNonStreamingTextures;
		int64 CurrentTextureSize;
		int64 WantedTextureSize;
		int64 MaxTextureSize;
		int64 NonStreamingSize;
	};
	FTextureGroupStats TextureGroupStats[TEXTUREGROUP_MAX];
	FTextureGroupStats TextureGroupWaste[TEXTUREGROUP_MAX];
	int64 NumNonStreamingTextures = 0;
	int64 NonStreamingSize = 0;
	int32 NumNonStreamingPoolTextures = 0;
	int64 NonStreamingPoolSize = 0;
	int64 TotalSavings = 0;
//	int32 UITexels = 0;
	int32 NumDXT[PF_MAX];
	int32 NumNonSaved[PF_MAX];
	int32 NumOneMip[PF_MAX];
	int32 NumBadAspect[PF_MAX];
	int32 NumTooSmall[PF_MAX];
	int32 NumNonPow2[PF_MAX];
	int32 NumNULLResource[PF_MAX];
	FMemory::Memzero( &NumDXT, sizeof(NumDXT) );
	FMemory::Memzero( &NumNonSaved, sizeof(NumNonSaved) );
	FMemory::Memzero( &NumOneMip, sizeof(NumOneMip) );
	FMemory::Memzero( &NumBadAspect, sizeof(NumBadAspect) );
	FMemory::Memzero( &NumTooSmall, sizeof(NumTooSmall) );
	FMemory::Memzero( &NumNonPow2, sizeof(NumNonPow2) );
	FMemory::Memzero( &NumNULLResource, sizeof(NumNULLResource) );

	// Gather stats.
	for( TObjectIterator<UTexture> It; It; ++It )
	{
		UTexture* Texture = *It;
		FTextureGroupStats& Stat = TextureGroupStats[Texture->LODGroup];
		FTextureGroupStats& Waste = TextureGroupWaste[Texture->LODGroup];
		UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
		uint32 TextureAlign = 0;
		if ( Texture2D && Texture2D->StreamingIndex >= 0 )
		{
			FStreamingTexture& StreamingTexture = GetStreamingTexture( Texture2D );
			Stat.NumTextures++;
			Stat.CurrentTextureSize += StreamingTexture.GetSize( StreamingTexture.ResidentMips );
			Stat.WantedTextureSize += StreamingTexture.GetSize( StreamingTexture.WantedMips );
			Stat.MaxTextureSize += StreamingTexture.GetSize( StreamingTexture.MaxAllowedMips );
			
			int64 WasteCurrent = StreamingTexture.GetSize( StreamingTexture.ResidentMips ) - RHICalcTexture2DPlatformSize(Texture2D->GetSizeX(), Texture2D->GetSizeY(), Texture2D->GetPixelFormat(), StreamingTexture.ResidentMips, 1, 0, TextureAlign);			

			int64 WasteWanted = StreamingTexture.GetSize( StreamingTexture.WantedMips ) - RHICalcTexture2DPlatformSize(Texture2D->GetSizeX(), Texture2D->GetSizeY(), Texture2D->GetPixelFormat(), StreamingTexture.WantedMips, 1, 0, TextureAlign);			

			int64 WasteMaxSize = StreamingTexture.GetSize( StreamingTexture.MaxAllowedMips ) - RHICalcTexture2DPlatformSize(Texture2D->GetSizeX(), Texture2D->GetSizeY(), Texture2D->GetPixelFormat(), StreamingTexture.MaxAllowedMips, 1, 0, TextureAlign);			

			Waste.NumTextures++;
			Waste.CurrentTextureSize += FMath::Max<int64>(WasteCurrent,0);
			Waste.WantedTextureSize += FMath::Max<int64>(WasteWanted,0);
			Waste.MaxTextureSize += FMath::Max<int64>(WasteMaxSize,0);
		}
		else
		{
			bool bIsPooledTexture = Texture->Resource && IsValidRef(Texture->Resource->TextureRHI) && appIsPoolTexture( Texture->Resource->TextureRHI );
			int64 TextureSize = Texture->CalcTextureMemorySizeEnum(TMC_ResidentMips);
			Stat.NumNonStreamingTextures++;
			Stat.NonStreamingSize += TextureSize;
			if ( Texture2D && Texture2D->Resource )
			{				
				int64 WastedSize = TextureSize - RHICalcTexture2DPlatformSize(Texture2D->GetSizeX(), Texture2D->GetSizeY(), Texture2D->GetPixelFormat(), Texture2D->GetNumMips(), 1, 0, TextureAlign);				

				Waste.NumNonStreamingTextures++;
				Waste.NonStreamingSize += FMath::Max<int64>(WastedSize, 0);
			}
			if ( bIsPooledTexture )
			{
				NumNonStreamingPoolTextures++;
				NonStreamingPoolSize += TextureSize;
			}
			else
			{
				NumNonStreamingTextures++;
				NonStreamingSize += TextureSize;
			}
		}

// 		if ( Texture2D && Texture2D->Resource && Texture2D->LODGroup == TEXTUREGROUP_UI )
// 		{
// 			UITexels += Texture2D->GetSizeX() * Texture2D->GetSizeY();
// 		}
// 
		if ( Texture2D && (Texture2D->GetPixelFormat() == PF_DXT1 || Texture2D->GetPixelFormat() == PF_DXT5) )
		{
			NumDXT[Texture2D->GetPixelFormat()]++;
			if ( Texture2D->Resource )
			{
				// Track the reasons we couldn't save any memory from the mip-tail.
				NumNonSaved[Texture2D->GetPixelFormat()]++;
				if ( Texture2D->GetNumMips() < 2 )
				{
					NumOneMip[Texture2D->GetPixelFormat()]++;
				}
				else if ( Texture2D->GetSizeX() > Texture2D->GetSizeY() * 2 || Texture2D->GetSizeY() > Texture2D->GetSizeX() * 2 )
				{
					NumBadAspect[Texture2D->GetPixelFormat()]++;
				}
				else if ( Texture2D->GetSizeX() < 16 || Texture2D->GetSizeY() < 16 || Texture2D->GetNumMips() < 5 )
				{
					NumTooSmall[Texture2D->GetPixelFormat()]++;
				}
				else if ( (Texture2D->GetSizeX() & (Texture2D->GetSizeX() - 1)) != 0 || (Texture2D->GetSizeY() & (Texture2D->GetSizeY() - 1)) != 0 )
				{
					NumNonPow2[Texture2D->GetPixelFormat()]++;
				}
				else
				{
					// Unknown reason
					int32 Q=0;
				}
			}
			else
			{
				NumNULLResource[Texture2D->GetPixelFormat()]++;
			}
		}
	}

	// Output stats.
	{
		UE_LOG(LogContentStreaming, Log, TEXT("Texture memory usage:"));
		FTextureGroupStats TotalStats;
		for ( int32 GroupIndex=0; GroupIndex < TEXTUREGROUP_MAX; ++GroupIndex )
		{
			FTextureGroupStats& Stat = TextureGroupStats[GroupIndex];
			TotalStats.NumTextures				+= Stat.NumTextures;
			TotalStats.NumNonStreamingTextures	+= Stat.NumNonStreamingTextures;
			TotalStats.CurrentTextureSize		+= Stat.CurrentTextureSize;
			TotalStats.WantedTextureSize		+= Stat.WantedTextureSize;
			TotalStats.MaxTextureSize			+= Stat.MaxTextureSize;
			TotalStats.NonStreamingSize			+= Stat.NonStreamingSize;
			UE_LOG(LogContentStreaming, Log, TEXT("%34s: NumTextures=%4d, Current=%8.1f KB, Wanted=%8.1f KB, OnDisk=%8.1f KB, NumNonStreaming=%4d, NonStreaming=%8.1f KB"),
				UTexture::GetTextureGroupString((TextureGroup)GroupIndex),
				Stat.NumTextures,
				Stat.CurrentTextureSize / 1024.0f,
				Stat.WantedTextureSize / 1024.0f,
				Stat.MaxTextureSize / 1024.0f,
				Stat.NumNonStreamingTextures,
				Stat.NonStreamingSize / 1024.0f );
		}
		UE_LOG(LogContentStreaming, Log, TEXT("%34s: NumTextures=%4d, Current=%8.1f KB, Wanted=%8.1f KB, OnDisk=%8.1f KB, NumNonStreaming=%4d, NonStreaming=%8.1f KB"),
			TEXT("Total"),
			TotalStats.NumTextures,
			TotalStats.CurrentTextureSize / 1024.0f,
			TotalStats.WantedTextureSize / 1024.0f,
			TotalStats.MaxTextureSize / 1024.0f,
			TotalStats.NumNonStreamingTextures,
			TotalStats.NonStreamingSize / 1024.0f );
	}
	if ( bDetailedStats )
	{
		UE_LOG(LogContentStreaming, Log, TEXT("Wasted memory due to inefficient texture storage:"));
		FTextureGroupStats TotalStats;
		for ( int32 GroupIndex=0; GroupIndex < TEXTUREGROUP_MAX; ++GroupIndex )
		{
			FTextureGroupStats& Stat = TextureGroupWaste[GroupIndex];
			TotalStats.NumTextures				+= Stat.NumTextures;
			TotalStats.NumNonStreamingTextures	+= Stat.NumNonStreamingTextures;
			TotalStats.CurrentTextureSize		+= Stat.CurrentTextureSize;
			TotalStats.WantedTextureSize		+= Stat.WantedTextureSize;
			TotalStats.MaxTextureSize			+= Stat.MaxTextureSize;
			TotalStats.NonStreamingSize			+= Stat.NonStreamingSize;
			UE_LOG(LogContentStreaming, Log, TEXT("%34s: NumTextures=%4d, Current=%8.1f KB, Wanted=%8.1f KB, OnDisk=%8.1f KB, NumNonStreaming=%4d, NonStreaming=%8.1f KB"),
				UTexture::GetTextureGroupString((TextureGroup)GroupIndex),
				Stat.NumTextures,
				Stat.CurrentTextureSize / 1024.0f,
				Stat.WantedTextureSize / 1024.0f,
				Stat.MaxTextureSize / 1024.0f,
				Stat.NumNonStreamingTextures,
				Stat.NonStreamingSize / 1024.0f );
		}
		UE_LOG(LogContentStreaming, Log, TEXT("%34s: NumTextures=%4d, Current=%8.1f KB, Wanted=%8.1f KB, OnDisk=%8.1f KB, NumNonStreaming=%4d, NonStreaming=%8.1f KB"),
			TEXT("Total Wasted"),
			TotalStats.NumTextures,
			TotalStats.CurrentTextureSize / 1024.0f,
			TotalStats.WantedTextureSize / 1024.0f,
			TotalStats.MaxTextureSize / 1024.0f,
			TotalStats.NumNonStreamingTextures,
			TotalStats.NonStreamingSize / 1024.0f );
	}

	//@TODO: Calculate memory usage for non-pool textures properly!
//	UE_LOG(LogContentStreaming, Log,  TEXT("%34s: NumTextures=%4d, Current=%7.1f KB"), TEXT("Non-streaming pool textures"), NumNonStreamingPoolTextures, NonStreamingPoolSize/1024.0f );
//	UE_LOG(LogContentStreaming, Log,  TEXT("%34s: NumTextures=%4d, Current=%7.1f KB"), TEXT("Non-streaming non-pool textures"), NumNonStreamingTextures, NonStreamingSize/1024.0f );
#endif
}

/**
 * Prints out detailed information about streaming textures that has a name that contains the given string.
 * Triggered by the InvestigateTexture exec command.
 *
 * @param InvestigateTextureName	Partial name to match textures against
 */
void FStreamingManagerTexture::InvestigateTexture( const FString& InInvestigateTextureName )
{
	bTriggerInvestigateTexture = false;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for ( int32 TextureIndex=0; TextureIndex < StreamingTextures.Num(); ++TextureIndex )
	{
		FStreamingTexture& StreamingTexture = StreamingTextures[TextureIndex];
		FString TextureName = StreamingTexture.Texture->GetFullName();
		if (TextureName.Contains(InInvestigateTextureName))
		{
			UTexture2D* Texture2D = StreamingTexture.Texture;
			ETextureStreamingType StreamType = StreamingTexture.GetStreamingType();
			int32 CurrentMipIndex = FMath::Max(Texture2D->GetNumMips() - StreamingTexture.ResidentMips, 0);
			int32 WantedMipIndex = FMath::Max(Texture2D->GetNumMips() - StreamingTexture.WantedMips, 0);
			int32 MaxMipIndex = FMath::Max(Texture2D->GetNumMips() - StreamingTexture.MaxAllowedMips, 0);
			UE_LOG(LogContentStreaming, Log,  TEXT("Texture: %s"), *TextureName );
			UE_LOG(LogContentStreaming, Log,  TEXT("  Texture group:   %s"), UTexture::GetTextureGroupString(StreamingTexture.LODGroup) );
			if ( StreamType != StreamType_LastRenderTime && StreamingTexture.bUsesLastRenderHeuristics )
			{
				UE_LOG(LogContentStreaming, Log,  TEXT("  Stream logic:    %s and %s (%d references)"), GStreamTypeNames[StreamType], GStreamTypeNames[StreamType_LastRenderTime], StreamingTexture.LastRenderTimeRefCount );
			}
			else
			{
				UE_LOG(LogContentStreaming, Log,  TEXT("  Stream logic:    %s"), GStreamTypeNames[StreamType] );
			}
			if ( StreamingTexture.LastRenderTime > 1000000.0f )
			{
				UE_LOG(LogContentStreaming, Log,  TEXT("  Last rendertime: Never") );
			}
			else
			{
				UE_LOG(LogContentStreaming, Log,  TEXT("  Last rendertime: %.3f seconds ago"), StreamingTexture.LastRenderTime );
			}
			if ( StreamingTexture.ForceLoadRefCount > 0 )
			{
				UE_LOG(LogContentStreaming, Log,  TEXT("  Force all mips:  %d level references"), StreamingTexture.ForceLoadRefCount );
			}
			else if ( Texture2D->bGlobalForceMipLevelsToBeResident )
			{
				UE_LOG(LogContentStreaming, Log,  TEXT("  Force all mips:  bGlobalForceMipLevelsToBeResident") );
			}
			else if ( Texture2D->bForceMiplevelsToBeResident )
			{
				UE_LOG(LogContentStreaming, Log,  TEXT("  Force all mips:  bForceMiplevelsToBeResident") );
			}
			else if ( Texture2D->ShouldMipLevelsBeForcedResident() )
			{
				float CurrentTime = float(FPlatformTime::Seconds() - GStartTime);
				float TimeLeft = CurrentTime - Texture2D->ForceMipLevelsToBeResidentTimestamp;
				UE_LOG(LogContentStreaming, Log,  TEXT("  Force all mips:  %.1f seconds left"), FMath::Max(TimeLeft,0.0f) );
			}
			else if ( StreamingTexture.MipCount == 1 )
			{
				UE_LOG(LogContentStreaming, Log,  TEXT("  Force all mips:  No mip-maps") );
			}
			UE_LOG(LogContentStreaming, Log,  TEXT("  Current size:    %dx%d"), Texture2D->PlatformData->Mips[CurrentMipIndex].SizeX, Texture2D->PlatformData->Mips[CurrentMipIndex].SizeY );
			UE_LOG(LogContentStreaming, Log,  TEXT("  Wanted size:     %dx%d"), Texture2D->PlatformData->Mips[WantedMipIndex].SizeX, Texture2D->PlatformData->Mips[WantedMipIndex].SizeY );
			UE_LOG(LogContentStreaming, Log,  TEXT("  Max size:        %dx%d"), Texture2D->PlatformData->Mips[MaxMipIndex].SizeX, Texture2D->PlatformData->Mips[MaxMipIndex].SizeY );
			UE_LOG(LogContentStreaming, Log,  TEXT("  Priority:        %.3f"), StreamingTexture.CalcPriority() );
			UE_LOG(LogContentStreaming, Log,  TEXT("  Boost factor:    %.1f"), StreamingTexture.BoostFactor );
			UE_LOG(LogContentStreaming, Log,  TEXT("  Allowed mips:    %d-%d"), StreamingTexture.MinAllowedMips, StreamingTexture.MaxAllowedMips );
			UE_LOG(LogContentStreaming, Log,  TEXT("  Global mip bias: %d"), ThreadSettings.MipBias );

			float ScreenSizeFactor;
			switch ( StreamingTexture.LODGroup )
			{
				case TEXTUREGROUP_Lightmap:
					ScreenSizeFactor = GLightmapStreamingFactor;
					break;
				case TEXTUREGROUP_Shadowmap:
					ScreenSizeFactor = GShadowmapStreamingFactor;
					break;
				default:
					ScreenSizeFactor = 1.0f;
			}
			ScreenSizeFactor *= StreamingTexture.BoostFactor;

			for( int32 ViewIndex=0; ViewIndex < ThreadNumViews(); ViewIndex++ )
			{
				// Calculate distance of viewer to bounding sphere.
				const FStreamingViewInfo& ViewInfo = ThreadGetView(ViewIndex);
				UE_LOG(LogContentStreaming, Log,  TEXT("  View%d: Position=(%d,%d,%d) ScreenSize=%f Boost=%f"), ViewIndex,
					int32(ViewInfo.ViewOrigin.X), int32(ViewInfo.ViewOrigin.Y), int32(ViewInfo.ViewOrigin.Z),
					ViewInfo.ScreenSize, ViewInfo.BoostFactor);
			}

			// Iterate over all associated/ visible levels.
			for ( int32 LevelIndex=0; LevelIndex < ThreadSettings.LevelData.Num(); ++LevelIndex )
			{
				// Find instances of the texture in this level.
				FStreamingManagerTexture::FThreadLevelData& LevelData = ThreadSettings.LevelData[ LevelIndex ].Value;
				TArray<FStreamableTextureInstance4>* TextureInstances = LevelData.ThreadTextureInstances.Find( StreamingTexture.Texture );
				if ( TextureInstances )
				{
					for ( int32 InstanceIndex=0; InstanceIndex < TextureInstances->Num(); ++InstanceIndex )
					{
						const FStreamableTextureInstance4& Instance = (*TextureInstances)[InstanceIndex];
						const FVector4& CenterX = Instance.BoundingSphereX;
						const FVector4& CenterY = Instance.BoundingSphereY;
						const FVector4& CenterZ = Instance.BoundingSphereZ;
						for ( int32 PartialIndex=0; PartialIndex < 4; ++PartialIndex )
						{
							if ( CenterX[PartialIndex] < 3.402823466e+30F )
							{
								FVector Center( CenterX[PartialIndex], CenterY[PartialIndex], CenterZ[PartialIndex] );
								float Radius = Instance.BoundingSphereRadius[PartialIndex];
								float TexelFactor = Instance.TexelFactor[PartialIndex];
								float MinDistance = MAX_FLT;
								FFloatMipLevel WantedMipCount;
								bool bInside = false;
								for( int32 ViewIndex=0; ViewIndex < ThreadNumViews(); ViewIndex++ )
								{
									// Calculate distance of viewer to bounding sphere.
									const FStreamingViewInfo& ViewInfo = ThreadGetView(ViewIndex);
									float Distance = (ViewInfo.ViewOrigin - Center).Size();
									MinDistance = FMath::Min(MinDistance, Distance);
									float DistSqMinusRadiusSq = ClampMeshToCameraDistanceSquared(FMath::Square(Distance) - FMath::Square(Radius));
									if( DistSqMinusRadiusSq > 1.f )
									{
										// Outside the texture instance bounding sphere, calculate miplevel based on screen space size of bounding sphere.
										// Calculate the maximum screen space dimension in pixels.
										const float ScreenSize = ViewInfo.ScreenSize * ViewInfo.BoostFactor * ScreenSizeFactor;
										const float	ScreenSizeInTexels = TexelFactor * FMath::InvSqrtEst( DistSqMinusRadiusSq ) * ScreenSize;
										// WantedMipCount is the number of mips so we need to adjust with "+ 1".
										WantedMipCount = FMath::Max( WantedMipCount, FFloatMipLevel::FromScreenSizeInTexels(ScreenSizeInTexels) );
									}
									else
									{
										// Request all miplevels to be loaded if we're inside the bounding sphere.
										WantedMipCount = FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedMips);
										bInside = true;
										break;
									}
								}
								int32 IntWantedMipCount = WantedMipCount.ComputeMip(&StreamingTexture, ThreadSettings.MipBias, false);
								int32 WantedMip = FMath::Max(Texture2D->GetNumMips() - IntWantedMipCount, 0);
								UE_LOG(LogContentStreaming, Log, TEXT("Static: Wanted=%dx%d, Distance=%.1f, TexelFactor=%.2f, Radius=%5.1f, Position=(%d,%d,%d)%s"),
									Texture2D->PlatformData->Mips[WantedMip].SizeX, Texture2D->PlatformData->Mips[WantedMip].SizeY,
									MinDistance, TexelFactor, Radius, int32(Center.X), int32(Center.Y), int32(Center.Z),
									bInside ? TEXT(" [all mips]") : TEXT(""));
							}
						}
					}
				}
			}

			// Dump all dynamic instances using this texture:
			for ( TMap<const UPrimitiveComponent*,FSpawnedPrimitiveData>::TIterator It(ThreadSettings.SpawnedPrimitives); It; ++It )
			{
				const UPrimitiveComponent* Primitive = It.Key();
				FSpawnedPrimitiveData& PrimitiveData = It.Value();
				DumpTextureInstances( Primitive, PrimitiveData, Texture2D );
			}
		}
	}
#endif
}

void FStreamingManagerTexture::DumpTextureInstances( const UPrimitiveComponent* Primitive, FSpawnedPrimitiveData& PrimitiveData, UTexture2D* Texture2D )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const TCHAR* TypeString = (PrimitiveData.DynamicType == DPT_Level) ? TEXT("Dynamic (level)") : TEXT("Dynamic (spawned)");
	for ( int32 InstanceIndex=0; InstanceIndex < PrimitiveData.TextureInstances.Num(); ++InstanceIndex )
	{
		FSpawnedTextureInstance& Instance = PrimitiveData.TextureInstances[InstanceIndex];
		if ( Instance.Texture2D == Texture2D )
		{
			const FStreamingTexture& StreamingTexture = GetStreamingTexture(Texture2D);
			float ScreenSizeFactor;
			switch ( StreamingTexture.LODGroup )
			{
				case TEXTUREGROUP_Lightmap:
					ScreenSizeFactor = GLightmapStreamingFactor;
					break;
				case TEXTUREGROUP_Shadowmap:
					ScreenSizeFactor = GShadowmapStreamingFactor;
					break;
				default:
					ScreenSizeFactor = 1.0f;
			}
			ScreenSizeFactor *= StreamingTexture.BoostFactor;

			float MinDistance = MAX_FLT;
			FFloatMipLevel WantedMipCount;
			for( int32 ViewIndex=0; ViewIndex < ThreadNumViews(); ViewIndex++ )
			{
				// Calculate distance of viewer to bounding sphere.
				const FStreamingViewInfo& ViewInfo = ThreadGetView(ViewIndex);
				float Distance = (ViewInfo.ViewOrigin - PrimitiveData.BoundingSphere.Center).Size();
				MinDistance = FMath::Min(MinDistance, Distance);
				const float ScreenSize = ViewInfo.ScreenSize * ViewInfo.BoostFactor * ScreenSizeFactor;
				float DistSqMinusRadiusSq = ClampMeshToCameraDistanceSquared(FMath::Square(Distance) - FMath::Square(PrimitiveData.BoundingSphere.W));
				if( DistSqMinusRadiusSq > 1.f )
				{
					float TexelFactorScale = PrimitiveData.BoundingSphere.W * Instance.InvOriginalRadius;
					float TexelFactor = Instance.TexelFactor * TexelFactorScale;

					// Outside the texture instance bounding sphere, calculate miplevel based on screen space size of bounding sphere.
					// Calculate the maximum screen space dimension in pixels.
					const float	ScreenSizeInTexels = TexelFactor * FMath::InvSqrtEst( DistSqMinusRadiusSq ) * ScreenSize;
					// WantedMipCount is the number of mips so we need to adjust with "+ 1".
					WantedMipCount = FMath::Max( WantedMipCount, FFloatMipLevel::FromScreenSizeInTexels(ScreenSizeInTexels) );
				}
				else
				{
					// Request all miplevels to be loaded if we're inside the bounding sphere.
					WantedMipCount = FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedMips);
					break;
				}
			}
			int32 IntWantedMipCount = WantedMipCount.ComputeMip(&StreamingTexture, ThreadSettings.MipBias, false);
			int32 WantedMipIndex = FMath::Max(Texture2D->GetNumMips() - IntWantedMipCount, 0);
			UE_LOG(LogContentStreaming, Log, TEXT("%s: Wanted=%dx%d, Distance=%.1f, TexelFactor=%.2f, CurrentRadius=%5.1f, OriginalRadius=%5.1f, Position=(%d,%d,%d), IsRegistered=%d, Component=\"%s\""),
				TypeString, Texture2D->PlatformData->Mips[WantedMipIndex].SizeX, Texture2D->PlatformData->Mips[WantedMipIndex].SizeY,
				MinDistance, Instance.TexelFactor, PrimitiveData.BoundingSphere.W, 1.0f / Instance.InvOriginalRadius,
				int32(PrimitiveData.BoundingSphere.Center.X), int32(PrimitiveData.BoundingSphere.Center.Y), int32(PrimitiveData.BoundingSphere.Center.Z),
				PrimitiveData.bAttached ? true : false,
				*Primitive->GetReadableName() );
		}
	}
#endif
}


/*-----------------------------------------------------------------------------
	Streaming handler implementations.
-----------------------------------------------------------------------------*/

/**
 * Returns mip count wanted by this handler for the passed in texture.
 * New version for the priority system, using SIMD and no fudge factor.
 * 
 * @param	StreamingManager	Streaming manager
 * @param	Streaming Texture	Texture to determine wanted mip count for
 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
 */
FFloatMipLevel FStreamingHandlerTextureStatic::GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, float& MinDistance )
{
	FFloatMipLevel WantedMipCount;
	bool bShouldAbortLoop		= false;

	// Nothing do to if there are no views or instances.
	if( StreamingManager.ThreadNumViews() /*&& StreamingTexture.Instances.Num() > 0*/ )
	{
		float ScreenSizeFactor;
		switch ( StreamingTexture.LODGroup )
		{
			case TEXTUREGROUP_Lightmap:
				ScreenSizeFactor = GLightmapStreamingFactor;
				break;
			case TEXTUREGROUP_Shadowmap:
				ScreenSizeFactor = GShadowmapStreamingFactor;
				break;
			default:
				ScreenSizeFactor = 1.0f;
		}

		ScreenSizeFactor *= StreamingTexture.BoostFactor;

		VectorRegister MinDistanceSq4 = VectorSet((float)FLT_MAX, (float)FLT_MAX, (float)FLT_MAX, (float)FLT_MAX);
		VectorRegister MaxTexels = VectorSet(-(float)FLT_MAX, -(float)FLT_MAX, -(float)FLT_MAX, -(float)FLT_MAX);
		for ( int32 LevelIndex=0; LevelIndex < StreamingManager.ThreadSettings.LevelData.Num(); ++LevelIndex )
		{
			FStreamingManagerTexture::FThreadLevelData& LevelData = StreamingManager.ThreadSettings.LevelData[ LevelIndex ].Value;
			TArray<FStreamableTextureInstance4>* TextureInstances = LevelData.ThreadTextureInstances.Find( StreamingTexture.Texture );
			if ( TextureInstances )
			{
				for ( int32 InstanceIndex=0; InstanceIndex < TextureInstances->Num() && !bShouldAbortLoop; ++InstanceIndex )
				{
					const FStreamableTextureInstance4& TextureInstance = (*TextureInstances)[InstanceIndex];

					// Calculate distance of viewer to bounding sphere.
					const VectorRegister CenterX = VectorLoadAligned( &TextureInstance.BoundingSphereX );
					const VectorRegister CenterY = VectorLoadAligned( &TextureInstance.BoundingSphereY );
					const VectorRegister CenterZ = VectorLoadAligned( &TextureInstance.BoundingSphereZ );

					// Iterate over all view infos.
					for( int32 ViewIndex=0; ViewIndex < StreamingManager.ThreadNumViews() && !bShouldAbortLoop; ViewIndex++ )
					{
						const FStreamingViewInfo& ViewInfo = StreamingManager.ThreadGetView(ViewIndex);
						const float ScreenSizeFloat = ViewInfo.ScreenSize * ViewInfo.BoostFactor * ScreenSizeFactor;
						const VectorRegister ScreenSize = VectorLoadFloat1( &ScreenSizeFloat );
						const VectorRegister ViewOriginX = VectorLoadFloat1( &ViewInfo.ViewOrigin.X );
						const VectorRegister ViewOriginY = VectorLoadFloat1( &ViewInfo.ViewOrigin.Y );
						const VectorRegister ViewOriginZ = VectorLoadFloat1( &ViewInfo.ViewOrigin.Z );

						//const float DistSq = FVector::DistSquared( ViewInfo.ViewOrigin, TextureInstance.BoundingSphere.Center );
						VectorRegister Temp = VectorSubtract( ViewOriginX, CenterX );
						VectorRegister DistSq = VectorMultiply( Temp, Temp );
						Temp = VectorSubtract( ViewOriginY, CenterY );
						DistSq = VectorMultiplyAdd( Temp, Temp, DistSq );
						Temp = VectorSubtract( ViewOriginZ, CenterZ );
						DistSq = VectorMultiplyAdd( Temp, Temp, DistSq );

//						const float DistSqMinusRadiusSq = DistSq - FMath::Square(TextureInstance.BoundingSphere.W);
						VectorRegister DistSqMinusRadiusSq = VectorLoadAligned( &TextureInstance.BoundingSphereRadius );
						DistSqMinusRadiusSq = VectorMultiply( DistSqMinusRadiusSq, DistSqMinusRadiusSq );
						DistSqMinusRadiusSq = VectorSubtract( DistSq, DistSqMinusRadiusSq );
						MinDistanceSq4 = VectorMin( MinDistanceSq4, DistSqMinusRadiusSq );

						if ( VectorAllGreaterThan( DistSqMinusRadiusSq, VectorOne() ) )
						{
							if (StreamingTexture.LODGroup != TEXTUREGROUP_Terrain_Heightmap)
							{
								// Calculate the maximum screen space dimension in pixels.
								//const float ScreenSizeInTexels = TextureInstance.TexelFactor * FMath::InvSqrtEst( DistSqMinusRadiusSq ) * ScreenSize;
								DistSqMinusRadiusSq = VectorMax( DistSqMinusRadiusSq, VectorOne() );
								VectorRegister Texels = VectorMultiply( VectorLoadAligned( &TextureInstance.TexelFactor ), VectorReciprocalSqrt(DistSqMinusRadiusSq) );
								Texels = VectorMultiply( Texels, ScreenSize );
								MaxTexels = VectorMax( MaxTexels, Texels );
							}
							else
							{
								// To check Forced LOD...
								VectorRegister MinForcedLODs = VectorLoadAligned( &TextureInstance.TexelFactor );
								bool AllForcedLOD = !!VectorAllLesserThan(MinForcedLODs, VectorZero());
								MinForcedLODs = VectorMin( MinForcedLODs, VectorSwizzle(MinForcedLODs, 2, 3, 0, 1) );
								MinForcedLODs = VectorMin( MinForcedLODs, VectorReplicate(MinForcedLODs, 1) );
								float MinLODValue;
								VectorStoreFloat1( MinForcedLODs, &MinLODValue );

								if (MinLODValue <= 0)
								{
									WantedMipCount = FMath::Max(WantedMipCount, FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedMips - 13 - FMath::FloorToInt(MinLODValue)));
									if (WantedMipCount >= FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedMips))
									{
										MinDistance = 1.0f;
										bShouldAbortLoop = true;
									}
								}

								if (!AllForcedLOD)
								{
									// Calculate the maximum screen space dimension in pixels.
									DistSqMinusRadiusSq = VectorMax( DistSqMinusRadiusSq, VectorOne() );
									VectorRegister Texels = VectorMultiply( VectorLoadAligned( &TextureInstance.TexelFactor ), VectorReciprocalSqrt(DistSqMinusRadiusSq) );
									Texels = VectorMultiply( Texels, ScreenSize );
									MaxTexels = VectorMax( MaxTexels, Texels );
								}
							}
						}
						else
						{
							WantedMipCount = FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedMips);
							MinDistance = 1.0f;
							bShouldAbortLoop = true;
						}
						StreamingTexture.bUsesStaticHeuristics = true;
					}
				}

				if (StreamingTexture.LODGroup == TEXTUREGROUP_Terrain_Heightmap && VectorAllLesserThan(MaxTexels, VectorZero())) // All ForcedLOD cases...
				{
					MinDistance = 1.0f;
					bShouldAbortLoop = true;
				}

				if ( StreamingTexture.bUsesStaticHeuristics && !bShouldAbortLoop )
				{
					MinDistanceSq4 = VectorMin( MinDistanceSq4, VectorSwizzle(MinDistanceSq4, 2, 3, 0, 1) );
					MinDistanceSq4 = VectorMin( MinDistanceSq4, VectorReplicate(MinDistanceSq4, 1) );
					float MinDistanceSq;
					VectorStoreFloat1( MinDistanceSq4, &MinDistanceSq );
					MinDistanceSq = ClampMeshToCameraDistanceSquared(MinDistanceSq);
					if ( MinDistanceSq > 1.0f )
					{
						MaxTexels = VectorMax( MaxTexels, VectorSwizzle(MaxTexels, 2, 3, 0, 1) );
						MaxTexels = VectorMax( MaxTexels, VectorReplicate(MaxTexels, 1) );
						float ScreenSizeInTexels;
						VectorStoreFloat1( MaxTexels, &ScreenSizeInTexels );
						// WantedMipCount is the number of mips so we need to adjust with "+ 1".
						WantedMipCount = FMath::Max(WantedMipCount, FFloatMipLevel::FromScreenSizeInTexels(ScreenSizeInTexels));
						MinDistance = FMath::Min( MinDistanceSq, FMath::Sqrt( MinDistanceSq ) );
					}
					else
					{
						WantedMipCount = FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedMips);
						MinDistance = 1.0f;
					}
				}
			}
		}
	}

	return WantedMipCount;
}

/**
 * Calculates DynamicWantedMips and DynamicMinDistanceSq for all dynamic textures.
 */
void FStreamingManagerTexture::CalcDynamicWantedMips()
{
	STAT( double StartTime = FPlatformTime::Seconds() );

	// Reset the dynamic variables for all textures.
	const int32 PrefetchCount = AlignArbitrary( 4*CACHE_LINE_SIZE, sizeof(FStreamingTexture) ) / sizeof(FStreamingTexture);
	for ( int32 Index=0; Index < StreamingTextures.Num() - PrefetchCount; ++Index )
	{
		FStreamingTexture& StreamingTexture = StreamingTextures[ Index ];
		FPlatformMisc::Prefetch( &StreamingTexture, CACHE_LINE_SIZE );
		FPlatformMisc::Prefetch( &StreamingTexture, CACHE_LINE_SIZE*2 );
		FPlatformMisc::Prefetch( &StreamingTexture, CACHE_LINE_SIZE*3 );
		StreamingTexture.DynamicScreenSize = 0.0f;
		StreamingTexture.DynamicMinDistanceSq = FLT_MAX;
	}
	// The last ones
	for ( int32 Index=FMath::Max(StreamingTextures.Num() - PrefetchCount, 0); Index < StreamingTextures.Num(); ++Index )
	{
		FStreamingTexture& StreamingTexture = StreamingTextures[ Index ];
		StreamingTexture.DynamicScreenSize = 0.0f;
		StreamingTexture.DynamicMinDistanceSq = FLT_MAX;
	}

	// Iterate over all dynamic primitives.
	for ( TMap<const UPrimitiveComponent*,FSpawnedPrimitiveData>::TIterator It(ThreadSettings.SpawnedPrimitives); It; ++It )
	{
		const UPrimitiveComponent* Primitive = It.Key();
		FSpawnedPrimitiveData& PrimitiveData = It.Value();

		bool bIsPrimitiveUpToDate = true;

		// Only account for this instance if the corresponding primitive is fully attached to the scene, and has a size.
		if ( PrimitiveData.bAttached && !PrimitiveData.bPendingUpdate && !FMath::IsNearlyZero(PrimitiveData.BoundingSphere.W) )
		{
			for ( int32 TextureIndex=0; TextureIndex < PrimitiveData.TextureInstances.Num(); ++TextureIndex )
			{
				FSpawnedTextureInstance& TextureInstance = PrimitiveData.TextureInstances[TextureIndex];
				bIsPrimitiveUpToDate = bIsPrimitiveUpToDate && IsManagedStreamingTexture(TextureInstance.Texture2D);
				if ( bIsPrimitiveUpToDate )
				{
					//@TODO: Accessing UTexture2D here prevents this function from being moved into the worker thread.
					FStreamingTexture& StreamingTexture = GetStreamingTexture( TextureInstance.Texture2D );
					bIsPrimitiveUpToDate = !StreamingTexture.bNeedPrimitiveUpdate;
					if ( bIsPrimitiveUpToDate )
					{
						StreamingTexture.bUsesDynamicHeuristics = true;

						float ScreenSizeFactor = 1.0f;
						switch ( StreamingTexture.LODGroup )
						{
							case TEXTUREGROUP_Lightmap:
								ScreenSizeFactor = GLightmapStreamingFactor;
								break;
							case TEXTUREGROUP_Shadowmap:
								ScreenSizeFactor = GShadowmapStreamingFactor;
								break;
						}

						ScreenSizeFactor *= StreamingTexture.BoostFactor;

						// Iterate over all view infos.
						for( int32 ViewIndex=0; ViewIndex < ThreadNumViews(); ViewIndex++ )
						{
							const FStreamingViewInfo& ViewInfo	= ThreadGetView(ViewIndex);
							const float ScreenSize = ViewInfo.ScreenSize * ViewInfo.BoostFactor * ScreenSizeFactor;

							// Calculate distance of viewer to bounding sphere.
							const float	DistSq					= FVector::DistSquared( ViewInfo.ViewOrigin, PrimitiveData.BoundingSphere.Center );
							const float	DistSqMinusRadiusSq		= ClampMeshToCameraDistanceSquared(DistSq - FMath::Square(PrimitiveData.BoundingSphere.W));

							// Outside the texture instance bounding sphere, calculate miplevel based on screen space size of bounding sphere.
							if( DistSqMinusRadiusSq > 1.f )
							{
								// Scale the texture density based on current size vs original size.
								float TexelFactorScale			= PrimitiveData.BoundingSphere.W * TextureInstance.InvOriginalRadius;
								float TexelFactor				= TextureInstance.TexelFactor * TexelFactorScale;

								// Calculate the maximum screen space dimension in pixels.
								const float	ScreenSizeInTexels			= TexelFactor * FMath::InvSqrtEst( DistSqMinusRadiusSq ) * ScreenSize;
								StreamingTexture.DynamicScreenSize		= FMath::Max<float>( StreamingTexture.DynamicScreenSize, ScreenSizeInTexels );
								StreamingTexture.DynamicMinDistanceSq	= FMath::Min<float>( StreamingTexture.DynamicMinDistanceSq, DistSqMinusRadiusSq );
							}
							// Request all miplevels to be loaded if we're inside the bounding sphere.
							else
							{
//								StreamingTexture.DynamicScreenSize		= StreamingTexture.MaxAllowedMips;
								StreamingTexture.DynamicScreenSize		= 1024.0f*1024.0f;	// Let's say we want a 1 Mpix resolution mip, so we get max...
								StreamingTexture.DynamicMinDistanceSq	= 1.0f;
								break;
							}
						}
					}

					//@DEBUG:
//					bool bTrackedTexture = TrackTextureEvent( &StreamingTexture, StreamingTexture.Texture, StreamingTexture.bForceFullyLoad, this );
				}
			}
		}

		if ( !bIsPrimitiveUpToDate )
		{
			NotifyPrimitiveUpdated( Primitive );
		}
	}
	STAT( GStreamingDynamicPrimitivesTime += FPlatformTime::Seconds() - StartTime );
}

/**
 * Returns mip count wanted by this handler for the passed in texture. 
 * 
 * @param	StreamingManager	Streaming manager
 * @param	Streaming Texture	Texture to determine wanted mip count for
 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
 */
FFloatMipLevel FStreamingHandlerTextureLastRender::GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, float& MinDistance )
{
	FFloatMipLevel Ret;

	float SecondsSinceLastRender = StreamingTexture.LastRenderTime;	//(FApp::GetCurrentTime() - Texture->Resource->LastRenderTime);;
	StreamingTexture.bUsesLastRenderHeuristics = true;

	if( SecondsSinceLastRender < 45.0f && GStreamWithTimeFactor )
	{
		Ret = FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedMips);
		MinDistance = 0;
	}
	else if( SecondsSinceLastRender < 90.0f && GStreamWithTimeFactor )
	{
		Ret = FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedMips - 1);
		MinDistance = 1000.0f;
	}
	else
	{
		Ret = FFloatMipLevel::FromMipLevel(0);
		MinDistance = 10000.0f;
	}

	return Ret;
}

/**
 * Returns mip count wanted by this handler for the passed in texture. 
 * New version for the priority system, using SIMD and no fudge factor.
 * 
 * @param	StreamingManager	Streaming manager
 * @param	Streaming Texture	Texture to determine wanted mip count for
 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
 */
FFloatMipLevel FStreamingHandlerTextureLevelForced::GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, float& MinDistance )
{
	FFloatMipLevel Ret;
	if ( StreamingTexture.ForceLoadRefCount > 0 )
	{
		StreamingTexture.bUsesForcedHeuristics = true;
		Ret = FFloatMipLevel::FromMipLevel(StreamingTexture.MaxAllowedMips);
	}
	return Ret;
}

/*-----------------------------------------------------------------------------
	Texture streaming helper structs.
-----------------------------------------------------------------------------*/

/**
 * FStreamableTextureInstance serialize operator.
 *
 * @param	Ar					Archive to to serialize object to/ from
 * @param	TextureInstance		Object to serialize
 * @return	Returns the archive passed in
 */
FArchive& operator<<( FArchive& Ar, FStreamableTextureInstance& TextureInstance )
{
	Ar << TextureInstance.BoundingSphere;
	Ar << TextureInstance.TexelFactor;
	return Ar;
}

/**
 * FDynamicTextureInstance serialize operator.
 *
 * @param	Ar					Archive to to serialize object to/ from
 * @param	TextureInstance		Object to serialize
 * @return	Returns the archive passed in
 */
FArchive& operator<<( FArchive& Ar, FDynamicTextureInstance& TextureInstance )
{
	Ar << TextureInstance.BoundingSphere;
	Ar << TextureInstance.TexelFactor;
	Ar << TextureInstance.Texture;
	Ar << TextureInstance.bAttached;
	Ar << TextureInstance.OriginalRadius;
	return Ar;
}


/** Constructor for the texture streaming memory tracker. */
FStreamMemoryTracker::FStreamMemoryTracker()
:	PendingStreamIn(0)
,	PendingTempMemory(0)
,	CurrentStreamOut(0)
,	CurrentTempMemory(0)
{
}

/** Track 'start streaming' on the gamethread. (Memory not yet affected.) */
void FStreamMemoryTracker::GameThread_BeginUpdate( const UTexture2D& Texture )
{
	TSize ResidentSize = Texture.CalcTextureMemorySize( Texture.ResidentMips );
	TSize RequestedSize = Texture.CalcTextureMemorySize( Texture.RequestedMips );

	// Always plan on allocating temporary memory (we don't know if we're going to use in-place realloc yet).
	FPlatformAtomics::InterlockedAdd( &PendingTempMemory, ResidentSize );	//@TODO: 0 for pre-planned in-place realloc 

	if ( RequestedSize > ResidentSize )
	{
		// Plan on allocating new mips.
		FPlatformAtomics::InterlockedAdd( &PendingStreamIn, RequestedSize - ResidentSize );
	}
	else
	{
		// Plan on freeing mips.
		FPlatformAtomics::InterlockedAdd( &CurrentStreamOut, ResidentSize - RequestedSize );
	}
}

void FStreamMemoryTracker::RenderThread_Update( const UTexture2D& Texture, bool bSuccessful )
{
	TSize ResidentSize = Texture.CalcTextureMemorySize( Texture.ResidentMips );
	TSize RequestedSize = Texture.CalcTextureMemorySize( Texture.RequestedMips );

	// We're not planning to use temp memory anymore - we're now either using it or not.
	FPlatformAtomics::InterlockedAdd( &PendingTempMemory, -ResidentSize );		//@TODO: 0 for pre-planned in-place realloc

	// Did we allocate temp memory?
	if ( bSuccessful )
	{
		// When not using in-place realloc, track allocated temp memory.
		FPlatformAtomics::InterlockedAdd( &CurrentTempMemory, ResidentSize );
	}

	// Are we streaming in?
	if ( RequestedSize > ResidentSize )
	{
		// New mips have now been allocated, they're not pending anymore.
		FPlatformAtomics::InterlockedAdd( &PendingStreamIn, -(RequestedSize - ResidentSize) );
	}
}

void FStreamMemoryTracker::RenderThread_Finalize( const UTexture2D& Texture, bool bSuccessful )
{
	TSize ResidentSize = Texture.CalcTextureMemorySize( Texture.ResidentMips );
	TSize RequestedSize = Texture.CalcTextureMemorySize( Texture.RequestedMips );

	// When not using in-place realloc, track freed temp memory.
	FPlatformAtomics::InterlockedAdd( &CurrentTempMemory, -ResidentSize );	

	// Are we streaming out?
	if ( RequestedSize < ResidentSize )
	{
		// The mips have been freed or canceled.
		FPlatformAtomics::InterlockedAdd( &CurrentStreamOut, -(ResidentSize - RequestedSize) );
	}
}

/** Calculate how much texture memory is currently available for streaming. */
FStreamMemoryTracker::TSize FStreamMemoryTracker::CalcAvailableNow( FStreamMemoryTracker::TSize TotalFreeMemory, FStreamMemoryTracker::TSize MemoryMargin )
{
	// Conservative.
	return TotalFreeMemory - PendingStreamIn - PendingTempMemory - MemoryMargin;
}

/** Calculate how much texture memory will available later for streaming. */
FStreamMemoryTracker::TSize FStreamMemoryTracker::CalcAvailableLater( FStreamMemoryTracker::TSize TotalFreeMemory, FStreamMemoryTracker::TSize MemoryMargin )
{
	return TotalFreeMemory - PendingStreamIn + CurrentStreamOut + CurrentTempMemory - MemoryMargin;
}

/** Calculate how much texture memory is currently being used for temporary texture data during streaming. */
FStreamMemoryTracker::TSize FStreamMemoryTracker::CalcTempMemory()
{
	return PendingTempMemory + CurrentTempMemory;
}

/** Texture streaming memory tracker. */
FStreamMemoryTracker GStreamMemoryTracker;
