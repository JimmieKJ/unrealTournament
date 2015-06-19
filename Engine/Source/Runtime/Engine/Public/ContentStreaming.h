// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ContentStreaming.h: Definitions of classes used for content streaming.
=============================================================================*/

#pragma once

#include "Audio.h"

/*-----------------------------------------------------------------------------
	Stats.
-----------------------------------------------------------------------------*/

/**
 * Streaming stats
 */
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Streaming Textures"),STAT_StreamingTextures,STATGROUP_Streaming, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Pool Memory Size"), STAT_TexturePoolSize, STATGROUP_Streaming, FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Pool Memory Used"), STAT_TexturePoolAllocatedSize, STATGROUP_Streaming, FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Streaming Textures"),STAT_StreamingTexturesSize,STATGROUP_Streaming,FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Non-streaming Textures"),STAT_NonStreamingTexturesSize,STATGROUP_Streaming,FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Textures On Disk"),STAT_StreamingTexturesMaxSize,STATGROUP_StreamingDetails,FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Lightmaps In Memory"),STAT_LightmapMemorySize,STATGROUP_StreamingDetails,FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Lightmaps On Disk"),STAT_LightmapDiskSize,STATGROUP_StreamingDetails,FPlatformMemory::MCR_TexturePool, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Requests In Cancelation Phase"),STAT_RequestsInCancelationPhase,STATGROUP_StreamingDetails, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Requests In Update Phase"),STAT_RequestsInUpdatePhase,STATGROUP_StreamingDetails, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Requests in Finalize Phase"),STAT_RequestsInFinalizePhase,STATGROUP_StreamingDetails, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Intermediate Textures"),STAT_IntermediateTextures,STATGROUP_StreamingDetails, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Intermediate Textures Size"),STAT_IntermediateTexturesSize,STATGROUP_StreamingDetails,FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Textures Streamed In, Frame"),STAT_RequestSizeCurrentFrame,STATGROUP_StreamingDetails,FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Textures Streamed In, Total"),STAT_RequestSizeTotal,STATGROUP_StreamingDetails,FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Lightmaps Streamed In, Total"),STAT_LightmapRequestSizeTotal,STATGROUP_StreamingDetails,FPlatformMemory::MCR_TexturePool, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Game Thread Update Time"),STAT_GameThreadUpdateTime,STATGROUP_Streaming, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Streaming Latency, Average (sec)"),STAT_StreamingLatency,STATGROUP_StreamingDetails, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Streaming Bandwidth, Average (MB/s)"),STAT_StreamingBandwidth,STATGROUP_StreamingDetails, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Dynamic Streaming Total Time (sec)"),STAT_DynamicStreamingTotal,STATGROUP_StreamingDetails, );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "Buffer Creation" ), STAT_AudioResourceCreationTime, STATGROUP_Audio , );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Volume Streaming Tick"),STAT_VolumeStreamingTickTime,STATGROUP_StreamingDetails, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Streaming Volumes"),STAT_VolumeStreamingChecks,STATGROUP_StreamingDetails, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Growing Reallocations"),STAT_GrowingReallocations,STATGROUP_StreamingDetails, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Shrinking Reallocations"),STAT_ShrinkingReallocations,STATGROUP_StreamingDetails, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Full Reallocations"),STAT_FullReallocations,STATGROUP_StreamingDetails, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Failed Reallocations"),STAT_FailedReallocations,STATGROUP_StreamingDetails, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Panic Defragmentations"),STAT_PanicDefragmentations,STATGROUP_StreamingDetails, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("AddToWorld Time"),STAT_AddToWorldTime,STATGROUP_StreamingDetails, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("RemoveFromWorld Time"),STAT_RemoveFromWorldTime,STATGROUP_StreamingDetails, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("UpdateLevelStreaming Time"),STAT_UpdateLevelStreamingTime,STATGROUP_StreamingDetails, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Required Streaming Textures"),STAT_OptimalTextureSize,STATGROUP_Streaming,FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Over Budget"),STAT_StreamingOverBudget,STATGROUP_Streaming,FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Under Budget"),STAT_StreamingUnderBudget,STATGROUP_StreamingDetails,FPlatformMemory::MCR_TexturePool, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Pending Textures"),STAT_NumWantingTextures,STATGROUP_Streaming, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Static Textures In Memory"),STAT_TotalStaticTextureHeuristicSize,STATGROUP_StreamingDetails,FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("LastRenderTime Textures In Memory"),STAT_TotalLastRenderHeuristicSize,STATGROUP_StreamingDetails,FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Dynamic Textures In Memory"),STAT_TotalDynamicHeuristicSize,STATGROUP_StreamingDetails,FPlatformMemory::MCR_TexturePool, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Forced Textures In Memory"),STAT_TotalForcedHeuristicSize,STATGROUP_StreamingDetails,FPlatformMemory::MCR_TexturePool, );

// Forward declarations
struct FStreamingTexture;
struct FStreamingContext;
class FAsyncTextureStreaming;
class UActorComponent;
class UPrimitiveComponent;
class AActor;
class UTexture2D;
class FSoundSource;
template<typename T>
class FAsyncTask;
struct FStreamingManagerTexture;

/** Helper function to flush resource streaming. */
void FlushResourceStreaming();

/*-----------------------------------------------------------------------------
	Base streaming classes.
-----------------------------------------------------------------------------*/

enum EDynamicPrimitiveType
{
	DPT_Level,
	DPT_Spawned,
	DPT_MAX
};

enum ERemoveStreamingViews
{
	/** Removes normal views, but leaves override views. */
	RemoveStreamingViews_Normal,
	/** Removes all views. */
	RemoveStreamingViews_All
};



// To compute the maximum mip size required by any heuristic
// float to avoid float->int conversion, unclamped until the end
class FFloatMipLevel
{
public:
	// constructor
	FFloatMipLevel();

	/**
	 * Helper function to map screen texels to a mip level, includes the global bias
	 * @param StreamingTexture can be 0 (if texture is getting destroyed)
	 * @apram MipBias from cvar
	 * @param bOptimal only needed for stats
	 */
	int32 ComputeMip(const FStreamingTexture* StreamingTexture, float MipBias, bool bOptimal) const;
	//
	bool operator>=(const FFloatMipLevel& rhs) const;
	//
	static FFloatMipLevel FromScreenSizeInTexels(float ScreenSizeInTexels);
	
	static FFloatMipLevel FromMipLevel(int32 InMipLevel);

	void Invalidate();

	// @return true: means this handle has a mip level that needs to be considered
	bool IsHandled() const
	{
		return MipLevel > 0.0f;
	}

private:
	// -1: not set yet
	float MipLevel;
};

/**
 * Helper structure containing all relevant information for streaming.
 */
