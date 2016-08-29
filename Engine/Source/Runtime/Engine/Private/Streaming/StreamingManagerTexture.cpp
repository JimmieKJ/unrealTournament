// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureStreamingManager.cpp: Implementation of content streaming classes.
=============================================================================*/

#include "EnginePrivate.h"
#include "StreamingManagerTexture.h"
#include "TextureInstanceManager.h"
#include "AsyncTextureStreaming.h"

void Renderthread_StreamOutTextureData(FRHICommandListImmediate& RHICmdList, TArray<FTextureSortElement>* InCandidateTextures, int64 RequiredMemorySize, volatile bool* bSucceeded);
bool TrackTexture( const FString& TextureName );
bool UntrackTexture( const FString& TextureName );
void ListTrackedTextures( FOutputDevice& Ar, int32 NumTextures );

/**
 * Helper function to clamp the mesh to camera distance
 */
FORCEINLINE float ClampMeshToCameraDistanceSquared(float MeshToCameraDistanceSquared)
{
	// called from streaming thread, maybe even main thread
	return FMath::Max<float>(MeshToCameraDistanceSquared, 0.0f);
}

/*-----------------------------------------------------------------------------
	FStreamingManagerTexture implementation.
-----------------------------------------------------------------------------*/

/** Constructor, initializing all members and  */
FStreamingManagerTexture::FStreamingManagerTexture()
:	CurrentUpdateStreamingTextureIndex(0)
,	bTriggerDumpTextureGroupStats( false )
,	bDetailedDumpTextureGroupStats( false )
,	AsyncWork( nullptr )
,	ProcessingStage( 0 )
,	NumTextureProcessingStages(5)
,	bUseDynamicStreaming( false )
,	BoostPlayerTextures( 3.0f )
,	MemoryMargin(0)
,	MinEvictSize(0)
,	EffectiveStreamingPoolSize(0)
,	MemoryOverBudget(0)
,	MaxEverRequired(0)
,	OriginalTexturePoolSize(0)
,	PreviousPoolSizeTimestamp(0.0)
,	PreviousPoolSizeSetting(-1)
,	bPauseTextureStreaming(false)
,	LastWorldUpdateTime(GIsEditor ? -FLT_MAX : 0) // In editor, visibility is not taken into consideration.
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

	UE_LOG(LogContentStreaming,Log,TEXT("Texture pool size is %.2f MB"),GTexturePoolSize/1024.f/1024.f);

	// Convert from MByte to byte.
	MinEvictSize *= 1024 * 1024;
	MemoryMargin *= 1024 * 1024;

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
		NumStreamedMips[LODGroup] = TexGroup.NumStreamedMips;
	}

	// setup the streaming resource flush function pointer
	GFlushStreamingFunc = &FlushResourceStreaming;

	ProcessingStage = 0;
	AsyncWork = new FAsyncTask<FAsyncTextureStreamingTask>(this);
}

FStreamingManagerTexture::~FStreamingManagerTexture()
{
	AsyncWork->EnsureCompletion();
	delete AsyncWork;

	// Clear the stats
	DisplayedStats.Reset();
	STAT(DisplayedStats.Apply();)
}

/**
 * Cancels the timed Forced resources (i.e used the Kismet action "Stream In Textures").
 */
void FStreamingManagerTexture::CancelForcedResources()
{
	// Update textures that are Forced on a timer.
	for ( int32 TextureIndex=0; TextureIndex < StreamingTextures.Num(); ++TextureIndex )
	{
		FStreamingTexture& StreamingTexture = StreamingTextures[ TextureIndex ];

		// Make sure this streaming texture hasn't been marked for removal.
		if ( StreamingTexture.Texture )
		{
			// Remove any prestream requests from textures
			float TimeLeft = (float)(StreamingTexture.Texture->ForceMipLevelsToBeResidentTimestamp - FApp::GetCurrentTime());
			if ( TimeLeft >= 0.0f )
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
 *	Try to stream out texture mip-levels to free up more memory.
 *	@param RequiredMemorySize	- Additional texture memory required
 *	@return						- Whether it succeeded or not
 **/
bool FStreamingManagerTexture::StreamOutTextureData( int64 RequiredMemorySize )
{
	const int64 MaxTempMemoryAllowed = Settings.MaxTempMemoryAllowed * 1024 * 1024;
	RequiredMemorySize = FMath::Max<int64>(RequiredMemorySize, MinEvictSize);

	// Array of candidates for reducing mip-levels.
	TArray<FTextureSortElement> CandidateTextures;
	CandidateTextures.Reserve( StreamingTextures.Num() );

	// Don't stream out character textures (to begin with)
	volatile bool bSucceeded = false;
	
	//resizing textures actually creates a temp copy so we can only resize so many at a time without running out of memory during the eject itself.
	int64 MemoryCostToResize = 0;	

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
			if ( IsStreamingTexture(Texture) && Texture->IsReadyForStreaming() && Texture->ResidentMips > NumRequiredResidentMips  )
			{
				// We can't stream out mip-tails.
				int32 CurrentBaseMip = NumMips - Texture->ResidentMips;
				if ( MipTailBaseIndex < 0 || CurrentBaseMip < MipTailBaseIndex )
				{
					if ( !Texture->ShouldMipLevelsBeForcedResident() && Texture->Resource )
					{
						// Don't try to stream out if the texture is currently being busy being streamed in/out.
						bool bSafeToStream = (Texture->UpdateStreamingStatus() == false);
						if ( bSafeToStream )
						{
							uint32 TextureDataAddress = 0;
							MemoryCostToResize += Texture->CalcTextureMemorySize(FMath::Max(0, Texture->ResidentMips - 1));
							CandidateTextures.Add( FTextureSortElement(Texture, Texture->CalcTextureMemorySize(Texture->ResidentMips), StreamingTexture.bIsCharacterTexture, TextureDataAddress, NumRequiredResidentMips) );
						}
					}
				}
			}
		}

		if (MemoryCostToResize >= MaxTempMemoryAllowed)
		{			
			// Queue up the process on the render thread.
			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
				StreamOutTextureDataCommand,
				TArray<FTextureSortElement>*, CandidateTextures, &CandidateTextures,
				int64, RequiredMemorySize, RequiredMemorySize,
				volatile bool*, bSucceeded, &bSucceeded,
				{				
				FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);				
				RHIFlushResources();

				Renderthread_StreamOutTextureData(RHICmdList, CandidateTextures, RequiredMemorySize, bSucceeded);

				FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
				RHIFlushResources();
			});

			// Block until the command has finished executing.
			FlushRenderingCommands();
			MemoryCostToResize = 0;			
			CandidateTextures.Reset();
		}
	}	

	if (CandidateTextures.Num() > 0)
	{
		// Queue up the process on the render thread.
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			StreamOutTextureDataCommand,
			TArray<FTextureSortElement>*, CandidateTextures, &CandidateTextures,
			int64, RequiredMemorySize, RequiredMemorySize,
			volatile bool*, bSucceeded, &bSucceeded,
			{
			Renderthread_StreamOutTextureData(RHICmdList, CandidateTextures, RequiredMemorySize, bSucceeded);

			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
			RHIFlushResources();
		});
	}

	// Block until the command has finished executing.
	FlushRenderingCommands();

	// Reset the streaming system, so it picks up any changes to UTexture2D ResidentMips and RequestedMips.
	ProcessingStage = 0;

	return bSucceeded;
}

