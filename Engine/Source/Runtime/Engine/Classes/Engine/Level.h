// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/World.h"
#include "Level.generated.h"

class ALevelBounds;
class UTexture2D;
class UNavigationDataChunk;
class AInstancedFoliageActor;

/**
 * Structure containing all information needed for determining the screen space
 * size of an object/ texture instance.
 */
USTRUCT()
struct ENGINE_API FStreamableTextureInstance
{
	GENERATED_USTRUCT_BODY()

	/** Bounding sphere/ box of object */
	FSphere BoundingSphere;
	/** Object (and bounding sphere) specific texel scale factor  */
	float	TexelFactor;

	/**
	 * FStreamableTextureInstance serialize operator.
	 *
	 * @param	Ar					Archive to to serialize object to/ from
	 * @param	TextureInstance		Object to serialize
	 * @return	Returns the archive passed in
	 */
	friend FArchive& operator<<( FArchive& Ar, FStreamableTextureInstance& TextureInstance );
};

/**
 * Serialized ULevel information about dynamic texture instances
 */
USTRUCT()
struct ENGINE_API FDynamicTextureInstance : public FStreamableTextureInstance
{
	GENERATED_USTRUCT_BODY()

	/** Texture that is used by a dynamic UPrimitiveComponent. */
	UPROPERTY()
	UTexture2D*					Texture;

	/** Whether the primitive that uses this texture is attached to the scene or not. */
	UPROPERTY()
	bool						bAttached;
	
	/** Original bounding sphere radius, at the time the TexelFactor was calculated originally. */
	UPROPERTY()
	float						OriginalRadius;

	/**
	 * FDynamicTextureInstance serialize operator.
	 *
	 * @param	Ar					Archive to to serialize object to/ from
	 * @param	TextureInstance		Object to serialize
	 * @return	Returns the archive passed in
	 */
	friend FArchive& operator<<( FArchive& Ar, FDynamicTextureInstance& TextureInstance );
};

/** Struct that holds on to information about Actors that wish to be auto enabled for input before the player controller has been created */
struct FPendingAutoReceiveInputActor
{
	TWeakObjectPtr<AActor> Actor;
	int32 PlayerIndex;

	FPendingAutoReceiveInputActor(AActor* InActor, const int32 InPlayerIndex)
		: Actor(InActor)
		, PlayerIndex(InPlayerIndex)
	{
	}
};

/** A precomputed visibility cell, whose data is stored in FCompressedVisibilityChunk. */
class FPrecomputedVisibilityCell
{
public:

	/** World space min of the cell. */
	FVector Min;

	/** Index into FPrecomputedVisibilityBucket::CellDataChunks of this cell's data. */
	uint16 ChunkIndex;

	/** Index into the decompressed chunk data of this cell's visibility data. */
	uint16 DataOffset;

	friend FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityCell& D )
	{
		Ar << D.Min << D.ChunkIndex << D.DataOffset;
		return Ar;
	}
};

/** A chunk of compressed visibility data from multiple FPrecomputedVisibilityCell's. */
class FCompressedVisibilityChunk
{
public:
	/** Whether the chunk is compressed. */
	bool bCompressed;

	/** Size of the uncompressed chunk. */
	int32 UncompressedSize;

	/** Compressed visibility data if bCompressed is true. */
	TArray<uint8> Data;

	friend FArchive& operator<<( FArchive& Ar, FCompressedVisibilityChunk& D )
	{
		Ar << D.bCompressed << D.UncompressedSize << D.Data;
		return Ar;
	}
};

/** A bucket of visibility cells that have the same spatial hash. */
class FPrecomputedVisibilityBucket
{
public:
	/** Size in bytes of the data of each cell. */
	int32 CellDataSize;

	/** Cells in this bucket. */
	TArray<FPrecomputedVisibilityCell> Cells;

	/** Data chunks corresponding to Cells. */
	TArray<FCompressedVisibilityChunk> CellDataChunks;

	friend FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityBucket& D )
	{
		Ar << D.CellDataSize << D.Cells << D.CellDataChunks;
		return Ar;
	}
};