struct FStreamingViewInfo
{
	FStreamingViewInfo( const FVector& InViewOrigin, float InScreenSize, float InFOVScreenSize, float InBoostFactor, bool bInOverrideLocation, float InDuration, TWeakObjectPtr<AActor> InActorToBoost )
	:	ViewOrigin( InViewOrigin )
	,	ScreenSize( InScreenSize )
	,	FOVScreenSize( InFOVScreenSize )
	,	BoostFactor( InBoostFactor )
	,	Duration( InDuration )
	,	bOverrideLocation( bInOverrideLocation )
	,	ActorToBoost( InActorToBoost )
	{
	}
	/** View origin */
	FVector ViewOrigin;
	/** Screen size, not taking FOV into account */
	float	ScreenSize;
	/** Screen size, taking FOV into account */
	float	FOVScreenSize;
	/** A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa. */
	float	BoostFactor;
	/** How long the streaming system should keep checking this location, in seconds. 0 means just for the next Tick. */
	float	Duration;
	/** Whether this is an override location, which forces the streaming system to ignore all other regular locations */
	bool	bOverrideLocation;
	/** Optional pointer to an actor who's textures should have their streaming priority boosted */
	TWeakObjectPtr<AActor> ActorToBoost;
};

/**
 * Pure virtual base class of a streaming manager.
 */
struct IStreamingManager
{
	IStreamingManager()
	:	NumWantingResources(0)
	,	NumWantingResourcesCounter(0)
	{
	}

	/** Virtual destructor */
	virtual ~IStreamingManager()
	{}

	ENGINE_API static struct FStreamingManagerCollection& Get();

	ENGINE_API static void Shutdown();

	/** Checks if the streaming manager has already been shut down. **/
	ENGINE_API static bool HasShutdown();

	/**
	 * Calls UpdateResourceStreaming(), and does per-frame cleaning. Call once per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
	 */
	ENGINE_API virtual void Tick( float DeltaTime, bool bProcessEverything=false );

	/**
	 * Updates streaming, taking into account all current view infos. Can be called multiple times per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
	 */
	virtual void UpdateResourceStreaming( float DeltaTime, bool bProcessEverything=false ) = 0;

	/**
	 * Streams in/out all resources that wants to and blocks until it's done.
	 *
	 * @param TimeLimit					Maximum number of seconds to wait for streaming I/O. If zero, uses .ini setting
	 * @return							Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual int32 StreamAllResources( float TimeLimit=0.0f )
	{
		return 0;
	}

	/**
	 * Blocks till all pending requests are fulfilled.
	 *
	 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
	 * @param bLogResults	Whether to dump the results to the log.
	 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual int32 BlockTillAllRequestsFinished( float TimeLimit = 0.0f, bool bLogResults = false ) = 0;

	/**
	 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
	 */
	virtual void CancelForcedResources() = 0;

	/**
	 * Notifies manager of "level" change.
	 */
	virtual void NotifyLevelChange() = 0;

	/**
	 * Removes streaming views from the streaming manager. This is also called by Tick().
	 *
	 * @param RemovalType	What types of views to remove (all or just the normal views)
	 */
	void RemoveStreamingViews( ERemoveStreamingViews RemovalType );

	/**
	 * Adds the passed in view information to the static array.
	 *
	 * @param ScreenSize			Screen size
	 * @param FOVScreenSize			Screen size taking FOV into account
	 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
	 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other regular locations
	 * @param Duration				How long the streaming system should keep checking this location, in seconds. 0 means just for the next Tick.
	 * @param InActorToBoost		Optional pointer to an actor who's textures should have their streaming priority boosted
	 */
	ENGINE_API void AddViewInformation( const FVector& ViewOrigin, float ScreenSize, float FOVScreenSize, float BoostFactor=1.0f, bool bOverrideLocation=false, float Duration=0.0f, TWeakObjectPtr<AActor> InActorToBoost = NULL );

	/**
	 * Queue up view "slave" locations to the streaming system. These locations will be added properly at the next call to AddViewInformation,
	 * re-using the screensize and FOV settings.
	 *
	 * @param SlaveLocation			World-space view origin
	 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
	 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other locations
	 * @param Duration				How long the streaming system should keep checking this location, in seconds. 0 means just for the next Tick.
	 */
	void AddViewSlaveLocation( const FVector& SlaveLocation, float BoostFactor=1.0f, bool bOverrideLocation=false, float Duration=0.0f );

	/** Don't stream world resources for the next NumFrames. */
	virtual void SetDisregardWorldResourcesForFrames( int32 NumFrames ) = 0;

	/**
	 * Allows the streaming manager to process exec commands.
	 *
	 * @param InWorld World context
	 * @param Cmd	Exec command
	 * @param Ar	Output device for feedback
	 * @return		true if the command was handled
	 */
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) 
	{
		return false;
	}

	/** Adds a ULevel that has already prepared StreamingData to the streaming manager. */
	virtual void AddPreparedLevel( class ULevel* Level ) = 0;

	/** Removes a ULevel from the streaming manager. */
	virtual void RemoveLevel( class ULevel* Level ) = 0;

	/** Called when an actor is spawned. */
	virtual void NotifyActorSpawned( AActor* Actor )
	{
	}

	/** Called when a spawned actor is destroyed. */
	virtual void NotifyActorDestroyed( AActor* Actor )
	{
	}

	/**
	 * Called when a primitive is attached to an actor or another component.
	 * Replaces previous info, if the primitive was already attached.
	 *
	 * @param InPrimitive	Newly attached dynamic/spawned primitive
	 */
	virtual void NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
	{
	}

	/** Called when a primitive is detached from an actor or another component. */
	virtual void NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive )
	{
	}

	/**
	 * Called when a LastRenderTime primitive is attached to an actor or another component.
	 * Modifies the LastRenderTimeRefCount for the textures used, so that those textures can
	 * use both distance-based and LastRenderTime heuristics.
	 *
	 * @param InPrimitive	Newly attached dynamic/spawned primitive
	 */
	virtual void NotifyTimedPrimitiveAttached( const UPrimitiveComponent* InPrimitive, EDynamicPrimitiveType DynamicType )
	{
	}

	/**
	 * Called when a LastRenderTime primitive is detached from an actor or another component.
	 * Modifies the LastRenderTimeRefCount for the textures used, so that those textures can
	 * use both distance-based and LastRenderTime heuristics.
	 *
	 * @param InPrimitive	Newly detached dynamic/spawned primitive
	 */
	virtual void NotifyTimedPrimitiveDetached( const UPrimitiveComponent* InPrimitive )
	{
	}

	/**
	 * Called when a primitive has had its textured changed.
	 * Only affects primitives that were already attached.
	 * Replaces previous info.
	 */
	virtual void NotifyPrimitiveUpdated( const UPrimitiveComponent* Primitive )
	{
	}

	/** Returns the number of view infos. */
	ENGINE_API int32 GetNumViews() const
	{
		return CurrentViewInfos.Num();
	}

	/** Returns the view info by the specified index. */
	ENGINE_API const FStreamingViewInfo& GetViewInformation( int32 ViewIndex ) const
	{
		return CurrentViewInfos[ ViewIndex ];
	}

	/** Returns the number of resources that currently wants to be streamed in. */
	virtual int32 GetNumWantingResources() const
	{
		return NumWantingResources;
	}

	/**
	 * Returns the current ID for GetNumWantingResources().
	 * The ID is incremented every time NumWantingResources is updated by the streaming system (every few frames).
	 * Can be used to verify that any changes have been fully examined, by comparing current ID with
	 * what it was when the changes were made.
	 */
	virtual int32 GetNumWantingResourcesID() const
	{
		return NumWantingResourcesCounter;
	}