void FStreamingManagerTexture::IncrementalUpdate( float Percentage )
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FStreamingManagerTexture_IncrementalUpdate);

	FRemovedTextureArray RemovedTextures;

	for (FLevelTextureManager& LevelManager : LevelTextureManagers)
	{
		LevelManager.IncrementalUpdate(DynamicComponentManager, RemovedTextures, Percentage, bUseDynamicStreaming); // Complete the incremental update.
	}

	DynamicComponentManager.IncrementalUpdate(RemovedTextures, Percentage); // Complete the incremental update.

	SetTexturesRemovedTimestamp(RemovedTextures);
}

void FStreamingManagerTexture::ProcessRemovedTextures()
{
	for (int32 TextureIndex : RemovedTextureIndices)
	{
		// Remove swap all elements, until this entry has a valid texture.
		while (StreamingTextures.IsValidIndex(TextureIndex) && !StreamingTextures[TextureIndex].Texture)
		{
			StreamingTextures.RemoveAtSwap(TextureIndex);
		}

		if (StreamingTextures.IsValidIndex(TextureIndex))
		{
			// Update the texture with its new index.
			StreamingTextures[TextureIndex].Texture->StreamingIndex = TextureIndex;
		}
	}
	RemovedTextureIndices.Empty();
}

void FStreamingManagerTexture::ProcessAddedTextures()
{
	// Add new textures.
	StreamingTextures.Reserve(StreamingTextures.Num() + PendingStreamingTextures.Num());
	for (int32 TextureIndex=0; TextureIndex < PendingStreamingTextures.Num(); ++TextureIndex)
	{
		UTexture2D* Texture = PendingStreamingTextures[TextureIndex];
		if (!Texture) continue; // Could be null if it was removed after being added.

		Texture->StreamingIndex = StreamingTextures.Num();
		new (StreamingTextures) FStreamingTexture(Texture, NumStreamedMips, Settings);
	}
	PendingStreamingTextures.Empty();
}

void FStreamingManagerTexture::ConditionalUpdateStaticData()
{
	static float PreviousLightmapStreamingFactor = GLightmapStreamingFactor;
	static float PreviousShadowmapStreamingFactor = GShadowmapStreamingFactor;
	static FTextureStreamingSettings PreviousSettings = Settings;

	if (PreviousLightmapStreamingFactor != GLightmapStreamingFactor || 
		PreviousShadowmapStreamingFactor != GShadowmapStreamingFactor || 
		PreviousSettings != Settings)
	{
		// Update each texture static data.
		for (FStreamingTexture& StreamingTexture : StreamingTextures)
		{
			StreamingTexture.UpdateStaticData(Settings);
		}

#if 0
		// Reinsert all levels to update static instance data. Only for testing perfs.
		TArray<ULevel*> Levels;
		for (FLevelTextureManager& LevelManager : LevelTextureManagers)
		{
			Levels.Push(LevelManager.GetLevel());
		}
		for (ULevel* Level : Levels)
		{
			AddLevel(Level);
		}
#endif

		// Update the cache variables.
		PreviousLightmapStreamingFactor = GLightmapStreamingFactor;
		PreviousShadowmapStreamingFactor = GShadowmapStreamingFactor;
		PreviousSettings = Settings;
	}
}

/**
 * Adds new textures and level data on the gamethread (while the worker thread isn't active).
 */