/** Handles operations on precomputed visibility data for a level. */
class FPrecomputedVisibilityHandler
{
public:

	FPrecomputedVisibilityHandler() :
		Id(NextId)
	{
		NextId++;
	}
	
	~FPrecomputedVisibilityHandler() 
	{ 
		UpdateVisibilityStats(false);
	}

	/** Updates visibility stats. */
	ENGINE_API void UpdateVisibilityStats(bool bAllocating) const;

	/** Sets this visibility handler to be actively used by the rendering scene. */
	ENGINE_API void UpdateScene(FSceneInterface* Scene) const;

	/** Invalidates the level's precomputed visibility and frees any memory used by the handler. */
	ENGINE_API void Invalidate(FSceneInterface* Scene);

	/** Shifts origin of precomputed visibility volume by specified offset */
	ENGINE_API void ApplyWorldOffset(const FVector& InOffset);

	/** @return the Id */
	int32 GetId() const { return Id; }

	friend FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityHandler& D );
	
private:

	/** World space origin of the cell grid. */
	FVector2D PrecomputedVisibilityCellBucketOriginXY;

	/** World space size of every cell in x and y. */
	float PrecomputedVisibilityCellSizeXY;

	/** World space height of every cell. */
	float PrecomputedVisibilityCellSizeZ;

	/** Number of cells in each bucket in x and y. */
	int32	PrecomputedVisibilityCellBucketSizeXY;

	/** Number of buckets in x and y. */
	int32	PrecomputedVisibilityNumCellBuckets;

	static int32 NextId;

	/** Id used by the renderer to know when cached visibility data is valid. */
	int32 Id;

	/** Visibility bucket data. */
	TArray<FPrecomputedVisibilityBucket> PrecomputedVisibilityCellBuckets;

	friend class FLightmassProcessor;
	friend class FSceneViewState;
};

/** Volume distance field generated by Lightmass, used by image based reflections for shadowing. */
class FPrecomputedVolumeDistanceField
{
public:

	/** Sets this volume distance field to be actively used by the rendering scene. */
	ENGINE_API void UpdateScene(FSceneInterface* Scene) const;

	/** Invalidates the level's volume distance field and frees any memory used by it. */
	ENGINE_API void Invalidate(FSceneInterface* Scene);

	friend FArchive& operator<<( FArchive& Ar, FPrecomputedVolumeDistanceField& D );

private:
	/** Largest world space distance stored in the volume. */
	float VolumeMaxDistance;
	/** World space bounding box of the volume. */
	FBox VolumeBox;
	/** Volume dimension X. */
	int32 VolumeSizeX;
	/** Volume dimension Y. */
	int32 VolumeSizeY;
	/** Volume dimension Z. */
	int32 VolumeSizeZ;
	/** Distance field data. */
	TArray<FColor> Data;

	friend class FScene;
	friend class FLightmassProcessor;
};

USTRUCT()
struct ENGINE_API FLevelSimplificationDetails
{
	GENERATED_USTRUCT_BODY()

	// Whether to create separate packages for each generated asset. All in map package otherwise
	UPROPERTY(Category=General, EditAnywhere)
	bool bCreatePackagePerAsset;

	// Percentage of details for static mesh proxy
	UPROPERTY(Category=StaticMesh, EditAnywhere, meta=(DisplayName="Static Mesh Details Percentage", ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))	
	float DetailsPercentage;

	// Whether to generate normal map for static mesh proxy
	UPROPERTY(Category=StaticMesh, EditAnywhere, meta=(DisplayName="Static Mesh Normal Map"))
	bool bGenerateMeshNormalMap;
	
	// Whether to generate metallic map for static mesh proxy
	UPROPERTY(Category=StaticMesh, EditAnywhere, meta=(DisplayName="Static Mesh Metallic Map"))
	bool bGenerateMeshMetallicMap;

	// Whether to generate roughness map for static mesh proxy
	UPROPERTY(Category=StaticMesh, EditAnywhere, meta=(DisplayName="Static Mesh Roughness Map"))
	bool bGenerateMeshRoughnessMap;
	
	// Whether to generate specular map for static mesh proxy
	UPROPERTY(Category=StaticMesh, EditAnywhere, meta=(DisplayName="Static Mesh Specular Map"))
	bool bGenerateMeshSpecularMap;