protected:

	/**
	 * Sets up the CurrentViewInfos array based on PendingViewInfos, LastingViewInfos and SlaveLocations.
	 * Removes out-dated LastingViewInfos.
	 *
	 * @param DeltaTime		Time since last call in seconds
	 */
	void SetupViewInfos( float DeltaTime );

	/**
	 * Adds the passed in view information to the static array.
	 *
	 * @param ViewInfos				Array to add the view to
	 * @param ViewOrigin			View origin
	 * @param ScreenSize			Screen size
	 * @param FOVScreenSize			Screen size taking FOV into account
	 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
	 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other regular locations
	 * @param Duration				How long the streaming system should keep checking this location (in seconds). 0 means just for the next Tick.
	 * @param InActorToBoost		Optional pointer to an actor who's textures should have their streaming priority boosted
	 */
	static void AddViewInfoToArray( TArray<FStreamingViewInfo> &ViewInfos, const FVector& ViewOrigin, float ScreenSize, float FOVScreenSize, float BoostFactor, bool bOverrideLocation, float Duration, TWeakObjectPtr<AActor> InActorToBoost );

	/**
	 * Remove view infos with the same location from the given array.
	 *
	 * @param ViewInfos				[in/out] Array to remove the view from
	 * @param ViewOrigin			View origin
	 */
	static void RemoveViewInfoFromArray( TArray<FStreamingViewInfo> &ViewInfos, const FVector& ViewOrigin );

	struct FSlaveLocation
	{
		FSlaveLocation( const FVector& InLocation, float InBoostFactor, bool bInOverrideLocation, float InDuration )
		:	Location( InLocation )
		,	BoostFactor( InBoostFactor )
		,	Duration( InDuration )
		,	bOverrideLocation( bInOverrideLocation )
		{
		}
		/** A location to use for distance-based heuristics next Tick(). */
		FVector		Location;
		/** A boost factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa. */
		float		BoostFactor;
		/** How long the streaming system should keep checking this location (in seconds). 0 means just for the next Tick. */
		float		Duration;
		/** Whether this is an override location, which forces the streaming system to ignore all other locations */
		bool		bOverrideLocation;
	};

	/** Current collection of views that need to be taken into account for streaming. Emptied every frame. */
	ENGINE_API static TArray<FStreamingViewInfo> CurrentViewInfos;

	/** Pending views. Emptied every frame. */
	static TArray<FStreamingViewInfo> PendingViewInfos;

	/** Views that stick around for a while. Override views are ignored if no movie is playing. */
	static TArray<FStreamingViewInfo> LastingViewInfos;

	/** Collection of view locations that will be added at the next call to AddViewInformation. */
	static TArray<FSlaveLocation> SlaveLocations;

	/** Set when Tick() has been called. The first time a new view is added, it will clear out all old views. */
	static bool bPendingRemoveViews;

	/** Number of resources that currently wants to be streamed in. */
	int32		NumWantingResources;

	/**
	 * The current counter for NumWantingResources.
	 * This counter is bumped every time NumWantingResources is updated by the streaming system (every few frames).
	 * Can be used to verify that any changes have been fully examined, by comparing current counter with
	 * what it was when the changes were made.
	 */
	int32		NumWantingResourcesCounter;
};

/**
 * Interface to add functions specifically related to texture streaming
 */
struct ITextureStreamingManager : public IStreamingManager
{
	/**
	* Updates streaming for an individual texture, taking into account all view infos.
	*
	* @param Texture		Texture to update
	*/
	virtual void UpdateIndividualTexture(UTexture2D* Texture) = 0;

	/**
	* Temporarily boosts the streaming distance factor by the specified number.
	* This factor is automatically reset to 1.0 after it's been used for mip-calculations.
	*/
	virtual void BoostTextures(AActor* Actor, float BoostFactor) = 0;

	/**
	*	Try to stream out texture mip-levels to free up more memory.
	*	@param RequiredMemorySize	- Required minimum available texture memory
	*	@return						- Whether it succeeded or not
	**/
	virtual bool StreamOutTextureData(int64 RequiredMemorySize) = 0;

	/** Adds a new texture to the streaming manager. */
	virtual void AddStreamingTexture(UTexture2D* Texture) = 0;

	/** Removes a texture from the streaming manager. */
	virtual void RemoveStreamingTexture(UTexture2D* Texture) = 0;

	/** Returns true if this is a streaming texture that is managed by the streaming manager. */
	virtual bool IsManagedStreamingTexture(const UTexture2D* Texture2D) = 0;
};

/**
 * Interface to add functions specifically related to audio streaming
 */
struct IAudioStreamingManager : public IStreamingManager
{
	/** Adds a new Sound Wave to the streaming manager. */
	virtual void AddStreamingSoundWave(USoundWave* SoundWave) = 0;

	/** Removes a Sound Wave from the streaming manager. */
	virtual void RemoveStreamingSoundWave(USoundWave* SoundWave) = 0;

	/** Returns true if this is a Sound Wave that is managed by the streaming manager. */
	virtual bool IsManagedStreamingSoundWave(const USoundWave* SoundWave) const = 0;

	/** Returns true if this Sound Wave is currently streaming a chunk. */
	virtual bool IsStreamingInProgress(const USoundWave* SoundWave) = 0;

	virtual bool CanCreateSoundSource(const FWaveInstance* WaveInstance) const = 0;

	/** Adds a new Sound Source to the streaming manager. */
	virtual void AddStreamingSoundSource(FSoundSource* SoundSource) = 0;

	/** Removes a Sound Source from the streaming manager. */
	virtual void RemoveStreamingSoundSource(FSoundSource* SoundSource) = 0;

	/** Returns true if this is a streaming Sound Source that is managed by the streaming manager. */
	virtual bool IsManagedStreamingSoundSource(const FSoundSource* SoundSource) const = 0;

	/**
	 * Gets a pointer to a chunk of audio data
	 *
	 * @param SoundWave		SoundWave we want a chunk from
	 * @param ChunkIndex	Index of the chunk we want
	 * @return Either the desired chunk or NULL if it's not loaded
	 */
	virtual const uint8* GetLoadedChunk(const USoundWave* SoundWave, uint32 ChunkIndex) const = 0;
};

/**
 * Streaming manager collection, routing function calls to streaming managers that have been added
 * via AddStreamingManager.
 */
struct FStreamingManagerCollection : public IStreamingManager
{
	/** Default constructor, initializing all member variables. */
	ENGINE_API FStreamingManagerCollection();