void FStreamingManagerTexture::UpdateThreadData(bool bProcessEverything)
{
	// Update the textures, can only be done while the async task is stopped since it accessed this StreamingTextures.
	ProcessRemovedTextures();
	ProcessAddedTextures();

	Settings.Update();
	ConditionalUpdateStaticData();

	// Fully complete all pending update.
	IncrementalUpdate(1.f);

	// Update the thread data.

	FAsyncTextureStreamingTask& AsyncTask = AsyncWork->GetTask();
	FTextureMemoryStats Stats;
	RHIGetTextureMemoryStats(Stats);

	// When processing all textures, we need unlimited budget so that textures get all at their required states.
	// Same when forcing stream-in, for which we want all used textures to be fully loaded 
	if (Stats.IsUsingLimitedPoolSize() && !bProcessEverything && !Settings.bFullyLoadUsedTextures)
	{
		const int64 TempMemoryBudget = Settings.MaxTempMemoryAllowed * 1024 * 1024;
		AsyncTask.Reset(Stats.TotalGraphicsMemory, Stats.AllocatedMemorySize, Stats.TexturePoolSize, TempMemoryBudget, MemoryMargin);
	}
	else
	{
		// Temp must be smaller since membudget only updates if it has a least temp memory available.
		AsyncTask.Reset(0, Stats.AllocatedMemorySize, MAX_int64, MAX_int64 / 2, 0);
	}
	AsyncTask.StreamingData.Init(CurrentViewInfos, LastWorldUpdateTime, LevelTextureManagers, DynamicComponentManager);
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
				for ( UTexture* Texture : Textures )
				{
					FStreamingTexture* StreamingTexture = GetStreamingTexture( Cast<UTexture2D>( Texture ) );
					if ( StreamingTexture )
					{
						StreamingTexture->DynamicBoostFactor = FMath::Max( StreamingTexture->DynamicBoostFactor, BoostFactor );
					}
				}
			}
		}
	}
}

/** Adds a ULevel to the streaming manager. */
void FStreamingManagerTexture::AddLevel( ULevel* Level )
{
	// Remove any data related to this level first.
	RemoveLevel(Level);

	// If the level was not already there, add a new entry.
	new (LevelTextureManagers) FLevelTextureManager(Level);
}

/** Removes a ULevel from the streaming manager. */
void FStreamingManagerTexture::RemoveLevel( ULevel* Level )
{
	const double CurrentTime = FApp::GetCurrentTime();

	for (int32 Index = 0; Index < LevelTextureManagers.Num(); ++Index)
	{
		FLevelTextureManager& LevelManager = LevelTextureManagers[Index];
		if (LevelManager.GetLevel() != Level) continue;

		FRemovedTextureArray RemovedTextures;
		LevelManager.Remove(DynamicComponentManager, RemovedTextures);
		SetTexturesRemovedTimestamp(RemovedTextures);

		// Remove the level entry. The async task view will still be valid as it uses a shared ptr.
		LevelTextureManagers.RemoveAtSwap(Index);
		break;
	}
}

/**
 * Adds a new texture to the streaming manager.
 */
void FStreamingManagerTexture::AddStreamingTexture( UTexture2D* Texture )
{
	// Adds the new texture to the Pending list, to avoid reallocation of the thread-safe StreamingTextures array.
	check(Texture->StreamingIndex == INDEX_NONE);
	Texture->StreamingIndex = PendingStreamingTextures.Add(Texture);

	// Notify that this texture ptr is valid.
	ReferencedTextures.Add(Texture);
}

/**
 * Removes a texture from the streaming manager.
 */
void FStreamingManagerTexture::RemoveStreamingTexture( UTexture2D* Texture )
{
	const int32	TextureIndex = Texture->StreamingIndex;

	// Remove it from the Pending list if it is there.
	if (PendingStreamingTextures.IsValidIndex(TextureIndex) && PendingStreamingTextures[TextureIndex] == Texture)
	{
		PendingStreamingTextures[TextureIndex] = nullptr;
	}
	else if (StreamingTextures.IsValidIndex(TextureIndex) && StreamingTextures[TextureIndex].Texture == Texture)
	{
		StreamingTextures[TextureIndex].Texture = nullptr;
		RemovedTextureIndices.Add(TextureIndex);
	}

	Texture->StreamingIndex = INDEX_NONE;

	// Remove reference to this texture.
	ReferencedTextures.Remove(Texture);
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
		NotifyPrimitiveDetached( Primitive );
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
		FRemovedTextureArray RemovedTextures;
		DynamicComponentManager.Add(Primitive, DynamicType, RemovedTextures);
		SetTexturesRemovedTimestamp(RemovedTextures);
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
		FRemovedTextureArray RemovedTextures;

		for (FLevelTextureManager& LevelManager: LevelTextureManagers)
		{
			LevelManager.RemoveComponentReferences(Primitive);
		}

		DynamicComponentManager.Remove(Primitive, RemovedTextures);
		SetTexturesRemovedTimestamp(RemovedTextures);
	}
}

