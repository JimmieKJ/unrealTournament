// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ContentStreaming.cpp: Implementation of content streaming classes.
=============================================================================*/

#include "EnginePrivate.h"
#include "GenericPlatformMemoryPoolStats.h"
#include "Streaming/AsyncTextureStreaming.h"
#include "AudioStreaming.h"
#include "Engine/LightMapTexture2D.h"
#include "Engine/ShadowMapTexture2D.h"

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

/**
 * Helper function to flush resource streaming from within Core project.
 */
void FlushResourceStreaming()
{
	RETURN_IF_EXIT_REQUESTED;
	IStreamingManager::Get().BlockTillAllRequestsFinished();
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
	,	BoostFactor(1.0f)
	{
	}
	FTrackedTextureEvent( const FString& InTextureNameString )
	:	NumResidentMips(0)
	,	NumRequestedMips(0)
	,	WantedMips(0)
	,	StreamingStatus(0)
	,	Timestamp(0.0f)
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
	/** Streaming status. */
	int32			StreamingStatus;
	/** Timestamp, in seconds from startup. */
	float			Timestamp;
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
					float BoostFactor		= 1.0f;
					if ( StreamingTexture )
					{
						WantedMips			= StreamingTexture->WantedMips;
						BoostFactor			= StreamingTexture->BoostFactor;
					}

					if ( LastEvent->NumResidentMips != Texture->ResidentMips ||
						 LastEvent->NumRequestedMips != Texture->RequestedMips ||
						 LastEvent->StreamingStatus != StreamingStatus ||
						 LastEvent->WantedMips != WantedMips ||
						 LastEvent->BoostFactor != BoostFactor ||
						 bIsDestroying )
					{
						GTrackedTextureEventIndex		= (GTrackedTextureEventIndex + 1) % NUM_TRACKEDTEXTUREEVENTS;
						FTrackedTextureEvent& NewEvent	= GTrackedTextureEvents[GTrackedTextureEventIndex];
						NewEvent.TextureName			= LastEvent->TextureName;
						NewEvent.NumResidentMips		= LastEvent->NumResidentMips	= Texture->ResidentMips;
						NewEvent.NumRequestedMips		= LastEvent->NumRequestedMips	= Texture->RequestedMips;
						NewEvent.WantedMips				= LastEvent->WantedMips			= WantedMips;
						NewEvent.StreamingStatus		= LastEvent->StreamingStatus	= StreamingStatus;
						NewEvent.Timestamp				= LastEvent->Timestamp			= float(FPlatformTime::Seconds() - GStartTime);
						NewEvent.BoostFactor			= LastEvent->BoostFactor		= BoostFactor;
						UE_LOG(LogContentStreaming, Log, TEXT("Texture: \"%s\", ResidentMips: %d/%d, RequestedMips: %d, WantedMips: %d, StreamingStatus: %d, Boost: %.1f (%s)"),
							TextureName, LastEvent->NumResidentMips, Texture->GetNumMips(), bIsDestroying ? 0 : LastEvent->NumRequestedMips, LastEvent->WantedMips, 
							LastEvent->StreamingStatus, BoostFactor, bIsDestroying ? TEXT("DESTROYED") : TEXT("updated") );
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
				if ( (ViewInfo.ViewOrigin - GPrevViewLocations(PrevView).ViewOrigin).SizeSquared() < 10000.0f )
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
				if ( (ViewInfo.ViewOrigin - GPrevViewLocations(PrevView).ViewOrigin).SizeSquared() < 10000.0f )
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
	int32 NumResourcesWantingStreaming = 0;

	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		NumResourcesWantingStreaming += StreamingManager->GetNumWantingResources();
	}

	return NumResourcesWantingStreaming;
}

/**
 * Returns the current ID for GetNumWantingResources().
 * The ID is bumped every time NumWantingResources is updated by the streaming system (every few frames).
 * Can be used to verify that any changes have been fully examined, by comparing current ID with
 * what it was when the changes were made.
 */