	/**
	 * Calls UpdateResourceStreaming(), and does per-frame cleaning. Call once per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
	 */
	ENGINE_API virtual void Tick( float DeltaTime, bool bProcessEverything=false ) override;

	/**
	 * Updates streaming, taking into account all current view infos. Can be called multiple times per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
	 */
	virtual void UpdateResourceStreaming( float DeltaTime, bool bProcessEverything=false ) override;

	/**
	 * Streams in/out all resources that wants to and blocks until it's done.
	 *
	 * @param TimeLimit					Maximum number of seconds to wait for streaming I/O. If zero, uses .ini setting
	 * @return							Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual int32 StreamAllResources( float TimeLimit=0.0f ) override;

	/**
	 * Blocks till all pending requests are fulfilled.
	 *
	 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
	 * @param bLogResults	Whether to dump the results to the log.
	 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual int32 BlockTillAllRequestsFinished( float TimeLimit = 0.0f, bool bLogResults = false ) override;

	/** Returns the number of resources that currently wants to be streamed in. */
	virtual int32 GetNumWantingResources() const override;

	/**
	 * Returns the current ID for GetNumWantingResources().
	 * The ID is bumped every time NumWantingResources is updated by the streaming system (every few frames).
	 * Can be used to verify that any changes have been fully examined, by comparing current ID with
	 * what it was when the changes were made.
	 */
	virtual int32 GetNumWantingResourcesID() const override;

	/**
	 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
	 */
	virtual void CancelForcedResources() override;

	/**
	 * Notifies manager of "level" change.
	 */
	virtual void NotifyLevelChange() override;

	/**
	 * Checks whether any kind of streaming is active
	 */
	ENGINE_API bool IsStreamingEnabled() const;

	/**
	 * Checks whether texture streaming is active
	 */
	virtual bool IsTextureStreamingEnabled() const;

	/**
	 * Gets a reference to the Texture Streaming Manager interface
	 */
	ENGINE_API ITextureStreamingManager& GetTextureStreamingManager() const;

	/**
	 * Gets a reference to the Audio Streaming Manager interface
	 */
	ENGINE_API IAudioStreamingManager& GetAudioStreamingManager() const;

	/**
	 * Adds a streaming manager to the array of managers to route function calls to.
	 *
	 * @param StreamingManager	Streaming manager to add
	 */
	void AddStreamingManager( IStreamingManager* StreamingManager );

	/**
	 * Removes a streaming manager from the array of managers to route function calls to.
	 *
	 * @param StreamingManager	Streaming manager to remove
	 */
	void RemoveStreamingManager( IStreamingManager* StreamingManager );

	/**
	 * Sets the number of iterations to use for the next time UpdateResourceStreaming is being called. This 
	 * is being reset to 1 afterwards.
	 *
	 * @param NumIterations	Number of iterations to perform the next time UpdateResourceStreaming is being called.
	 */
	void SetNumIterationsForNextFrame( int32 NumIterations );

	/** Don't stream world resources for the next NumFrames. */
	virtual void SetDisregardWorldResourcesForFrames( int32 NumFrames ) override;

	/**
	 * Disables resource streaming. Enable with EnableResourceStreaming. Disable/enable can be called multiple times nested
	 */
	void DisableResourceStreaming();

	/**
	 * Enables resource streaming, previously disabled with enableResourceStreaming. Disable/enable can be called multiple times nested
	 * (this will only actually enable when all disables are matched with enables)
	 */
	void EnableResourceStreaming();

	/**
	 * Allows the streaming manager to process exec commands.
	 *
	 * @param InWorld World context
	 * @param Cmd	Exec command
	 * @param Ar	Output device for feedback
	 * @return		true if the command was handled
	 */
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

	/** Adds a ULevel to the streaming manager. */
	virtual void AddLevel( class ULevel* Level );

	/** Removes a ULevel from the streaming manager. */
	virtual void RemoveLevel( class ULevel* Level ) override;

	/** Adds a ULevel that has already prepared StreamingData to the streaming manager. */
	virtual void AddPreparedLevel( class ULevel* Level ) override;

	/** Called when an actor is spawned. */
	virtual void NotifyActorSpawned( AActor* Actor ) override;

	/** Called when a spawned actor is destroyed. */
	virtual void NotifyActorDestroyed( AActor* Actor ) override;

	/**
	 * Called when a primitive is attached to an actor or another component.
	 * Replaces previous info, if the primitive was already attached.
	 *
	 * @param InPrimitive	Newly attached dynamic/spawned primitive
	 */
	virtual void NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType ) override;

	/** Called when a primitive is detached from an actor or another component. */
	virtual void NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive ) override;

	/**
	 * Called when a primitive has had its textured changed.
	 * Only affects primitives that were already attached.
	 * Replaces previous info.
	 */
	virtual void NotifyPrimitiveUpdated( const UPrimitiveComponent* Primitive ) override;

protected:

	virtual void AddOrRemoveTextureStreamingManagerIfNeeded(bool bIsInit=false);

	/** Array of streaming managers to route function calls to */
	TArray<IStreamingManager*> StreamingManagers;
	/** Number of iterations to perform. Gets reset to 1 each frame. */
	int32 NumIterations;

	/** Count of how many nested DisableResourceStreaming's were called - will enable when this is 0 */
	int32 DisableResourceStreamingCount;

	/** Maximum number of seconds to block in StreamAllResources(), by default (.ini setting). */
	float LoadMapTimeLimit;

	/** The currently added texture streaming manager. Can be NULL*/
	FStreamingManagerTexture* TextureStreamingManager;

	/** The audio streaming manager, should always exist */
	IAudioStreamingManager* AudioStreamingManager;
};

/*-----------------------------------------------------------------------------
	Texture streaming helper structs.
-----------------------------------------------------------------------------*/

struct FSpawnedTextureInstance
{
	FSpawnedTextureInstance( UTexture2D* InTexture2D, float InTexelFactor, float InOriginalRadius )
	:	Texture2D( InTexture2D )
	,	TexelFactor( InTexelFactor )
	,	InvOriginalRadius( (InOriginalRadius > 0.0f) ? (1.0f/InOriginalRadius) : 1.0f )
	{
	}
	/** Texture that is used by a dynamic UPrimitiveComponent. */
	UTexture2D*		Texture2D;
	/** Texture specific texel scale factor  */
	float			TexelFactor;
	/** 1.0f / OriginalBoundingSphereRadius, at the time the TexelFactor was calculated originally. */
	float			InvOriginalRadius;
};

/**
 * Helper class for tracking upcoming changes to the texture memory,
 * and how much of the currently allocated memory is used temporarily for streaming.
 */
struct FStreamMemoryTracker
{
#if PLATFORM_64BITS
	typedef int64 TSize;
#else
	typedef int32 TSize;
#endif
	/** Stream-in memory that hasn't been allocated yet. */
	volatile TSize PendingStreamIn;
	/** Temp memory that hasn't been allocated yet. */
	volatile TSize PendingTempMemory;
	/** Stream-out memory that is allocated but hasn't been freed yet. */
	volatile TSize CurrentStreamOut;
	/** Temp memory that is allocated, but not freed yet. */
	volatile TSize CurrentTempMemory;