	UPROPERTY()
	bool bOverrideLandscapeExportLOD;

	// Landscape LOD to use for static mesh generation, when not specified 'Max LODLevel' from landscape actor will be used
	UPROPERTY(Category=Landscape, EditAnywhere, meta=(ClampMin = "0", ClampMax = "7", UIMin = "0", UIMax = "7", editcondition = "bOverrideLandscapeExportLOD"))
	int32 LandscapeExportLOD;

	// Whether to generate normal map for landscape static mesh
	UPROPERTY(Category=Landscape, EditAnywhere, meta=(DisplayName="Landscape Normal Map"))
	bool bGenerateLandscapeNormalMap;

	// Whether to generate metallic map for landscape static mesh
	UPROPERTY(Category=Landscape, EditAnywhere, meta=(DisplayName="Landscape Metallic Map"))
	bool bGenerateLandscapeMetallicMap;

	// Whether to generate roughness map for landscape static mesh
	UPROPERTY(Category=Landscape, EditAnywhere, meta=(DisplayName="Landscape Roughness Map"))
	bool bGenerateLandscapeRoughnessMap;
	
	// Whether to generate specular map for landscape static mesh
	UPROPERTY(Category=Landscape, EditAnywhere, meta=(DisplayName="Landscape Specular Map"))
	bool bGenerateLandscapeSpecularMap;
	
	// Whether to bake foliage into landscape static mesh texture
	UPROPERTY(Category=Landscape, EditAnywhere)
	bool bBakeFoliageToLandscape;

	// Whether to bake grass into landscape static mesh texture
	UPROPERTY(Category=Landscape, EditAnywhere)
	bool bBakeGrassToLandscape;

	FLevelSimplificationDetails();

	bool operator == (const FLevelSimplificationDetails& Other) const;
};

//
// The level object.  Contains the level's actor list, BSP information, and brush list.
// Every Level has a World as its Outer and can be used as the PersistentLevel, however,
// when a Level has been streamed in the OwningWorld represents the World that it is a part of.
//


/**
 * A Level is a collection of Actors (lights, volumes, mesh instances etc.).
 * Multiple Levels can be loaded and unloaded into the World to create a streaming experience.
 * 
 * @see https://docs.unrealengine.com/latest/INT/Engine/Levels
 * @see UActor
 */
UCLASS(MinimalAPI)
class ULevel : public UObject, public IInterface_AssetUserData
{
	GENERATED_BODY()

public:

	/** URL associated with this level. */
	FURL					URL;

	/** Array of all actors in this level, used by FActorIteratorBase and derived classes */
	TTransArray<AActor*> Actors;

	/** Set before calling LoadPackage for a streaming level to ensure that OwningWorld is correct on the Level */
	ENGINE_API static TMap<FName, TWeakObjectPtr<UWorld> > StreamedLevelsOwningWorld;
		
	/** 
	 * The World that has this level in its Levels array. 
	 * This is not the same as GetOuter(), because GetOuter() for a streaming level is a vestigial world that is not used. 
	 * It should not be accessed during BeginDestroy(), just like any other UObject references, since GC may occur in any order.
	 */
	UPROPERTY(transient)
	UWorld* OwningWorld;

	/** BSP UModel. */
	UPROPERTY()
	class UModel* Model;

	/** BSP Model components used for rendering. */
	UPROPERTY()
	TArray<class UModelComponent*> ModelComponents;

#if WITH_EDITORONLY_DATA
	/** Reference to the blueprint for level scripting */
	UPROPERTY(NonTransactional)
	class ULevelScriptBlueprint* LevelScriptBlueprint;
#endif //WITH_EDITORONLY_DATA

	/** The level scripting actor, created by instantiating the class from LevelScriptBlueprint.  This handles all level scripting */
	UPROPERTY(NonTransactional)
	class ALevelScriptActor* LevelScriptActor;

	/**
	 * Start and end of the navigation list for this level, used for quickly fixing up
	 * when streaming this level in/out. @TODO DEPRECATED - DELETE
	 */
	UPROPERTY()
	class ANavigationObjectBase *NavListStart;
	UPROPERTY()
	class ANavigationObjectBase	*NavListEnd;
	