int32 FStreamingManagerCollection::GetNumWantingResourcesID() const
{
	int32 NumWantingResourcesStreamingCounter = MAX_int32;

	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		NumWantingResourcesStreamingCounter = FMath::Min( NumWantingResourcesStreamingCounter, StreamingManager->GetNumWantingResourcesID() );
	}

	return NumWantingResourcesStreamingCounter;
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

	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		StreamingManager->AddLevel( Level );
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

/** Called when an actor is spawned. */
void FStreamingManagerCollection::NotifyActorSpawned( AActor* Actor )
{
	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		StreamingManager->NotifyActorSpawned( Actor );
	}
}

/** Called when a spawned actor is destroyed. */
void FStreamingManagerCollection::NotifyActorDestroyed( AActor* Actor )
{
	// Route to streaming managers.
	for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
	{
		IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
		StreamingManager->NotifyActorDestroyed( Actor );
	}
}

/**
 * Called when a primitive is attached to an actor or another component.
 * Replaces previous info, if the primitive was already attached.
 *
 * @param InPrimitive	Newly attached dynamic/spawned primitive
 */
void FStreamingManagerCollection::NotifyPrimitiveAttached( const UPrimitiveComponent* Primitive, EDynamicPrimitiveType DynamicType )
{
	// Distance-based heuristics only handle mesh components
	if ( Primitive->IsA( UMeshComponent::StaticClass() ) )
	{
		for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
		{
			IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
			StreamingManager->NotifyPrimitiveAttached( Primitive, DynamicType );
		}
	}
}

/** Called when a primitive is detached from an actor or another component. */
void FStreamingManagerCollection::NotifyPrimitiveDetached( const UPrimitiveComponent* Primitive )
{
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
}

/**
 * Called when a primitive has had its textured changed.
 * Only affects primitives that were already attached.
 * Replaces previous info.
 */
void FStreamingManagerCollection::NotifyPrimitiveUpdated( const UPrimitiveComponent* Primitive )
{
	if ( Primitive->IsA( UMeshComponent::StaticClass() ) )
	{
		// Route to streaming managers.
		for( int32 ManagerIndex=0; ManagerIndex<StreamingManagers.Num(); ManagerIndex++ )
		{
			IStreamingManager* StreamingManager = StreamingManagers[ManagerIndex];
			StreamingManager->NotifyPrimitiveUpdated( Primitive );
		}
	}
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
			TextureStreamingManager->BlockTillAllRequestsFinished();

			RemoveStreamingManager(TextureStreamingManager);
			delete TextureStreamingManager;
			TextureStreamingManager = nullptr;

			for( TObjectIterator<UTexture2D>It; It; ++It )
			{
				if( It->bIsStreamable )
				{
					It->UpdateResource();
				}
			}
		}
	}
}

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

	UE_LOG(LogContentStreaming, Log, TEXT("Streaming out texture memory! Saved %.2f MB."), float(SavedMemory)/1024.0f/1024.0f);

	// Return the result.
	*bSucceeded = SavedMemory >= RequiredMemorySize;
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
	if (Ar.UE4Ver() >= VER_UE4_STREAMABLE_TEXTURE_AABB)
	{
		Ar << TextureInstance.Bounds;
	}
	else if (Ar.IsLoading())
	{
		FSphere BoundingSphere;
		Ar << BoundingSphere;
		TextureInstance.Bounds = FBoxSphereBounds(BoundingSphere);
	}

	if (Ar.UE4Ver() >= VER_UE4_STREAMABLE_TEXTURE_MIN_MAX_DISTANCE)
	{
		Ar << TextureInstance.MinDistance;
		Ar << TextureInstance.MaxDistance;
	}
	else if (Ar.IsLoading())
	{
		TextureInstance.MinDistance = 0;
		TextureInstance.MaxDistance = FLT_MAX;
	}

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
	FStreamableTextureInstance& Super = TextureInstance;
	Ar << Super;

	Ar << TextureInstance.Texture;
	Ar << TextureInstance.bAttached;
	Ar << TextureInstance.OriginalRadius;
	return Ar;
}