	//@TODO: Possibly track canceling early (on the gamethread), not just in finalize.
	//@TODO: Add support for pre-planned in-place reallocs

	/** Constructor for the texture streaming memory tracker. */
	FStreamMemoryTracker();

	/** Track 'start streaming' on the gamethread. (Memory not yet affected.) */
	void GameThread_BeginUpdate( const UTexture2D& Texture );

	/**
	 * Track 'start streaming' on the renderthread. (Memory is now allocated/deallocated.)
	 * 
	 * @param Texture				Texture that is beginning to stream
	 * @param bSuccessful			Whether the update succeeded or not
	 */
	void RenderThread_Update( const UTexture2D& Texture, bool bSuccessful );

	/**
	 * Track 'streaming finished' on the renderthread.
	 * Note: Only called if the RenderThread Update was successful.
	 *
	 * @param Texture				Texture that is being finalized
	 * @param bSuccessful			Whether the finalize succeeded or not
	 */
	void RenderThread_Finalize( const UTexture2D& Texture, bool bSuccessful );

	/** Calculate how much texture memory is currently available for streaming. */
	TSize CalcAvailableNow( TSize TotalFreeMemory, TSize MemoryMargin );

	/** Calculate how much texture memory will available later for streaming. */
	TSize CalcAvailableLater( TSize TotalFreeMemory, TSize MemoryMargin );

	/** Calculate how much texture memory is currently being used for temporary texture data during streaming. */
	TSize CalcTempMemory();
};

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

/** Texture streaming memory tracker. */
extern FStreamMemoryTracker GStreamMemoryTracker;


/*-----------------------------------------------------------------------------
	Texture streaming.
-----------------------------------------------------------------------------*/

struct FStreamingHandlerTextureBase;
struct FTexturePriority;
#define NUM_BANDWIDTHSAMPLES 512
#define NUM_LATENCYSAMPLES 512

typedef TArray<int32, TMemStackAllocator<> > FStreamingRequests;

enum FStreamoutLogic
{
	StreamOut_UnwantedMips,
	StreamOut_AllMips,
};


/**
 * Structure containing all information needed for determining the screen space
 * size of an object/ texture instance.
 */
struct FStreamableTextureInstance4
{
	FStreamableTextureInstance4()
	:	BoundingSphereX( 3.402823466e+38F, 3.402823466e+38F, 3.402823466e+38F, 3.402823466e+38F )
	,	BoundingSphereY( 0, 0, 0, 0 )
	,	BoundingSphereZ( 0, 0, 0, 0 )
	,	BoundingSphereRadius( 0, 0, 0, 0 )
	,	TexelFactor( 0, 0, 0, 0 )
	{
	}
	/** X coordinates for the bounding sphere origin of 4 texture instances */
	FVector4 BoundingSphereX;
	/** Y coordinates for the bounding sphere origin of 4 texture instances */
	FVector4 BoundingSphereY;
	/** Z coordinates for the bounding sphere origin of 4 texture instances */
	FVector4 BoundingSphereZ;
	/** Sphere radii for the bounding sphere of 4 texture instances */
	FVector4 BoundingSphereRadius;
	/** Texel scale factors for 4 texture instances */
	FVector4 TexelFactor;
};


/**
 * Streaming manager dealing with textures.
 */
struct FStreamingManagerTexture : public ITextureStreamingManager
{
	/** Constructor, initializing all members */
	FStreamingManagerTexture();

	virtual ~FStreamingManagerTexture();

	/**
	 * Updates streaming, taking into account all current view infos. Can be called multiple times per frame.
	 *
	 * @param DeltaTime				Time since last call in seconds
	 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
	 */
	virtual void UpdateResourceStreaming( float DeltaTime, bool bProcessEverything=false ) override;

	/**
	 * Updates streaming for an individual texture, taking into account all view infos.
	 *
	 * @param Texture	Texture to update
	 */
	virtual void UpdateIndividualTexture( UTexture2D* Texture ) override;

	/**
	 * Blocks till all pending requests are fulfilled.
	 *
	 * @param TimeLimit		Optional time limit for processing, in seconds. Specifying 0 means infinite time limit.
	 * @param bLogResults	Whether to dump the results to the log.
	 * @return				Number of streaming requests still in flight, if the time limit was reached before they were finished.
	 */
	virtual int32 BlockTillAllRequestsFinished( float TimeLimit = 0.0f, bool bLogResults = false ) override;

	/**
	 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
	 */
	virtual void CancelForcedResources() override;

	/**
	 * Notifies manager of "level" change so it can prioritize character textures for a few frames.
	 */
	virtual void NotifyLevelChange() override;

	/**
	 * Adds a textures streaming handler to the array of handlers used to determine which
	 * miplevels need to be streamed in/ out.
	 *
	 * @param TextureStreamingHandler	Handler to add
	 */
	void AddTextureStreamingHandler( FStreamingHandlerTextureBase* TextureStreamingHandler );

	/**
	 * Removes a textures streaming handler from the array of handlers used to determine which
	 * miplevels need to be streamed in/ out.
	 *
	 * @param TextureStreamingHandler	Handler to remove
	 */
	void RemoveTextureStreamingHandler( FStreamingHandlerTextureBase* TextureStreamingHandler );

	/** Don't stream world resources for the next NumFrames. */
	virtual void SetDisregardWorldResourcesForFrames( int32 NumFrames ) override;

	/**
	 *	Try to stream out texture mip-levels to free up more memory.
	 *	@param RequiredMemorySize	- Required minimum available texture memory
	 *	@return						- Whether it succeeded or not
	 **/
	virtual bool StreamOutTextureData( int64 RequiredMemorySize ) override;

	/**
	 * Allows the streaming manager to process exec commands.
	 *
	 * @param InWorld World context
	 * @param Cmd	Exec command
	 * @param Ar	Output device for feedback
	 * @return		true if the command was handled
	 */
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

	/**
	 * Exec command handlers
	 */
#if STATS_FAST
	bool HandleDumpTextureStreamingStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif // STATS_FAST
#if STATS
	bool HandleListStreamingTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListStreamingTexturesCollectCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListStreamingTexturesReportReadyCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListStreamingTexturesReportCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif // STATS
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bool HandleLightmapStreamingFactorCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleCancelTextureStreamingCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleShadowmapStreamingFactorCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleNumStreamedMipsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleTrackTextureCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleListTrackedTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDebugTrackedTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleUntrackTextureCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleStreamOutCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandlePauseTextureStreamingCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleStreamingManagerMemoryCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleTextureGroupsCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleInvestigateTextureCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Adds a new texture to the streaming manager. */
	virtual void AddStreamingTexture( UTexture2D* Texture ) override;