	/** Navigation related data that can be stored per level */
	UPROPERTY()
	TArray<UNavigationDataChunk*> NavDataChunks;
	
	/** Total number of KB used for lightmap textures in the level. */
	UPROPERTY(VisibleAnywhere, Category=Level)
	float LightmapTotalSize;
	/** Total number of KB used for shadowmap textures in the level. */
	UPROPERTY(VisibleAnywhere, Category=Level)
	float ShadowmapTotalSize;

	/** threes of triangle vertices - AABB filtering friendly. Stored if there's a runtime need to rebuild navigation that accepts BSPs 
	 *	as well - it's a lot easier this way than retrieve this data at runtime */
	UPROPERTY()
	TArray<FVector> StaticNavigableGeometry;

	/** Static information used by texture streaming code, generated during PreSave									*/
	TMap<UTexture2D*,TArray<FStreamableTextureInstance> >	TextureToInstancesMap;

	/** Information about textures on dynamic primitives. Used by texture streaming code, generated during PreSave.		*/
	TMap<TWeakObjectPtr<UPrimitiveComponent>,TArray<FDynamicTextureInstance> >	DynamicTextureInstances;

	/** Set of textures used by PrimitiveComponents that have bForceMipStreaming checked. */
	TMap<UTexture2D*,bool>									ForceStreamTextures;

	/** Index into Actors array pointing to first net relevant actor. Used as an optimization for FActorIterator	*/
	int32											iFirstNetRelevantActor;

	/** Data structures for holding the tick functions **/
	class FTickTaskLevel*		TickTaskLevel;

	/** 
	* The precomputed light information for this level.  
	* The extra level of indirection is to allow forward declaring FPrecomputedLightVolume.
	*/
	class FPrecomputedLightVolume*				PrecomputedLightVolume;

	/** Contains precomputed visibility data for this level. */
	FPrecomputedVisibilityHandler				PrecomputedVisibilityHandler;

	/** Precomputed volume distance field for this level. */
	FPrecomputedVolumeDistanceField				PrecomputedVolumeDistanceField;

	/** Fence used to track when the rendering thread has finished referencing this ULevel's resources. */
	FRenderCommandFence							RemoveFromSceneFence;

	/** Whether components are currently registered or not. */
	uint32										bAreComponentsCurrentlyRegistered:1;

	/** Whether the geometry needs to be rebuilt for correct lighting */
	uint32										bGeometryDirtyForLighting:1;

	/** Has texture streaming been built */
	uint32										bTextureStreamingBuilt:1;

	/** Whether the level is currently visible/ associated with the world */
	UPROPERTY(transient)
	uint32										bIsVisible:1;
	
	/** Whether this level is locked; that is, its actors are read-only 
	 *	Used by WorldBrowser to lock a level when corresponding ULevelStreaming does not exist
	 */
	UPROPERTY()
	uint32 										bLocked:1;
	
	/** The below variables are used temporarily while making a level visible.				*/

	/** Whether we already moved actors.													*/
	uint32										bAlreadyMovedActors:1;
	/** Whether we already shift actors positions according to world composition.			*/
	uint32										bAlreadyShiftedActors:1;
	/** Whether we already updated components.												*/
	uint32										bAlreadyUpdatedComponents:1;
	/** Whether we already associated streamable resources.									*/
	uint32										bAlreadyAssociatedStreamableResources:1;
	/** Whether we already initialized network actors.											*/
	uint32										bAlreadyInitializedNetworkActors:1;
	/** Whether we already routed initialize on actors.										*/
	uint32										bAlreadyRoutedActorInitialize:1;
	/** Whether we already sorted the actor list.											*/
	uint32										bAlreadySortedActorList:1;
	/** Whether this level is in the process of being associated with its world				*/
	uint32										bIsAssociatingLevel:1;
	/** Whether this level should be fully added to the world before rendering his components	*/
	uint32										bRequireFullVisibilityToRender:1;
	/** Whether this level is specific to client, visibility state will not be replicated to server	*/
	uint32										bClientOnlyVisible:1;
	/** Whether this level was duplicated for PIE	*/
	uint32										bWasDuplicatedForPIE:1;
	/** Current index into actors array for updating components.							*/
	int32										CurrentActorIndexForUpdateComponents;