/**
* Mark the textures with a timestamp. They're about to lose their location-based heuristic and we don't want them to
* start using LastRenderTime heuristic for a few seconds until they are garbage collected!
*
* @param RemovedTextures	List of removed textures.
*/
void FStreamingManagerTexture::SetTexturesRemovedTimestamp(const FRemovedTextureArray& RemovedTextures)
{
	const double CurrentTime = FApp::GetCurrentTime();
	for ( int32 TextureIndex=0; TextureIndex < RemovedTextures.Num(); ++TextureIndex )
	{
		// When clearing references to textures, those textures could be already deleted.
		// This happens because we don't clear texture references in RemoveStreamingTexture.
		const UTexture2D* Texture = RemovedTextures[TextureIndex];
		if (!ReferencedTextures.Contains(Texture)) continue;

		FStreamingTexture* StreamingTexture = GetStreamingTexture(Texture);
		if ( StreamingTexture )
		{
			StreamingTexture->InstanceRemovedTimestamp = CurrentTime;
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
	FRemovedTextureArray RemovedTextures;
	DynamicComponentManager.Update(Primitive, RemovedTextures);
	SetTexturesRemovedTimestamp(RemovedTextures);
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
	const double CurrentTime = FApp::GetCurrentTime();
	if ( Primitive  && Primitive->IsRegistered() )
	{
		FStreamingTextureLevelContext LevelContext;
		TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
		Primitive->GetStreamingTextureInfoWithNULLRemoval( LevelContext, TextureInstanceInfos );

		for ( const FStreamingTexturePrimitiveInfo& Info :  TextureInstanceInfos )
		{
			FStreamingTexture* StreamingTexture = GetStreamingTexture( Info.Texture );
			if ( StreamingTexture )
			{
				// Note: Doesn't have to be cycle-perfect for thread safety.
				StreamingTexture->LastRenderTimeRefCount++;
				StreamingTexture->LastRenderTimeRefCountTimestamp = CurrentTime;
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
	const double CurrentTime = FApp::GetCurrentTime();
	if ( Primitive )
	{
		FStreamingTextureLevelContext LevelContext;
		TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
		Primitive->GetStreamingTextureInfoWithNULLRemoval( LevelContext, TextureInstanceInfos );

		for ( const FStreamingTexturePrimitiveInfo& Info :  TextureInstanceInfos )
		{
			FStreamingTexture* StreamingTexture = GetStreamingTexture( Info.Texture );
			if ( StreamingTexture && StreamingTexture->LastRenderTimeRefCount > 0 )
			{
				StreamingTexture->LastRenderTimeRefCount--;
				StreamingTexture->LastRenderTimeRefCountTimestamp = CurrentTime;
			}
		}
	}
}

/**
 * Returns the corresponding FStreamingTexture for a UTexture2D.
 */
FStreamingTexture* FStreamingManagerTexture::GetStreamingTexture( const UTexture2D* Texture2D )
{
	if (Texture2D && StreamingTextures.IsValidIndex(Texture2D->StreamingIndex))
	{
		return &StreamingTextures[Texture2D->StreamingIndex];
	}
	else
	{
		return nullptr;
	}
}

/**
 * Updates streaming for an individual texture, taking into account all view infos.
 *
 * @param Texture	Texture to update
 */
void FStreamingManagerTexture::UpdateIndividualTexture( UTexture2D* Texture )
{
	if (!IStreamingManager::Get().IsStreamingEnabled() || !Texture) return;

	AsyncWork->EnsureCompletion();

	FStreamingTexture* StreamingTexture = GetStreamingTexture(Texture);
	if (!StreamingTexture) return;

	StreamingTexture->UpdateDynamicData(NumStreamedMips, Settings);

	if (StreamingTexture->bForceFullyLoad) // Somewhat expected at this point.
	{
		StreamingTexture->WantedMips = StreamingTexture->BudgetedMips = StreamingTexture->MaxAllowedMips;
	}

	StreamingTexture->StreamWantedMips(*this);
}

/**
 * Not thread-safe: Updates a portion (as indicated by 'StageIndex') of all streaming textures,
 * allowing their streaming state to progress.
 *
 * @param Context			Context for the current stage (frame)
 * @param StageIndex		Current stage index
 * @param NumUpdateStages	Number of texture update stages
 */
void FStreamingManagerTexture::UpdateStreamingTextures( int32 StageIndex, int32 NumUpdateStages )
{
	if ( StageIndex == 0 )
	{
		CurrentUpdateStreamingTextureIndex = 0;
		InflightTextures.Reset();
	}

	int32 StartIndex = CurrentUpdateStreamingTextureIndex;
	int32 EndIndex = StreamingTextures.Num() * (StageIndex + 1) / NumUpdateStages;
	for ( int32 Index=StartIndex; Index < EndIndex; ++Index )
	{
		FStreamingTexture& StreamingTexture = StreamingTextures[Index];
		FPlatformMisc::Prefetch( &StreamingTexture + 1 );

		// Is this texture marked for removal? Will get cleanup once the async task is done.
		if (!StreamingTexture.Texture) continue;

		STAT(int32 PreviousResidentMips = StreamingTexture.ResidentMips;)

		StreamingTexture.UpdateDynamicData(NumStreamedMips, Settings);

		// Make a list of each texture that can potentially require additional UpdateStreamingStatus
		if (StreamingTexture.bInFlight)
		{
			InflightTextures.Add(Index);
		}

#if STATS
		if (StreamingTexture.ResidentMips > PreviousResidentMips)
		{
			GatheredStats.MipIOBandwidth += StreamingTexture.GetSize(StreamingTexture.ResidentMips) - StreamingTexture.GetSize(PreviousResidentMips);
		}
#endif
	}
	CurrentUpdateStreamingTextureIndex = EndIndex;
}



/**
 * Stream textures in/out, based on the priorities calculated by the async work.
 * @param bProcessEverything	Whether we're processing all textures in one go
 */
void FStreamingManagerTexture::StreamTextures( bool bProcessEverything )
{
	if (!bPauseTextureStreaming || bProcessEverything)
	{
		FAsyncTextureStreamingTask& AsyncTask = AsyncWork->GetTask();

		for (int32 TextureIndex : AsyncTask.GetCancelationRequests())
		{
			check(StreamingTextures.IsValidIndex(TextureIndex));
			StreamingTextures[TextureIndex].CancelPendingMipChangeRequest();
		}

		for (int32 TextureIndex : AsyncTask.GetLoadRequests())
		{
			check(StreamingTextures.IsValidIndex(TextureIndex));
			StreamingTextures[TextureIndex].StreamWantedMips(*this);
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

void FStreamingManagerTexture::SetLastUpdateTime()
{
	// Update the last update time.
	if (!GIsEditor)
	{
		for (int32 LevelIndex = 0; LevelIndex < LevelTextureManagers.Num(); ++LevelIndex)
		{
			// Update last update time only if there is a reasonable threshold to define visibility.
			float WorldTime = LevelTextureManagers[LevelIndex].GetWorldTime();
			if (WorldTime > 0)
			{
				LastWorldUpdateTime = WorldTime - .5f;
				break;
			}
		}
	}
}

void FStreamingManagerTexture::UpdateStats()
{
	float DeltaStatTime = (float)(GatheredStats.Timestamp - DisplayedStats.Timestamp);
	if (DeltaStatTime > SMALL_NUMBER)
	{
		GatheredStats.MipIOBandwidth = DeltaStatTime > SMALL_NUMBER ? GatheredStats.MipIOBandwidth / DeltaStatTime : 0;
	}
	DisplayedStats = GatheredStats;
	GatheredStats.MipIOBandwidth = 0;
	MemoryOverBudget = DisplayedStats.OverBudget;
	MaxEverRequired = FMath::Max<int64>(MaxEverRequired, DisplayedStats.RequiredPool);
}

void FStreamingManagerTexture::LogViewLocationChange()
{
#if STREAMING_LOG_VIEWCHANGES
	static bool bWasLocationOveridden = false;
	bool bIsLocationOverridden = false;
	for ( int32 ViewIndex=0; ViewIndex < CurrentViewInfos.Num(); ++ViewIndex )
	{
		FStreamingViewInfo& ViewInfo = CurrentViewInfos[ViewIndex];
		if ( ViewInfo.bOverrideLocation )
		{
			bIsLocationOverridden = true;
			break;
		}
	}
	if ( bIsLocationOverridden != bWasLocationOveridden )
	{
		UE_LOG(LogContentStreaming, Log, TEXT("Texture streaming view location is now %s."), bIsLocationOverridden ? TEXT("OVERRIDDEN") : TEXT("normal") );
		bWasLocationOveridden = bIsLocationOverridden;
	}
#endif
}

/**
 * Main function for the texture streaming system, based on texture priorities and asynchronous processing.
 * Updates streaming, taking into account all view infos.
 *
 * @param DeltaTime				Time since last call in seconds
 * @param bProcessEverything	[opt] If true, process all resources with no throttling limits
 */
static TAutoConsoleVariable<int32> CVarFramesForFullUpdate(
	TEXT("r.Streaming.FramesForFullUpdate"),
	5,
	TEXT("Texture streaming is time sliced per frame. This values gives the number of frames to visit all textures."));

void FStreamingManagerTexture::UpdateResourceStreaming( float DeltaTime, bool bProcessEverything/*=false*/ )
{
	SCOPE_CYCLE_COUNTER(STAT_GameThreadUpdateTime);

	LogViewLocationChange();
	STAT(DisplayedStats.Apply();)

	if (NumTextureProcessingStages <= 0 || bProcessEverything)
	{
		if (!AsyncWork->IsDone())
		{	// Is the AsyncWork is running for some reason? (E.g. we reset the system by simply setting ProcessingStage to 0.)
			AsyncWork->EnsureCompletion();
		}

		ProcessingStage = 0;
		NumTextureProcessingStages =  FMath::Max<int32>(CVarFramesForFullUpdate.GetValueOnGameThread(), 0);

		// Update Thread Data
		CheckUserSettings();
		SetLastUpdateTime();
		UpdateStreamingTextures(0, 1);
		IncrementalUpdate(1.f);
		UpdateThreadData(bProcessEverything);
		AsyncWork->StartSynchronousTask();
		StreamTextures(bProcessEverything);

		STAT(GatheredStats.SetupAsyncTaskCycles = 0);
		STAT(GatheredStats.UpdateStreamingDataCycles = 0);
		STAT(GatheredStats.StreamTexturesCycles = 0);
		STAT(UpdateStats();)
	}
	else if (ProcessingStage == 0)
	{
		STAT(GatheredStats.SetupAsyncTaskCycles = -(int32)FPlatformTime::Cycles();)

		NumTextureProcessingStages =  FMath::Max<int32>(CVarFramesForFullUpdate.GetValueOnGameThread(), 0);

		if (!AsyncWork->IsDone())
		{	// Is the AsyncWork is running for some reason? (E.g. we reset the system by simply setting ProcessingStage to 0.)
			AsyncWork->EnsureCompletion();
		}

		CheckUserSettings();
		UpdateThreadData(bProcessEverything);
		AsyncWork->StartBackgroundTask();
		++ProcessingStage;

		STAT(GatheredStats.SetupAsyncTaskCycles += FPlatformTime::Cycles();)
	}
	else if (ProcessingStage <= NumTextureProcessingStages)
	{
		STAT(int32 StartTime = (int32)FPlatformTime::Cycles();)

		if (ProcessingStage == 1)
		{
			SetLastUpdateTime();
		}

		UpdateStreamingTextures(ProcessingStage - 1, NumTextureProcessingStages);
		IncrementalUpdate(1.f / (float)FMath::Max(NumTextureProcessingStages - 1, 1)); // -1 since we don't want to do anything at stage 0.)
		++ProcessingStage;

		STAT(GatheredStats.UpdateStreamingDataCycles = FMath::Max<uint32>(ProcessingStage > 2 ? GatheredStats.UpdateStreamingDataCycles : 0, FPlatformTime::Cycles() - StartTime);)
	}
	else if (AsyncWork->IsDone())
	{
		STAT(GatheredStats.StreamTexturesCycles = -(int32)FPlatformTime::Cycles();)

		// Since this step is lightweight, tick each texture inflight here, to accelerate the state changes.
		for (int32 TextureIndex : InflightTextures)
		{
			StreamingTextures[TextureIndex].UpdateStreamingStatus();
		}

		StreamTextures(bProcessEverything);
		ProcessingStage = 0;

		STAT(GatheredStats.StreamTexturesCycles += FPlatformTime::Cycles();)
		STAT(UpdateStats();)
	}
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

	while (true) 
	{
		int32 NumOfInFlights = 0;

		for (FStreamingTexture& StreamingTexture : StreamingTextures)
		{
			StreamingTexture.UpdateStreamingStatus();
			if (StreamingTexture.bInFlight)
			{
				++NumOfInFlights;
			}
		}

		if (NumOfInFlights && (TimeLimit == 0 || (float)(FPlatformTime::Seconds() - StartTime) < TimeLimit))
		{
			FlushRenderingCommands();
			FPlatformProcess::Sleep( 0.010f );
		}
		else
		{
			if (bLogResults)
			{
				UE_LOG(LogContentStreaming, Log, TEXT("Blocking on texture streaming: %.1f ms (%d still in flight)"), (float)(FPlatformTime::Seconds() - StartTime) * 1000, NumOfInFlights );

			}
			return NumOfInFlights;
		}
	}
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

#if !UE_BUILD_SHIPPING

bool FStreamingManagerTexture::HandleListStreamingTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Make sure the async task is idle (also implies Update_Async is finished and that the distances are valid).
	AsyncWork->EnsureCompletion();

	// Sort texture by names so that the state can be compared between runs.
	TMap<FString, int32> SortedTextures;
	for ( int32 TextureIndex=0; TextureIndex < StreamingTextures.Num(); ++TextureIndex )
	{
		const FStreamingTexture& StreamingTexture = StreamingTextures[TextureIndex];
		if (!StreamingTexture.Texture) continue;

		SortedTextures.Add(StreamingTexture.Texture->GetFullName(), TextureIndex);
	}

	SortedTextures.KeySort(TLess<FString>());

	for (TMap<FString, int32>::TConstIterator It(SortedTextures); It; ++It)
	{
		const FStreamingTexture& StreamingTexture = StreamingTextures[It.Value()];
		const UTexture2D* Texture2D = StreamingTexture.Texture;
		UE_LOG(LogContentStreaming, Log,  TEXT("Texture [%d] : %s"), It.Value(), *Texture2D->GetFullName() );

		int32 CurrentMipIndex = FMath::Max(Texture2D->GetNumMips() - StreamingTexture.ResidentMips, 0);
		int32 WantedMipIndex = FMath::Max(Texture2D->GetNumMips() - StreamingTexture.GetPerfectWantedMips(), 0);
		int32 MaxAllowedMipIndex = FMath::Max(Texture2D->GetNumMips() - StreamingTexture.MaxAllowedMips, 0);
		const TIndirectArray<struct FTexture2DMipMap>& Mips = Texture2D->PlatformData->Mips;

		if (StreamingTexture.LastRenderTime != MAX_FLT)
		{
			UE_LOG(LogContentStreaming, Log,  TEXT("    Current=%dx%d Wanted=%dx%d MaxAllowed=%dx%d LastRenderTime=%.3f BudgetBias=%d Group=%s"), 
				Mips[CurrentMipIndex].SizeX, Mips[CurrentMipIndex].SizeY, 
				Mips[WantedMipIndex].SizeX, Mips[WantedMipIndex].SizeY, 
				Mips[MaxAllowedMipIndex].SizeX, Mips[MaxAllowedMipIndex].SizeY,
				StreamingTexture.LastRenderTime,
				StreamingTexture.BudgetMipBias,
				UTexture::GetTextureGroupString(StreamingTexture.LODGroup));
		}
		else
		{
			UE_LOG(LogContentStreaming, Log,  TEXT("    Current=%dx%d Wanted=%dx%d MaxAllowed=%dx%d BudgetBias=%d Group=%s"), 
				Mips[CurrentMipIndex].SizeX, Mips[CurrentMipIndex].SizeY, 
				Mips[WantedMipIndex].SizeX, Mips[WantedMipIndex].SizeY, 
				Mips[MaxAllowedMipIndex].SizeX, Mips[MaxAllowedMipIndex].SizeY,
				StreamingTexture.BudgetMipBias,
				UTexture::GetTextureGroupString(StreamingTexture.LODGroup));
		}

	}
	return true;
}

bool FStreamingManagerTexture::HandleResetMaxEverRequiredTexturesCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	Ar.Logf(TEXT("OldMax: %u MaxEverRequired Reset."), MaxEverRequired);
	ResetMaxEverRequired();	
	return true;
}

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

FORCEINLINE float SqrtKeepMax(float V)
{
	return V == FLT_MAX ? FLT_MAX : FMath::Sqrt(V);
}

bool FStreamingManagerTexture::HandleDebugTrackedTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
#if ENABLE_TEXTURE_TRACKING
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
								LastEvent->DynamicWantedMips.ComputeMip(&StreamingTexture, MipBias, false),
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
							float MaxSize = 0;
							float MaxSize_VisibleOnly = 0;
							FFloatMipLevel HandlerWantedMips = TextureStreamingHandler->GetWantedSize(*this, StreamingTexture, HandlerDistance);
							Ar.Logf(
								TEXT("    Handler %s: WantedMips: %d, PerfectWantedMips: %d, Distance: %f"),
								TextureStreamingHandler->HandlerName,
								HandlerWantedMips.ComputeMip(&StreamingTexture, MipBias, false),
								HandlerWantedMips.ComputeMip(&StreamingTexture, MipBias, true),
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
											TEXT("    Instance: %f,%f,%f Radius: %f Range: [%f, %f] TexelFactor: %f"),
											TextureInstance.BoundsOriginX[i],
											TextureInstance.BoundsOriginY[i],
											TextureInstance.BoundsOriginZ[i],
											TextureInstance.BoundingSphereRadius[i],
											FMath::Sqrt(TextureInstance.MinDistanceSq[i]),
											SqrtKeepMax(TextureInstance.MaxDistanceSq[i]),
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
#endif // ENABLE_TEXTURE_TRACKING

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
		Ar.Logf( TEXT("Tried to stream out %ld MB of texture data: %s"), FreeMB, bSucceeded ? TEXT("Succeeded") : TEXT("Failed") );
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
	AsyncWork->EnsureCompletion();

	uint32 MemSize = sizeof(FStreamingManagerTexture);
	MemSize += StreamingTextures.GetAllocatedSize();
	MemSize += DynamicComponentManager.GetAllocatedSize();
	MemSize += PendingStreamingTextures.GetAllocatedSize() + RemovedTextureIndices.GetAllocatedSize();
	MemSize += LevelTextureManagers.GetAllocatedSize();
	MemSize += AsyncWork->GetTask().StreamingData.GetAllocatedSize();

	for (const FLevelTextureManager& LevelManager : LevelTextureManagers)
	{
		MemSize += LevelManager.GetAllocatedSize();
	}

	Ar.Logf(TEXT("StreamingManagerTexture: %.2f KB used"), MemSize / 1024.0f);

	return true;
}

bool FStreamingManagerTexture::HandleTextureGroupsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	bDetailedDumpTextureGroupStats = FParse::Param(Cmd, TEXT("Detailed"));
	bTriggerDumpTextureGroupStats = true;
	return true;
}

bool FStreamingManagerTexture::HandleInvestigateTextureCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	FString InvestigateTextureName(FParse::Token(Cmd, 0));
	if ( InvestigateTextureName.Len() )
	{
		// Make sure the async task is idle (also implies Update_Async is finished and that the distances are valid).
		AsyncWork->EnsureCompletion();
		FAsyncTextureStreamingData& StreamingData = AsyncWork->GetTask().StreamingData;
		StreamingData.UpdateBoundSizes_Async(Settings);

		for ( int32 TextureIndex=0; TextureIndex < StreamingTextures.Num(); ++TextureIndex )
		{
			FStreamingTexture& StreamingTexture = StreamingTextures[TextureIndex];
			FString TextureName = StreamingTexture.Texture->GetFullName();
			if (TextureName.Contains(InvestigateTextureName))
			{
				UTexture2D* Texture2D = StreamingTexture.Texture;
				int32 CurrentMipIndex = FMath::Max(Texture2D->GetNumMips() - StreamingTexture.ResidentMips, 0);
				int32 WantedMipIndex = FMath::Max(Texture2D->GetNumMips() - StreamingTexture.GetPerfectWantedMips(), 0);
				int32 MaxMipIndex = FMath::Max(Texture2D->GetNumMips() - StreamingTexture.MaxAllowedMips, 0);

				UE_LOG(LogContentStreaming, Log,  TEXT("Texture: %s"), *TextureName );
				UE_LOG(LogContentStreaming, Log,  TEXT("  Texture group:   %s"), UTexture::GetTextureGroupString(StreamingTexture.LODGroup) );

				if ( StreamingTexture.bForceFullyLoadHeuristic )
				{
					UE_LOG(LogContentStreaming, Log,  TEXT("  Force all mips: bForceFullyLoad") );
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
					float TimeLeft = (float)(Texture2D->ForceMipLevelsToBeResidentTimestamp - FApp::GetCurrentTime());
					UE_LOG(LogContentStreaming, Log,  TEXT("  Force all mips:  %.1f seconds left"), FMath::Max(TimeLeft,0.0f) );
				}
				else if ( StreamingTexture.MipCount == 1 )
				{
					UE_LOG(LogContentStreaming, Log,  TEXT("  Force all mips:  No mip-maps") );
				}
				UE_LOG(LogContentStreaming, Log,  TEXT("  Current size:    %dx%d"), Texture2D->PlatformData->Mips[CurrentMipIndex].SizeX, Texture2D->PlatformData->Mips[CurrentMipIndex].SizeY );
				UE_LOG(LogContentStreaming, Log,  TEXT("  Wanted size:     %dx%d"), Texture2D->PlatformData->Mips[WantedMipIndex].SizeX, Texture2D->PlatformData->Mips[WantedMipIndex].SizeY );
				UE_LOG(LogContentStreaming, Log,  TEXT("  MaxAllowed size: %dx%d"), Texture2D->PlatformData->Mips[MaxMipIndex].SizeX, Texture2D->PlatformData->Mips[MaxMipIndex].SizeY );
				UE_LOG(LogContentStreaming, Log,  TEXT("  LoadOrder Priority: %d"), StreamingTexture.LoadOrderPriority );
				UE_LOG(LogContentStreaming, Log,  TEXT("  Retention Priority: %d"), StreamingTexture.RetentionPriority );
				UE_LOG(LogContentStreaming, Log,  TEXT("  Boost factor:    %.1f"), StreamingTexture.BoostFactor );
				UE_LOG(LogContentStreaming, Log,  TEXT("  Allowed mips:    %d-%d"), StreamingTexture.MinAllowedMips, StreamingTexture.MaxAllowedMips );
				UE_LOG(LogContentStreaming, Log,  TEXT("  Mip bias : Texture=%d Global=%.1f"), StreamingTexture.BudgetMipBias, Settings.GlobalMipBias() );

				if (InWorld && !GIsEditor)
				{
					UE_LOG(LogContentStreaming, Log,  TEXT("  Time: World=%.3f LastUpdate=%.3f "), InWorld->GetTimeSeconds(), LastWorldUpdateTime);
				}

				for( int32 ViewIndex=0; ViewIndex < StreamingData.GetViewInfos().Num(); ViewIndex++ )
				{
					// Calculate distance of viewer to bounding sphere.
					const FStreamingViewInfo& ViewInfo = StreamingData.GetViewInfos()[ViewIndex];
					UE_LOG(LogContentStreaming, Log,  TEXT("  View%d: Position=(%s) ScreenSize=%f Boost=%f"), ViewIndex, *ViewInfo.ViewOrigin.ToString(), ViewInfo.ScreenSize, ViewInfo.BoostFactor);
				}

				StreamingData.UpdatePerfectWantedMips_Async(StreamingTexture, Settings, true);
			}
		}
	}
	else
	{
		Ar.Logf( TEXT("Usage: InvestigateTexture <name>") );
	}
	return true;
}
#endif // !UE_BUILD_SHIPPING
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
#endif
#if !UE_BUILD_SHIPPING
	if (FParse::Command(&Cmd, TEXT("ResetMaxEverRequiredTextures")))
	{
		return HandleResetMaxEverRequiredTexturesCommand(Cmd, Ar);
	}
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
		return HandleInvestigateTextureCommand( Cmd, Ar, InWorld );
	}
#endif // !UE_BUILD_SHIPPING

	return false;
}

void FStreamingManagerTexture::DumpTextureGroupStats( bool bDetailedStats )
{
	bTriggerDumpTextureGroupStats = false;
#if !UE_BUILD_SHIPPING
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
		UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
		FTextureGroupStats& Stat = TextureGroupStats[Texture->LODGroup];
		FTextureGroupStats& Waste = TextureGroupWaste[Texture->LODGroup];
		FStreamingTexture* StreamingTexture = GetStreamingTexture(Texture2D);
		uint32 TextureAlign = 0;
		if ( StreamingTexture )
		{
			Stat.NumTextures++;
			Stat.CurrentTextureSize += StreamingTexture->GetSize( StreamingTexture->ResidentMips );
			Stat.WantedTextureSize += StreamingTexture->GetSize( StreamingTexture->WantedMips );
			Stat.MaxTextureSize += StreamingTexture->GetSize( StreamingTexture->MaxAllowedMips );
			
			int64 WasteCurrent = StreamingTexture->GetSize( StreamingTexture->ResidentMips ) - RHICalcTexture2DPlatformSize(Texture2D->GetSizeX(), Texture2D->GetSizeY(), Texture2D->GetPixelFormat(), StreamingTexture->ResidentMips, 1, 0, TextureAlign);			

			int64 WasteWanted = StreamingTexture->GetSize( StreamingTexture->WantedMips ) - RHICalcTexture2DPlatformSize(Texture2D->GetSizeX(), Texture2D->GetSizeY(), Texture2D->GetPixelFormat(), StreamingTexture->WantedMips, 1, 0, TextureAlign);			

			int64 WasteMaxSize = StreamingTexture->GetSize( StreamingTexture->MaxAllowedMips ) - RHICalcTexture2DPlatformSize(Texture2D->GetSizeX(), Texture2D->GetSizeY(), Texture2D->GetPixelFormat(), StreamingTexture->MaxAllowedMips, 1, 0, TextureAlign);			

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
#endif // !UE_BUILD_SHIPPING
}