	/** Removes a texture from the streaming manager. */
	virtual void RemoveStreamingTexture( UTexture2D* Texture ) override;

	/** Adds a ULevel to the streaming manager. */
	virtual void AddPreparedLevel( class ULevel* Level ) override;

	/** Removes a ULevel from the streaming manager. */
	virtual void RemoveLevel( class ULevel* Level ) override;

	/** Called when an actor is spawned. */
	virtual void NotifyActorSpawned( AActor* Actor ) override;

	/** Called when a spawned actor is destroyed. */
	virtual void NotifyActorDestroyed( AActor* Actor ) override;

	/**
	 * Called when a primitive is attached to an actor or another component.
	 * Replaces previous info, if the primitive was already attached.
	 *
	 * @param InPrimitive	Newly attached dynamic/spawned primitive
	 */
	virtual void NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType ) override;

	/** Called when a primitive is detached from an actor or another component. */
	virtual void NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive ) override;

	/**
	 * Called when a LastRenderTime primitive is attached to an actor or another component.
	 * Modifies the LastRenderTimeRefCount for the textures used, so that those textures can
	 * use both distance-based and LastRenderTime heuristics.
	 *
	 * @param Primitive	Newly attached dynamic/spawned primitive
	 */
	virtual void NotifyTimedPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType ) override;

	/**
	 * Called when a LastRenderTime primitive is detached from an actor or another component.
	 * Modifies the LastRenderTimeRefCount for the textures used, so that those textures can
	 * use both distance-based and LastRenderTime heuristics.
	 *
	 * @param Primitive	Newly detached dynamic/spawned primitive
	 */
	virtual void NotifyTimedPrimitiveDetached( const UPrimitiveComponent* Primitive ) override;

	/**
	 * Called when a primitive has had its textured changed.
	 * Only affects primitives that were already attached.
	 * Replaces previous info.
	 */
	virtual void NotifyPrimitiveUpdated( const UPrimitiveComponent* Primitive ) override;

	bool AddDynamicPrimitive( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType );
	bool RemoveDynamicPrimitive( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType );

	/** Returns the corresponding FStreamingTexture for a UTexture2D. */
	FStreamingTexture& GetStreamingTexture( const UTexture2D* Texture2D );

	/** Returns true if this is a streaming texture that is managed by the streaming manager. */
	virtual bool IsManagedStreamingTexture( const UTexture2D* Texture2D ) override;

	/** Updates the I/O state of a texture (allowing it to progress to the next stage) and some stats. */
	void UpdateTextureStatus( FStreamingTexture& StreamingTexture, FStreamingContext& Context );

	/**
	 * Starts streaming in/out a texture.
	 *
	 * @param StreamingTexture			Texture to start to stream in/out
	 * @param WantedMips				Number of mips we want in memory for this texture.
	 * @param Context					Context for the current frame
	 * @param bIgnoreMemoryConstraints	Whether to ignore memory constraints and always start streaming
	 * @return							true if the texture is now in flight
	 */
	bool StartStreaming( FStreamingTexture& StreamingTexture, int32 WantedMips, FStreamingContext& Context, bool bIgnoreMemoryConstraints );

	/**
	 * Cancels the current streaming request for the specified texture.
	 *
	 * @param StreamingTexture		Texture to cancel streaming for
	 * @return						true if a streaming request was canceled
	 */
	bool CancelStreamingRequest( FStreamingTexture& StreamingTexture );

	/** Resets the streaming statistics to zero. */
	void ResetStreamingStats();

	/**
	 * Updates the streaming statistics with current frame's worth of stats.
	 *
	 * @param Context					Context for the current frame
	 * @param bAllTexturesProcessed		Whether all processing is complete
	 */
	void UpdateStreamingStats( const FStreamingContext& Context, bool bAllTexturesProcessed );

	/** Returns the number of cached view infos (thread-safe). */
	int32 ThreadNumViews()
	{
		return ThreadSettings.ThreadViewInfos.Num();
	}

	/** Returns a cached view info for the specified index (thread-safe). */
	const FStreamingViewInfo& ThreadGetView( int32 ViewIndex )
	{
		return ThreadSettings.ThreadViewInfos[ ViewIndex ];
	}

#if STATS
	/** Ringbuffer of bandwidth samples for streaming in mip-levels (MB/sec). */
	static float BandwidthSamples[NUM_BANDWIDTHSAMPLES];
	/** Number of bandwidth samples that have been filled in. Will stop counting when it reaches NUM_BANDWIDTHSAMPLES. */
	static int32 NumBandwidthSamples;
	/** Current sample index in the ring buffer. */
	static int32 BandwidthSampleIndex;
	/** Average of all bandwidth samples in the ring buffer, in MB/sec. */
	static float BandwidthAverage;
	/** Maximum bandwidth measured since the start of the game.  */
	static float BandwidthMaximum;
	/** Minimum bandwidth measured since the start of the game.  */
	static float BandwidthMinimum;
#endif