	/** Whether the level is currently pending being made visible.							*/
	bool HasVisibilityRequestPending() const
	{
		return (OwningWorld && this == OwningWorld->CurrentLevelPendingVisibility);
	}

	// Event on level transform changes
	DECLARE_MULTICAST_DELEGATE_OneParam(FLevelTransformEvent, const FTransform&);
	FLevelTransformEvent OnApplyLevelTransform;

#if WITH_EDITORONLY_DATA
	/** Level simplification settings for each LOD */
	UPROPERTY()
	FLevelSimplificationDetails LevelSimplification[WORLDTILE_LOD_MAX_INDEX];
#endif //WITH_EDITORONLY_DATA

	/** Actor which defines level logical bounding box				*/
	TWeakObjectPtr<ALevelBounds>				LevelBoundsActor;

	/** Cached pointer to Foliage actor		*/
	TWeakObjectPtr<AInstancedFoliageActor>		InstancedFoliageActor;

	/** Called when Level bounds actor has been updated */
	DECLARE_EVENT( ULevel, FLevelBoundsActorUpdatedEvent );
	FLevelBoundsActorUpdatedEvent& LevelBoundsActorUpdated() { return LevelBoundsActorUpdatedEvent; }
	/**	Broadcasts that Level bounds actor has been updated */ 
	void BroadcastLevelBoundsActorUpdated() { LevelBoundsActorUpdatedEvent.Broadcast(); }

	/** Marks level bounds as dirty so they will be recalculated  */
	ENGINE_API void MarkLevelBoundsDirty();

private:
	FLevelBoundsActorUpdatedEvent LevelBoundsActorUpdatedEvent; 

protected:
	/** Array of all MovieSceneBindings that are used in this level.  These store the relationship between
	    a MovieScene asset and possessed actors in this level. */
	UPROPERTY()
	TArray< class UObject* > MovieSceneBindingsArray;

	/** List of RuntimeMovieScenePlayers that are currently active in this level.  We'll keep references to these to keep
	    them around until they're no longer needed.  Also, we'll tick them every frame! */
	// @todo sequencer uobjects: Ideally this is using URuntimeMovieScenePlayer* and not UObject*, but there are DLL/interface issues with that
	UPROPERTY( transient )
	TArray< UObject* > ActiveRuntimeMovieScenePlayers;

	/** Array of user data stored with the asset */
	UPROPERTY()
	TArray<UAssetUserData*> AssetUserData;

private:
	// Actors awaiting input to be enabled once the appropriate PlayerController has been created
	TArray<FPendingAutoReceiveInputActor> PendingAutoReceiveInputActors;

public:
	/** Called when a level package has been dirtied. */
	ENGINE_API static FSimpleMulticastDelegate LevelDirtiedEvent;