protected:
//BEGIN: Thread-safe functions and data
		friend class FAsyncTextureStreaming;
		friend struct FStreamingHandlerTextureStatic;

		/** Calculates the minimum and maximum number of mip-levels for a streaming texture. */
		void CalcMinMaxMips( FStreamingTexture& StreamingTexture );

		/** Calculates the number of mip-levels we would like to have in memory for a texture. */
		void CalcWantedMips( FStreamingTexture& StreamingTexture );

		/**
		 * Fallback handler to catch textures that have been orphaned recently.
		 * This handler prevents massive spike in texture memory usage.
		 * Orphaned textures were previously streamed based on distance but those instance locations have been removed -
		 * for instance because a ULevel is being unloaded. These textures are still active and in memory until they are garbage collected,
		 * so we must ensure that they do not start using the LastRenderTime fallback handler and suddenly stream in all their mips -
		 * just to be destroyed shortly after by a garbage collect.
		 */
		FFloatMipLevel GetWantedMipsForOrphanedTexture( FStreamingTexture& StreamingTexture, float& Distance );

		/** Updates this frame's STATs by one texture. */
		void UpdateFrameStats( FStreamingContext& Context, FStreamingTexture& StreamingTexture, int32 TextureIndex );

		/**
		 * Not thread-safe: Updates a portion (as indicated by 'StageIndex') of all streaming textures,
		 * allowing their streaming state to progress.
		 *
		 * @param Context			Context for the current stage (frame)
		 * @param StageIndex		Current stage index
		 * @param NumUpdateStages	Number of texture update stages
		 */
		void UpdateStreamingTextures( FStreamingContext& Context, int32 StageIndex, int32 NumStages );

		/** Adds new textures and level data on the gamethread (while the worker thread isn't active). */
		void UpdateThreadData();

		/** Checks for updates in the user settings (CVars, etc). */
		void CheckUserSettings();

		/**
		 * Temporarily boosts the streaming distance factor by the specified number.
		 * This factor is automatically reset to 1.0 after it's been used for mip-calculations.
		 */
		void BoostTextures( AActor* Actor, float BoostFactor ) override;

		/** Updates the thread-safe cache information for dynamic primitives. */
		void UpdateDynamicPrimitiveCache();

		/** Calculates DynamicWantedMips and DynamicMinDistance for all dynamic textures. */
		void CalcDynamicWantedMips();

		/**
		 * Stream textures in/out, based on the priorities calculated by the async work.
		 *
		 * @param bProcessEverything	Whether we're processing all textures in one go
		 */
		void StreamTextures( bool bProcessEverything );

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
		int32 StreamoutTextures( FStreamoutLogic StreamoutLogic, int64& AvailableLater, int64& TempMemoryUsed, int32 StartIndex, int32 StopIndex, int32& LowPrioIndex, const TArray<FTexturePriority>& PrioritizedTextures, FStreamingRequests& StreamingRequests );

		/**
		 * Stream textures in/out, when no texture pool with limited size is used by the platform.
		 *
		 * @param Context				Context for the current stage
		 * @param PrioritizedTextures	Array of prioritized textures to process
		 * @param TempMemoryUsed		Current amount of temporary memory used by the streaming system, in bytes
		 */
		void StreamTexturesUnlimited( FStreamingContext& Context, const TArray<FTexturePriority>& PrioritizedTextures, int64 TempMemoryUsed );

		/** Thread-safe helper struct for per-level information. */
		struct FThreadLevelData
		{
			FThreadLevelData()
			:	bRemove( false )
			{
			}

			/** Whether this level has been removed. */
			bool bRemove;

			/** Texture instances used in this level, stored in SIMD-friendly format. */
			TMap<const UTexture2D*,TArray<FStreamableTextureInstance4> > ThreadTextureInstances;
		};

		/** Texture instance data for a spawned primitive. Stored in the ThreadSettings.SpawnedPrimitives map. */
		struct FSpawnedPrimitiveData
		{
			FSpawnedPrimitiveData()
			:	bAttached( false )
			,	bPendingUpdate( false )
			{
			}
			/** Texture instances used by primitive. */
			TArray<FSpawnedTextureInstance> TextureInstances;
			/** Bounding sphere of primitive. */
			FSphere		BoundingSphere;
			/** Dynamic primitive tracking type. */
			EDynamicPrimitiveType DynamicType;
			/** Whether the primitive that uses this texture is currently attached to the scene or not. */
			uint32	bAttached : 1;
			/** Set to true when it's marked for Attach or Detach. Don't touch this primitive until it's been fully updated. */
			uint32	bPendingUpdate : 1;
		};

		typedef TKeyValuePair< class ULevel*, FThreadLevelData >	FLevelData;

		/** Thread-safe helper struct for streaming information. */
		struct FThreadSettings
		{
			/** Cached from the system settings. */
			int32 NumStreamedMips[TEXTUREGROUP_MAX];

			/** Cached from each ULevel. */
			TArray< FLevelData > LevelData;

			/** Cached from FStreamingManagerBase. */
			TArray<FStreamingViewInfo> ThreadViewInfos;

			/** Maps spawned primitives to texture instances. Owns the instance data. */
			TMap<const UPrimitiveComponent*,FSpawnedPrimitiveData> SpawnedPrimitives;

			/** from cvar, >=0 */
			float MipBias;
		};

		/** Thread-safe helper data for streaming information. */
		FThreadSettings	ThreadSettings;

		/** All streaming UTexture2D objects. */
		TArray<FStreamingTexture> StreamingTextures;

		/** Index of the StreamingTexture that will be updated next by UpdateStreamingTextures(). */
		int32 CurrentUpdateStreamingTextureIndex;
//END: Thread-safe functions and data

	/**
	 * Mark the textures instances with a timestamp. They're about to lose their location-based heuristic and we don't want them to
	 * start using LastRenderTime heuristic for a few seconds until they are garbage collected!
	 *
	 * @param PrimitiveData		Our data structure for the spawned primitive that is being detached.
	 */
	void	SetInstanceRemovedTimestamp( FSpawnedPrimitiveData& PrimitiveData );

	void	DumpTextureGroupStats( bool bDetailedStats );

	/**
	 * Prints out detailed information about streaming textures that has a name that contains the given string.
	 * Triggered by the InvestigateTexture exec command.
	 *
	 * @param InvestigateTextureName	Partial name to match textures against
	 */
	void	InvestigateTexture( const FString& InvestigateTextureName );

	void	DumpTextureInstances( const UPrimitiveComponent* Primitive, FSpawnedPrimitiveData& PrimitiveData, UTexture2D* Texture2D );

	/** Next sync, dump texture group stats. */
	bool	bTriggerDumpTextureGroupStats;

	/** Whether to the dumped texture group stats should contain extra information. */
	bool	bDetailedDumpTextureGroupStats;

	/** Next sync, dump all information we have about a certain texture. */
	bool	bTriggerInvestigateTexture;

	/** Name of a texture to investigate. Can be partial name. */
	FString	InvestigateTextureName;

	/** Async work for calculating priorities for all textures. */
	FAsyncTask<FAsyncTextureStreaming>*	AsyncWork;

	/** New textures, before they've been added to the thread-safe container. */
	TArray<UTexture2D*>		PendingStreamingTextures;

	struct FPendingPrimitiveType
	{
		FPendingPrimitiveType( EDynamicPrimitiveType InDynamicType, bool bInShouldTrack )
		:	DynamicType( InDynamicType)
		,	bShouldTrack( bInShouldTrack )
		{
		}
		EDynamicPrimitiveType DynamicType;
		bool bShouldTrack;
	};
	/** Textures on newly spawned primitives, before they've been added to the thread-safe container. */
	TMap<const UPrimitiveComponent*,FPendingPrimitiveType>	PendingSpawnedPrimitives;

	/** New levels, before they've been added to the thread-safe container. */
	TArray<class ULevel*>	PendingLevels;

	/** Stages [0,N-2] is non-threaded data collection, Stage N-1 is wait-for-AsyncWork-and-finalize. */
	int32					ProcessingStage;

	/** Total number of processing stages (N). */
	int32					NumTextureProcessingStages;

	/** Maximum amount of temp memory used for streaming, at any given time. */
	int64					MaxTempMemoryUsed;

	/** Whether to support texture instance streaming for dynamic (movable/spawned) objects. */
	bool					bUseDynamicStreaming;

	float					BoostPlayerTextures;

	/** Array of texture streaming objects to use during update. */
	TArray<FStreamingHandlerTextureBase*> TextureStreamingHandlers;

	/** Amount of memory to leave free in the texture pool. */
	int64					MemoryMargin;

	/** Minimum number of bytes to evict when we need to stream out textures because of a failed allocation. */
	int64					MinEvictSize;

	/** If set, UpdateResourceStreaming() will only process this texture. */
	UTexture2D*				IndividualStreamingTexture;

	// Stats we need to keep across frames as we only iterate over a subset of textures.

	/** Number of streaming textures */
	uint32 NumStreamingTextures;
	/** Number of requests in cancelation phase. */
	uint32 NumRequestsInCancelationPhase;
	/** Number of requests in mip update phase. */
	uint32 NumRequestsInUpdatePhase;
	/** Number of requests in mip finalization phase. */
	uint32 NumRequestsInFinalizePhase;
	/** Size ot all intermerdiate textures in flight. */
	uint32 TotalIntermediateTexturesSize;
	/** Number of intermediate textures in flight. */
	uint32 NumIntermediateTextures;
	/** Size of all streaming testures. */
	uint64 TotalStreamingTexturesSize;
	/** Maximum size of all streaming textures. */
	uint64 TotalStreamingTexturesMaxSize;
	/** Total number of bytes in memory, for all streaming lightmap textures. */
	uint64 TotalLightmapMemorySize;
	/** Total number of bytes on disk, for all streaming lightmap textures. */
	uint64 TotalLightmapDiskSize;
	/** Number of mip count increase requests in flight. */
	uint32 TotalMipCountIncreaseRequestsInFlight;
	/** Total number of bytes in memory, if all textures were streamed perfectly with a 1.0 fudge factor. */
	uint64 TotalOptimalWantedSize;
	/** Total number of bytes using StaticTexture heuristics, currently in memory. */
	uint64 TotalStaticTextureHeuristicSize;
	/** Total number of bytes using DynmicTexture heuristics, currently in memory. */
	uint64 TotalDynamicTextureHeuristicSize;
	/** Total number of bytes using LastRenderTime heuristics, currently in memory. */
	uint64 TotalLastRenderHeuristicSize;
	/** Total number of bytes using ForcedIntoMemory heuristics, currently in memory. */
	uint64 TotalForcedHeuristicSize;

	/** Unmodified texture pool size, in bytes, as specified in the .ini file. */
	int64 OriginalTexturePoolSize;
	/** Timestamp when we last shrunk the pool size because of memory usage. */
	double PreviousPoolSizeTimestamp;
	/** PoolSize CVar setting the last time we adjusted the pool size. */
	int32 PreviousPoolSizeSetting;

	/** Whether to collect, and optionally report, texture stats for the next run. */
	bool   bCollectTextureStats;
	bool   bReportTextureStats;
	TArray<FString> TextureStatsReport;

	/** Optional string to match against the texture names when collecting stats. */
	FString CollectTextureStatsName;

	/** Whether texture streaming is paused or not. When paused, it won't stream any textures in or out. */
	bool bPauseTextureStreaming;