	// Constructor.
	ENGINE_API void Initialize(const FURL& InURL);
	ULevel(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_HOT_RELOAD_CTORS
	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	ULevel(FVTableHelper& Helper)
		: Super(Helper)
		, Actors(this)
	{}
#endif // WITH_HOT_RELOAD_CTORS

	~ULevel();

	// Begin UObject interface.
	virtual void Serialize( FArchive& Ar ) override;
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void FinishDestroy() override;
	virtual UWorld* GetWorld() const override;

#if	WITH_EDITOR
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;	
#endif // WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PreSave() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End UObject interface.

	/**
	 * Adds a newly-created RuntimeMovieScenePlayer to this level.  The level assumes ownership of this object.
	 *
	 * @param	RuntimeMovieScenePlayer	The RuntimeMovieScenePlayer instance to add
	 */
	ENGINE_API void AddActiveRuntimeMovieScenePlayer( class UObject* RuntimeMovieScenePlayer );

	/**
	 * Called by the engine every frame to tick all actively-playing RuntimeMovieScenePlayers
	 *
	 * @param	DeltaSeconds	Time elapsed since last tick
	 */
	void TickRuntimeMovieScenePlayers( const float DeltaSeconds );

	/**
	 * Clears all components of actors associated with this level (aka in Actors array) and 
	 * also the BSP model components.
	 */
	ENGINE_API void ClearLevelComponents();

	/**
	 * Updates all components of actors associated with this level (aka in Actors array) and 
	 * creates the BSP model components.
	 * @param bRerunConstructionScripts	If we want to rerun construction scripts on actors in level
	 */
	ENGINE_API void UpdateLevelComponents(bool bRerunConstructionScripts);

	/**
	 * Incrementally updates all components of actors associated with this level.
	 *
	 * @param NumComponentsToUpdate		Number of components to update in this run, 0 for all
	 * @param bRerunConstructionScripts	If we want to rerun construction scripts on actors in level
	 */
	void IncrementalUpdateComponents( int32 NumComponentsToUpdate, bool bRerunConstructionScripts );

	/**
	 * Invalidates the cached data used to render the level's UModel.
	 */
	void InvalidateModelGeometry();

#if WITH_EDITOR
	/** Called to create ModelComponents for BSP rendering */
	void CreateModelComponents();
#endif // WITH_EDITOR

	/**
	 * Updates the model components associated with this level
	 */
	ENGINE_API void UpdateModelComponents();

	/**
	 * Commits changes made to the UModel's surfaces.
	 */
	void CommitModelSurfaces();

	/**
	 * Discards the cached data used to render the level's UModel.  Assumes that the
	 * faces and vertex positions haven't changed, only the applied materials.
	 */
	void InvalidateModelSurface();

	/**
	 * Makes sure that all light components have valid GUIDs associated.
	 */
	void ValidateLightGUIDs();

	/**
	 * Sorts the actor list by net relevancy and static behaviour. First all not net relevant static
	 * actors, then all net relevant static actors and then the rest. This is done to allow the dynamic
	 * and net relevant actor iterators to skip large amounts of actors.
	 */
	ENGINE_API void SortActorList();

	virtual bool IsNameStableForNetworking() const override { return true; }		// For now, assume all levels have stable net names

	/** Handles network initialization for actors in this level */
	void InitializeNetworkActors();

	/** Initializes rendering resources for this level. */
	void InitializeRenderingResources();

	/** Releases rendering resources for this level. */
	ENGINE_API void ReleaseRenderingResources();

	/**
	 * Routes pre and post initialize to actors and also sets volumes.
	 *
	 * @todo seamless worlds: this doesn't correctly handle volumes in the multi- level case
	 */
	void RouteActorInitialize();



	/**
	 * Rebuilds static streaming data for all levels in the specified UWorld.
	 *
	 * @param World				Which world to rebuild streaming data for. If NULL, all worlds will be processed.
	 * @param TargetLevel		[opt] Specifies a single level to process. If NULL, all levels will be processed.
	 * @param TargetTexture		[opt] Specifies a single texture to process. If NULL, all textures will be processed.
	 */
	ENGINE_API static void BuildStreamingData(UWorld* World, ULevel* TargetLevel=NULL, UTexture2D* TargetTexture=NULL);

	/**
	 * Rebuilds static streaming data for this level.
	 *
	 * @param TargetTexture			[opt] Specifies a single texture to process. If NULL, all textures will be processed.
	 */
	void BuildStreamingData(UTexture2D* TargetTexture=NULL);

	/**
	 * Clamp lightmap and shadowmap texelfactors to 20-80% range.
	 * This is to prevent very low-res or high-res charts to dominate otherwise decent streaming.
	 */
	void NormalizeLightmapTexelFactor();

	/** Retrieves the array of streamable texture isntances. */
	ENGINE_API TArray<FStreamableTextureInstance>* GetStreamableTextureInstances(UTexture2D*& TargetTexture);

	/**
	* Deprecated. Returns the default brush for this level.
	*
	* @return		The default brush for this level.
	*/
	DEPRECATED(4.3, "GetBrush is deprecated use GetDefaultBrush instead.")
	ENGINE_API ABrush* GetBrush() const;

	/**
	 * Returns the default brush for this level.
	 *
	 * @return		The default brush for this level.
	 */
	ENGINE_API class ABrush* GetDefaultBrush() const;

	/**
	 * Returns the world info for this level.
	 *
	 * @return		The AWorldSettings for this level.
	 */
	ENGINE_API 
	class AWorldSettings* GetWorldSettings() const;

	/**
	 * Returns the level scripting actor associated with this level
	 * @return	a pointer to the level scripting actor for this level (may be NULL)
	 */
	ENGINE_API class ALevelScriptActor* GetLevelScriptActor() const;

	/**
	 * Utility searches this level's actor list for any actors of the specified type.
	 */
	bool HasAnyActorsOfType(UClass *SearchType);

	/**
	 * Resets the level nav list.
	 */
	ENGINE_API void ResetNavList();

#if WITH_EDITOR
	/**
	 *	Grabs a reference to the level scripting blueprint for this level.  If none exists, it creates a new blueprint
	 *
	 * @param	bDontCreate		If true, if no level scripting blueprint is found, none will be created
	 */
	ENGINE_API class ULevelScriptBlueprint* GetLevelScriptBlueprint(bool bDontCreate=false);

	/**
	 *  Returns a list of all blueprints contained within the level
	 */
	ENGINE_API TArray<class UBlueprint*> GetLevelBlueprints() const;

	/**
	 *  Called when the level script blueprint has been successfully changed and compiled.  Handles creating an instance of the blueprint class in LevelScriptActor
	 */
	ENGINE_API void OnLevelScriptBlueprintChanged(class ULevelScriptBlueprint* InBlueprint);
#endif

	/** @todo document */
	ENGINE_API TArray<FVector> const* GetStaticNavigableGeometry() const { return &StaticNavigableGeometry;}

	/** 
	* Is this the persistent level 
	*/
	ENGINE_API bool IsPersistentLevel() const;

	/** 
	* Is this the current level in the world it is owned by
	*/
	ENGINE_API bool IsCurrentLevel() const;
	
	/** 
	 * Shift level actors by specified offset
	 * The offset vector will get subtracted from all actors positions and corresponding data structures
	 *
	 * @param InWorldOffset	 Vector to shift all actors by
	 * @param bWorldShift	 Whether this call is part of whole world shifting
	 */
	ENGINE_API void ApplyWorldOffset(const FVector& InWorldOffset, bool bWorldShift);

	/** Register an actor that should be added to a player's input stack when they are created */
	void RegisterActorForAutoReceiveInput(AActor* Actor, const int32 PlayerIndex);

	/** Push any pending auto receive input actor's input components on to the player controller's input stack */
	void PushPendingAutoReceiveInput(APlayerController* PC);
	
	// Begin IInterface_AssetUserData Interface
	virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	// End IInterface_AssetUserData Interface

#if WITH_EDITOR
	/** meant to be called only from editor, calculating and storing static geometry to be used with off-line and/or on-line navigation building */
	ENGINE_API void RebuildStaticNavigableGeometry();


	/**
	 * Adds a new MovieSceneBindings object to the level script actor.  This actor takes full ownership of
	 * the MovieSceneBindings
	 *
	 * @param	MovieSceneBindings	The MovieSceneBindings object to take ownership of
	 */
	virtual void AddMovieSceneBindings( class UMovieSceneBindings* MovieSceneBindings );

	/**
	 * Clears all existing MovieSceneBindings off of the level.  Only meant to be called by the level script compiler.
	 */
	virtual void ClearMovieSceneBindings();


#endif

};


/**
 * Macro for wrapping Delegates in TScopedCallback
 */
 #define DECLARE_SCOPED_DELEGATE( CallbackName, TriggerFunc )						\
	class ENGINE_API FScoped##CallbackName##Impl										\
	{																				\
	public:																			\
		static void FireCallback() { TriggerFunc; }									\
	};																				\
																					\
	typedef TScopedCallback<FScoped##CallbackName##Impl> FScoped##CallbackName;

DECLARE_SCOPED_DELEGATE( LevelDirtied, ULevel::LevelDirtiedEvent.Broadcast() );

#undef DECLARE_SCOPED_DELEGATE