#if STATS
	/**
	 * Ring buffer containing all latency samples we keep track of, as measured in seconds from the time the streaming system
	 * detects that a change is required until the data has been streamed in and the new texture is ready to be used.
	 */
	float LatencySamples[NUM_LATENCYSAMPLES];
	/** Number of latency samples that have been filled in. Will stop counting when it reaches NUM_LATENCYSAMPLES. */
	int32 NumLatencySamples;
	/** Current sample index in the ring buffer. */
	int32 LatencySampleIndex;
	/** Average of all latency samples in the ring buffer, in seconds. */
	float LatencyAverage;
	/** Maximum latency measured since the start of the game.  */
	float LatencyMaximum;

	/**
	 * Updates the streaming latency STAT for a texture.
	 *
	 * @param Texture		Texture to update for
	 * @param WantedMips	Number of desired mip-levels for the texture
	 * @param bInFlight		Whether the texture is currently being streamed
	 */
	void UpdateLatencyStats( UTexture2D* Texture, int32 WantedMips, bool bInFlight );

	/** Total time taken for each processing stage. */
	TArray<double> StreamingTimes;
#endif

#if STATS_FAST
	uint64 MaxStreamingTexturesSize;
	uint64 MaxOptimalTextureSize;
	int64 MaxStreamingOverBudget;
	uint64 MaxTexturePoolAllocatedSize;
	uint64 MinLargestHoleSize;
	uint32 MaxNumWantingTextures;
#endif
	
	friend bool TrackTextureEvent( FStreamingTexture* StreamingTexture, UTexture2D* Texture, bool bForceMipLevelsToBeResident, const FStreamingManagerTexture* Manager);
};

/*-----------------------------------------------------------------------------
	Texture streaming handler.
-----------------------------------------------------------------------------*/

/**
 * Base of texture streaming handler functionality.
 */
struct FStreamingHandlerTextureBase
{
	/** Friendly name identifying the handler for debug purposes. */
	const TCHAR* HandlerName;

	/** Default constructor. */
	FStreamingHandlerTextureBase()
		: HandlerName(TEXT("Base"))
	{
	}

	/**
	 * Returns mip count wanted by this handler for the passed in texture. 
	 * 
	 * @param	StreamingManager	Streaming manager
	 * @param	Streaming Texture	Texture to determine wanted mip count for
	 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
	 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
	 */
	virtual FFloatMipLevel GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, float& MinDistance ) = 0;
};

/**
 * Static texture streaming handler. Used to stream textures on static level geometry.
 */
struct FStreamingHandlerTextureStatic : public FStreamingHandlerTextureBase
{
	/** Default constructor. */
	FStreamingHandlerTextureStatic()
	{
		HandlerName = TEXT("Static");
	}

	/**
	 * Returns mip count wanted by this handler for the passed in texture. 
	 * 
	 * @param	StreamingManager	Streaming manager
	 * @param	Streaming Texture	Texture to determine wanted mip count for
	 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
	 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
	 */
	virtual FFloatMipLevel GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, float& MinDistance );
};

/**
 * Streaming handler that bases its decision on the last render time.
 */
struct FStreamingHandlerTextureLastRender : public FStreamingHandlerTextureBase
{
	/** Default constructor. */
	FStreamingHandlerTextureLastRender()
	{
		HandlerName = TEXT("LastRender");
	}

	/**
	 * Returns mip count wanted by this handler for the passed in texture. 
	 * 
	 * @param	StreamingManager	Streaming manager
	 * @param	Streaming Texture	Texture to determine wanted mip count for
	 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
	 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
	 */
	virtual FFloatMipLevel GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, float& MinDistance );
};

/**
 *	Streaming handler that bases its decision on the bForceMipStreaming flag in PrimitiveComponent.
 */
struct FStreamingHandlerTextureLevelForced : public FStreamingHandlerTextureBase
{
	/** Default constructor. */
	FStreamingHandlerTextureLevelForced()
	{
		HandlerName = TEXT("LevelForced");
	}

	/**
	 * Returns mip count wanted by this handler for the passed in texture. 
	 * 
	 * @param	StreamingManager	Streaming manager
	 * @param	Streaming Texture	Texture to determine wanted mip count for
	 * @param	MinDistance			[out] Distance to the closest instance of this texture, in world space units.
	 * @return	Number of miplevels that should be streamed in or INDEX_NONE if texture isn't handled by this handler.
	 */
	virtual FFloatMipLevel GetWantedMips( FStreamingManagerTexture& StreamingManager, FStreamingTexture& StreamingTexture, float& MinDistance );
};
