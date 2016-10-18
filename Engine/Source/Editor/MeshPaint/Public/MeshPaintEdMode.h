// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericOctree.h"
#include "GenericOctreePublic.h"
#include "Engine/StaticMesh.h"
#include "IVREditorModule.h"


class IMeshPaintGeometryAdapter;

/** Mesh paint resource types */
namespace EMeshPaintResource
{
	enum Type
	{
		/** Editing vertex colors */
		VertexColors,

		/** Editing textures */
		Texture
	};
}


struct FTexturePaintTriangleInfo
{
	FVector TriVertices[3];
	FVector2D TrianglePoints[3];
	FVector2D TriUVs[3];
};

/** Structure used to house and compare Texture and UV channel pairs */
struct FPaintableTexture
{	
	UTexture*	Texture;
	int32		UVChannelIndex;

	FPaintableTexture(UTexture* InTexture = NULL, uint32 InUVChannelIndex = 0)
		: Texture(InTexture)
		, UVChannelIndex(InUVChannelIndex)
	{}
	
	/** Overloaded equality operator for use with TArrays Contains method. */
	bool operator==(const FPaintableTexture& rhs) const
	{
		return (Texture == rhs.Texture);
		/* && (UVChannelIndex == rhs.UVChannelIndex);*/// if we compare UVChannel we would have to duplicate the texture
	}
};

/** Mesh paint mode */
namespace EMeshPaintMode
{
	enum Type
	{
		/** Painting colors directly */
		PaintColors,

		/** Painting texture blend weights */
		PaintWeights
	};
}


/** Vertex paint target */
namespace EMeshVertexPaintTarget
{
	enum Type
	{
		/** Paint the static mesh component instance in the level */
		ComponentInstance,

		/** Paint the actual static mesh asset */
		Mesh
	};
}


/** Mesh paint color view modes (somewhat maps to EVertexColorViewMode engine enum.) */
namespace EMeshPaintColorViewMode
{
	enum Type
	{
		/** Normal view mode (vertex color visualization off) */
		Normal,

		/** RGB only */
		RGB,
		
		/** Alpha only */
		Alpha,

		/** Red only */
		Red,

		/** Green only */
		Green,

		/** Blue only */
		Blue,
	};
}


/**
 * Mesh Paint settings
 */
class FMeshPaintSettings
{
public:

	/** Static: Returns global mesh paint settings */
	static FMeshPaintSettings& Get()
	{
		return StaticMeshPaintSettings;
	}

protected:

	/** Static: Global mesh paint settings */
	static FMeshPaintSettings StaticMeshPaintSettings;

public:

	/** Constructor */
	FMeshPaintSettings()
		: BrushFalloffAmount( 1.0f ),
		  BrushStrength( 0.2f ),
		  bEnableFlow( true ),
		  FlowAmount( 1.0f ),
		  bOnlyFrontFacingTriangles( true ),
		  ResourceType( EMeshPaintResource::VertexColors ),
		  PaintMode( EMeshPaintMode::PaintColors ),
		  PaintColor( 1.0f, 1.0f, 1.0f, 1.0f ),
		  EraseColor( 0.0f, 0.0f, 0.0f, 0.0f ),
		  bWriteRed( true ),
		  bWriteGreen( true ),
		  bWriteBlue( true ),
		  bWriteAlpha( false ),		// NOTE: Don't write alpha by default
		  TotalWeightCount( 2 ),
		  PaintWeightIndex( 0 ),
		  EraseWeightIndex( 1 ),
		  VertexPaintTarget( EMeshVertexPaintTarget::ComponentInstance ),
		  UVChannel( 0 ),
		  ColorViewMode( EMeshPaintColorViewMode::Normal ),
		  bEnableSeamPainting( true )
	{ }

public:

	/** Amount of falloff to apply (0.0 - 1.0) */
	float BrushFalloffAmount;

	/** Strength of the brush (0.0 - 1.0) */
	float BrushStrength;

	/** Enables "Flow" painting where paint is continually applied from the brush every tick */
	bool bEnableFlow;

	/** When "Flow" is enabled, this scale is applied to the brush strength for paint applied every tick (0.0-1.0) */
	float FlowAmount;

	/** Whether back-facing triangles should be ignored */
	bool bOnlyFrontFacingTriangles;

	/** Resource type we're editing */
	EMeshPaintResource::Type ResourceType;

	/** Mode we're painting in.  If this is set to PaintColors we're painting colors directly; if it's set
	    to PaintWeights we're painting texture blend weights using a single channel. */
	EMeshPaintMode::Type PaintMode;

	/** Colors Mode: Paint color */
	FLinearColor PaintColor;

	/** Colors Mode: Erase color */
	FLinearColor EraseColor;

	/** Colors Mode: True if red colors values should be written */
	bool bWriteRed;

	/** Colors Mode: True if green colors values should be written */
	bool bWriteGreen;

	/** Colors Mode: True if blue colors values should be written */
	bool bWriteBlue;

	/** Colors Mode: True if alpha colors values should be written */
	bool bWriteAlpha;

	/** Weights Mode: Total weight count */
	int32 TotalWeightCount;

	/** Weights Mode: Weight index that we're currently painting */
	int32 PaintWeightIndex;

	/** Weights Mode: Weight index that we're currently using to erase with */
	int32 EraseWeightIndex;

	/**
	 * Vertex paint settings
	 */
	
	/** Vertex paint target */
	EMeshVertexPaintTarget::Type VertexPaintTarget;

	/**
	 * Texture paint settings
	 */
	
	/** UV channel to paint textures using */
	int32 UVChannel;

	/**
	 * View settings
	 */

	/** Color visualization mode */
	EMeshPaintColorViewMode::Type ColorViewMode;

	/** Seam painting flag, True if we should enable dilation to allow the painting of texture seams */
	bool bEnableSeamPainting; 
};


/** Mesh painting action (paint, erase) */
namespace EMeshPaintAction
{
	enum Type
	{
		/** Paint (add color or increase blending weight) */
		Paint,

		/** Erase (remove color or decrease blending weight) */
		Erase,

		/** Fill with the active paint color */
		Fill,

		/** Push instance colors to mesh color */
		PushInstanceColorsToMesh
	};
}


namespace MeshPaintDefs
{
	// Design constraints

	// Currently we never support more than five channels (R, G, B, A, OneMinusTotal)
	static const int32 MaxSupportedPhysicalWeights = 4;
	static const int32 MaxSupportedWeights = MaxSupportedPhysicalWeights + 1;
}


/**
 *  Wrapper to expose texture targets to WPF code.
 */
struct FTextureTargetListInfo
{
	UTexture2D* TextureData;
	bool bIsSelected;
	uint32 UndoCount;
	uint32 UVChannelIndex;
	FTextureTargetListInfo(UTexture2D* InTextureData, int32 InUVChannelIndex, bool InbIsSelected = false)
		:	TextureData(InTextureData)
		,	bIsSelected(InbIsSelected)
		,	UndoCount(0)
		,	UVChannelIndex(InUVChannelIndex)
	{}
};


/** 
 *  Wrapper to store which of a meshes materials is selected as well as the total number of materials. 
 */
struct FMeshSelectedMaterialInfo
{
	int32 NumMaterials;
	int32 SelectedMaterialIndex;

	FMeshSelectedMaterialInfo(int32 InNumMaterials)
		:	NumMaterials(InNumMaterials)
		,	SelectedMaterialIndex(0)
	{}
};


/**
 * Mesh Paint editor mode
 */
class FEdModeMeshPaint : public FEdMode
{
private:
	/** struct used to store the color data copied from mesh instance to mesh instance */
	struct FPerLODVertexColorData
	{
		TArray< FColor > ColorsByIndex;
		TMap<FVector, FColor> ColorsByPosition;
	};

	/** struct used to store the color data copied from mesh component to mesh component */
	struct FPerComponentVertexColorData
	{
		FPerComponentVertexColorData(UStaticMesh* InStaticMesh, int32 InComponentIndex)
			: OriginalMesh(InStaticMesh)
			, ComponentIndex(InComponentIndex)
		{
		}

		/** We match up components by the mesh they use */
		TWeakObjectPtr<UStaticMesh> OriginalMesh;

		/** We also match by component index */
		int32 ComponentIndex;

		/** Vertex colors by LOD */
		TArray<FPerLODVertexColorData> PerLODVertexColorData;
	};

public:

	struct PaintTexture2DData
	{
		/** The original texture that we're painting */
		UTexture2D* PaintingTexture2D;
		bool bIsPaintingTexture2DModified;

		/** A copy of the original texture we're painting, used for restoration. */
		UTexture2D* PaintingTexture2DDuplicate;

		/** Render target texture for painting */
		UTextureRenderTarget2D* PaintRenderTargetTexture;

		/** Render target texture used as an input while painting that contains a clone of the original image */
		UTextureRenderTarget2D* CloneRenderTargetTexture;

		/** List of materials we are painting on */
		TArray< UMaterialInterface* > PaintingMaterials;

		/** Default ctor */
		PaintTexture2DData() :
			PaintingTexture2D( NULL ),
			bIsPaintingTexture2DModified(false),
			PaintingTexture2DDuplicate(NULL),
			PaintRenderTargetTexture( NULL ),
			CloneRenderTargetTexture( NULL )
		{}

		PaintTexture2DData( UTexture2D* InPaintingTexture2D, bool InbIsPaintingTexture2DModified = false ) :
			PaintingTexture2D( InPaintingTexture2D ),
			bIsPaintingTexture2DModified( InbIsPaintingTexture2DModified ),
			PaintRenderTargetTexture( NULL ),
			CloneRenderTargetTexture( NULL )
		{}

		/** Serializer */
		void AddReferencedObjects( FReferenceCollector& Collector )
		{
			// @todo MeshPaint: We're relying on GC to clean up render targets, can we free up remote memory more quickly?
			Collector.AddReferencedObject( PaintingTexture2D );
			Collector.AddReferencedObject( PaintRenderTargetTexture );
			Collector.AddReferencedObject( CloneRenderTargetTexture );
			for( int32 Index = 0; Index < PaintingMaterials.Num(); Index++ )
			{
				Collector.AddReferencedObject( PaintingMaterials[ Index ] );
			}
		}
	};

	/** Constructor */
	FEdModeMeshPaint();

	/** Destructor */
	virtual ~FEdModeMeshPaint();

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	// FEdMode interface
	virtual bool UsesToolkits() const override;
	virtual void Enter() override;
	virtual void Exit() override;
	virtual bool MouseMove( FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y ) override;
	virtual bool CapturedMouseMove( FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY ) override;
	virtual bool StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool InputKey( FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent ) override;
	virtual bool InputDelta( FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale ) override;
	virtual void PostUndo() override;
	virtual void Render( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI ) override;
	virtual bool Select( AActor* InActor, bool bInSelected ) override;
	virtual bool IsSelectionAllowed( AActor* InActor, bool bInSelection ) const override;
	virtual void ActorSelectionChangeNotify() override;
	virtual bool AllowWidgetMove() override { return false; }
	virtual bool ShouldDrawWidget() const override { return false; }
	virtual bool UsesTransformWidget() const override { return false; }
	virtual void Tick( FEditorViewportClient* ViewportClient, float DeltaTime ) override;
	virtual EEditAction::Type GetActionEditDelete() override { return EEditAction::Process; }
	virtual bool ProcessEditDelete() override;
	// End of FEdMode interface


	/** Called when an editor mode is entered or exited */
	void OnEditorModeChanged( FEdMode* EditorMode, bool bEntered );

	/** Returns true if we need to force a render/update through based fill/copy */
	bool IsForceRendered (void) const;

	/** Saves out cached mesh settings for the given actor */
	void SaveSettingsForActor( AActor* InActor );

	/** Updates settings for the specified component with updated textures */
	void UpdateSettingsForMeshComponent( UMeshComponent* InMeshComponent, UTexture2D* InOldTexture, UTexture2D* InNewTexture );

	/** Helper function to get the current paint action for use in DoPaint */
	EMeshPaintAction::Type GetPaintAction(FViewport* InViewport);

	/** Removes vertex colors associated with the object */
	void RemoveInstanceVertexColors(UObject* Obj) const;

	/** Removes vertex colors associated with the static mesh component */
	void RemoveComponentInstanceVertexColors(UStaticMeshComponent* StaticMeshComponent) const;

	/** Removes vertex colors associated with the currently selected mesh */
	void RemoveInstanceVertexColors() const;

	/** Copies vertex colors associated with the currently selected mesh */
	void CopyInstanceVertexColors();

	/** Pastes vertex colors to the currently selected mesh */
	void PasteInstanceVertexColors();

	/** Returns whether the instance vertex colors associated with the currently selected mesh need to be fixed up or not */
	bool RequiresInstanceVertexColorsFixup() const;

	/** Attempts to fix up the instance vertex colors associated with the currently selected mesh, if necessary */
	void FixupInstanceVertexColors() const;

	/** Applies vertex color painting on LOD0 to all lower LODs. */
	void ApplyVertexColorsToAllLODs();

	/** Forces all selected meshes to their best LOD, or removes the forcing. */
	void ApplyOrRemoveForceBestLOD(bool bApply);

	/** Forces the static mesh component to it's best LOD, or removes the forcing. */
	void ApplyOrRemoveForceBestLOD(const IMeshPaintGeometryAdapter& GeometryInfo, UMeshComponent* InMeshComponent, bool bApply);

	/** Applies vertex color painting on LOD0 to all lower LODs. */
	void ApplyVertexColorsToAllLODs(const IMeshPaintGeometryAdapter& GeometryInfo, UMeshComponent* InMeshComponent);

	/** Fills the vertex colors associated with the currently selected mesh*/
	void FillInstanceVertexColors();

	/** Pushes instance vertex colors to the  mesh*/
	void PushInstanceVertexColorsToMesh();

	/** Creates a paintable material/texture for the selected mesh */
	void CreateInstanceMaterialAndTexture() const;

	/** Removes instance of paintable material/texture for the selected mesh */
	void RemoveInstanceMaterialAndTexture() const;

	/** Returns information about the currently selected mesh */
	bool GetSelectedMeshInfo( int32& OutTotalBaseVertexColorBytes, int32& OutTotalInstanceVertexColorBytes, bool& bOutHasInstanceMaterialAndTexture ) const;

	/** Sets the default brush radius size */
	void SetBrushRadiiDefault( float InBrushRadius );

	/** Returns the default brush radius size */
	float GetBrushRadiiDefault() const;

	/** Returns the min and max brush radius sizes specified in the user config */
	void GetBrushRadiiSliderLimits( float& OutMinBrushSliderRadius, float& OutMaxBrushSliderRadius ) const;

	/** Returns the min and max brush radius sizes that the user is allowed to type in */
	void GetBrushRadiiLimits( float& OutMinBrushRadius, float& OutMaxBrushRadius ) const;

	/** Returns whether there are colors in the copy buffer */
	bool CanPasteVertexColors() const;

	/** 
	 * Will update the list of available texture paint targets based on selection 
	 */
	void UpdateTexturePaintTargetList();

	/** Will update the list of available texture paint targets based on selection */
	int32 GetMaxNumUVSets() const;

	/** Will return the list of available texture paint targets */
	TArray<FTextureTargetListInfo>* GetTexturePaintTargetList();

	/** Will return the selected target paint texture if there is one. */
	UTexture2D* GetSelectedTexture();

	void SetSelectedTexture( const UTexture2D* Texture );

	/** will find the currently selected paint target texture in the content browser */
	void FindSelectedTextureInContentBrowser();

	/** 
	 * Used to commit all paint changes to corresponding target textures. 
	 * @param	bShouldTriggerUIRefresh		Flag to trigger a UI Refresh if any textures have been modified (defaults to true)
	 */	
	void CommitAllPaintedTextures();

	/** Clears all texture overrides for this component. */
	void ClearMeshTextureOverrides(const IMeshPaintGeometryAdapter& GeometryInfo, UMeshComponent* InMeshComponent);

	/** Clears all texture overrides, removing any pending texture paint changes */
	void ClearAllTextureOverrides();

	void SetAllTextureOverrides(const IMeshPaintGeometryAdapter& GeometryInfo, UMeshComponent* InMeshComponent);

	void SetSpecificTextureOverrideForMesh(const IMeshPaintGeometryAdapter& GeometryInfo, UTexture2D* Texture);

	/** Used to tell the texture paint system that we will need to restore the rendertargets */
	void RestoreRenderTargets();

	/** Returns the number of texture that require a commit. */
	int32 GetNumberOfPendingPaintChanges();

	/** Returns index of the currently selected Texture Target */
	int32 GetCurrentTextureTargetIndex() const;

	bool UpdateTextureList() { return bShouldUpdateTextureList; };

	/** Duplicates the currently selected texture and material, relinking them together and assigning them to the selected meshes. */
	void DuplicateTextureMaterialCombo();

	/** Will create a new texture. */
	void CreateNewTexture();

	/** Sets the currently editing actor to the actor thats passed in */
	void SetEditingMesh(  TWeakObjectPtr<AActor> InActor );

	/** Returns an array of weak object pointers to the currently editing meshes */
	TArray<TWeakObjectPtr<AActor>> GetEditingActors() const;

	/** Returns a weak pointer to the currently Editing actor */
	TWeakObjectPtr<AActor> GetEditingActor() const;

	/** Sets the currently editing material index */
	void SetEditingMaterialIndex( int32 SelectedIndex );

	/** Returns the currently editing material index */
	int32 GetEditingMaterialIndex() const;

	/** Returns the amount of materials on the currently editing mesh */
	int32 GetEditingActorsNumberOfMaterials() const;

	/** Is the tool currently painting vertices */
	bool IsPainting() const { return bIsPainting; }

private:

	/** Struct to hold MeshPaint settings on a per mesh basis */
	struct StaticMeshSettings
	{
		UTexture2D* SelectedTexture;
		int32 SelectedUVChannel;

		StaticMeshSettings( )
			:	SelectedTexture(NULL)
			,	SelectedUVChannel(0)
		{}
		StaticMeshSettings( UTexture2D* InSelectedTexture, int32 InSelectedUVSet )
			:	SelectedTexture(InSelectedTexture)
			,	SelectedUVChannel(InSelectedUVSet)
		{}

		void operator=(const StaticMeshSettings& SrcSettings)
		{
			SelectedTexture = SrcSettings.SelectedTexture;
			SelectedUVChannel = SrcSettings.SelectedUVChannel;
		}
	};

	/** Static: Determines if a world space point is influenced by the brush and reports metrics if so */
	static bool IsPointInfluencedByBrush( const FVector& InPosition,
										   const class FMeshPaintParameters& InParams,
										   float& OutSquaredDistanceToVertex2D,
										   float& OutVertexDepthToBrush );

	/** Static: Paints the specified vertex!  Returns true if the vertex was in range. */
	static bool PaintVertex( const FVector& InVertexPosition,
							  const class FMeshPaintParameters& InParams,
							  const bool bIsPainting,
							  FColor& InOutVertexColor );

	/** Paint the mesh that impacts the specified ray */
	void DoPaint( const FVector& InCameraOrigin,
				  const FVector& InRayOrigin,
				  const FVector& InRayDirection,
				  FPrimitiveDrawInterface* PDI,
				  const EMeshPaintAction::Type InPaintAction,
				  const bool bVisualCueOnly,
				  const float InStrengthScale,
				  OUT bool& bAnyPaintAbleActorsUnderCursor);

	/** Paints mesh vertices */
	void PaintMeshVertices(UStaticMeshComponent* StaticMeshComponent, const FMeshPaintParameters& Params, const bool bShouldApplyPaint, FStaticMeshLODResources& LODModel, const FVector& ActorSpaceCameraPosition, const FMatrix& ActorToWorldMatrix, const float ActorSpaceSquaredBrushRadius, const FVector& ActorSpaceBrushPosition, FPrimitiveDrawInterface* PDI, const float VisualBiasDistance, const IMeshPaintGeometryAdapter& GeometryInfo);

	/** Paints mesh texture */
	void PaintMeshTexture( UMeshComponent* MeshComponent, const FMeshPaintParameters& Params, const bool bShouldApplyPaint, const FVector& ActorSpaceCameraPosition, const FMatrix& ActorToWorldMatrix, const float ActorSpaceSquaredBrushRadius, const FVector& ActorSpaceBrushPosition, const IMeshPaintGeometryAdapter& GeometryInfo );

	/** Forces real-time perspective viewports */
	void ForceRealTimeViewports( const bool bEnable, const bool bStoreCurrentState );

	/** Sets show flags for perspective viewports */
	void SetViewportShowFlags( const bool bAllowColorViewModes, FEditorViewportClient& Viewport );

	/** Starts painting a texture */
	void StartPaintingTexture(UMeshComponent* InMeshComponent, const IMeshPaintGeometryAdapter& GeometryInfo);

	/** Paints on a texture */
	void PaintTexture( const FMeshPaintParameters& InParams,
					   const TArray< uint32 >& InInfluencedTriangles,
					   const FMatrix& InActorToWorldMatrix,
					   const IMeshPaintGeometryAdapter& GeometryInfo);

	/** Finishes painting a texture */
	void FinishPaintingTexture();

	/** Makes sure that the render target is ready to paint on */
	void SetupInitialRenderTargetData( UTexture2D* InTextureSource, UTextureRenderTarget2D* InRenderTarget );

	/** Static: Copies a texture to a render target texture */
	static void CopyTextureToRenderTargetTexture(UTexture* SourceTexture, UTextureRenderTarget2D* RenderTargetTexture, ERHIFeatureLevel::Type FeatureLevel);

	/** Will generate a mask texture, used for texture dilation, and store it in the passed in rendertarget */
	bool GenerateSeamMask(UMeshComponent* MeshComponent, int32 UVSet, UTextureRenderTarget2D* RenderTargetTexture);

	/** Static: Creates a temporary texture used to transfer data to a render target in memory */
	UTexture2D* CreateTempUncompressedTexture( UTexture2D* SourceTexture );

	/** Used when we want to change selection to the next available paint target texture.  Will stop at the end of the list and will NOT cycle to the beginning of the list. */
	void SelectNextTexture() { ShiftSelectedTexture( true, false ); }

	/** Used to change the selected texture paint target texture. Will stop at the beginning of the list and will NOT cycle to the end of the list. */
	void SelectPrevTexture() { ShiftSelectedTexture( false, false); }

	/**
	 * Used to change the currently selected paint target texture.
	 *
	 * @param	bToTheRight 	True if a shift to next texture is desired, false if a shift to the previous texture is desired.
	 * @param	bCycle		 	If set to False, this function will stop at the first or final element.  If true the selection will cycle to the opposite end of the list.
	 */
	void ShiftSelectedTexture( bool bToTheRight = true, bool bCycle = true );

	
	/**
	 * Used to get a reference to data entry associated with the texture.  Will create a new entry if one is not found.
	 *
	 * @param	inTexture 		The texture we want to retrieve data for.
	 * @return					Returns a reference to the paint data associated with the texture.  This reference
	 *								is only valid until the next change to any key in the map.  Will return NULL only when inTexture is NULL.
	 */
	PaintTexture2DData* GetPaintTargetData(  UTexture2D* inTexture );

	/**
	 * Used to add an entry to to our paint target data.
	 *
	 * @param	inTexture 		The texture we want to create data for.
	 * @return					Returns a reference to the newly created entry.  If an entry for the input texture already exists it will be returned instead.
	 *								Will return NULL only when inTexture is NULL.   This reference is only valid until the next change to any key in the map.
	 *								 
	 */
	PaintTexture2DData* AddPaintTargetData(  UTexture2D* inTexture );

	/**
	 * Used to get the original texture that was overridden with a render target texture.
	 *
	 * @param	inTexture 		The render target that was used to override the original texture.
	 * @return					Returns a reference to texture that was overridden with the input render target texture.  Returns NULL if we don't find anything.
	 *								 
	 */
	UTexture2D* GetOriginalTextureFromRenderTarget( UTextureRenderTarget2D* inTexture );

	/**
	* Ends the outstanding transaction, if one exists.
	*/
	void EndTransaction();

	/**
	* Begins a new transaction, if no outstanding transaction exists.
	*/
	void BeginTransaction(const FText& Description);

	/** Helper function to start painting, resets timer etc. */
	void StartPainting();

	/** Helper function to end painting, finish painting textures & update transactions */
	void EndPainting();

	/** Caches the currently selected actors info into CurrentlySelectedActorsMaterialInfo */
	void CacheActorInfo();

	/** Returns valid MeshComponents in the current selection */
	TArray<UMeshComponent*> GetSelectedMeshComponents() const;

	/** Create geometry adapters for selected components */
	void CreateGeometryAdaptersForSelectedComponents();

	/** Create a new geometry adapter */
	IMeshPaintGeometryAdapter* AddGeometryAdapter(UMeshComponent* MeshComponent);

	/** Finds an existing geometry adapter for the given component */
	IMeshPaintGeometryAdapter* FindGeometryAdapter(UMeshComponent* MeshComponent);

	/** Remove a geometry adapter */
	void RemoveGeometryAdapter(UMeshComponent* MeshComponent);

	/** Removes all geometry adapters from the cache */
	void RemoveAllGeometryAdapters();

	/** Called prior to saving a level */
	void OnPreSaveWorld(uint32 SaveFlags, UWorld* World);

	/** Called after saving a level */
	void OnPostSaveWorld(uint32 SaveFlags, UWorld* World, bool bSuccess);

	/** Called when an asset has just been imported */
	void OnPostImportAsset(UFactory* Factory, UObject* Object);

	/** Called when an asset has just been reimported */
	void OnPostReimportAsset(UObject* Object, bool bSuccess);

	/** Called when an asset is deleted */
	void OnAssetRemoved(const FAssetData& AssetData);

	/** Called when the user presses a button on their motion controller device */
	void OnVRAction( class FEditorViewportClient& ViewportClient, class UViewportInteractor* Interactor,
	const struct FViewportActionKeyInput& Action, bool& bOutIsInputCaptured, bool& bWasHandled );

	/** Called when rerunning a construction script causes objects to be replaced */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

private:

	/** Whether we're currently painting */
	bool bIsPainting;

	/** When painting in VR, this is the hand index that we're painting with.  Otherwise INDEX_NONE. */
	class UViewportInteractor* PaintingWithInteractorInVR;

	/** Whether we are doing a flood fill the next render */
	bool bIsFloodFill;

	/** Whether we are pushing the instance colors to the mesh next render */
	bool bPushInstanceColorsToMesh;

	/** Will store the state of selection locks on start of paint mode so that it can be restored on close */
	bool bWasSelectionLockedOnStart;

	/** True when the actor selection has changed since we last updated the TexturePaint list, used to avoid reloading the same textures */
	bool bShouldUpdateTextureList;

	/** True if we need to generate a texture seam mask used for texture dilation */
	bool bGenerateSeamMask;

	/** Real time that we started painting */
	double PaintingStartTime;

	/** Array of static meshes that we've modified */
	TArray< UStaticMesh* > ModifiedStaticMeshes;

	/** Which mesh LOD index we're painting */
	// @todo MeshPaint: Allow painting on other LODs?
	static const uint32 PaintingMeshLODIndex = 0;

	/** Texture paint: The mesh components that we're currently painting */
	UMeshComponent* TexturePaintingCurrentMeshComponent;

	/** Texture paint: The LOD being used for texture painting. */
	int32 TexturePaintingStaticMeshLOD;

	/** The original texture that we're painting */
	UTexture2D* PaintingTexture2D;

	/** Stores data associated with our paint target textures */
	TMap< UTexture2D*, PaintTexture2DData > PaintTargetData;

	/** Temporary buffers used when copying/pasting colors */
	TArray<FPerComponentVertexColorData> CopiedColorsByComponent;

	/** Texture paint: Will hold a list of texture items that we can paint on */
	TArray<FTextureTargetListInfo> TexturePaintTargetList;

	/** Map of settings for each StaticMeshComponent */
	TMap< UMeshComponent*, StaticMeshSettings > StaticMeshSettingsMap;

	/** Map from UMeshComponent to the associated MeshPaintAdapter */
	TMap< UMeshComponent*, TSharedPtr<IMeshPaintGeometryAdapter> > ComponentToAdapterMap;

	/** Used to store a flag that will tell the tick function to restore data to our rendertargets after they have been invalidated by a viewport resize. */
	bool bDoRestoreRenTargets;

	/** Temporary rendertarget used to draw incremental paint to */
	UTextureRenderTarget2D* BrushRenderTargetTexture;
	
	/** Temporary rendertarget used to store a mask of the affected paint region, updated every time we add incremental texture paint */ 
	UTextureRenderTarget2D* BrushMaskRenderTargetTexture;
	
	/** Temporary rendertarget used to store generated mask for texture seams, we create this by projecting object triangles into texture space using the selected UV channel */
	UTextureRenderTarget2D* SeamMaskRenderTargetTexture;

	/** Used to store the transaction to some vertex paint operations that can not be easily scoped. */
	FScopedTransaction* ScopedTransaction;

	/** A map of the currently selected actors against info required for painting (selected material index etc)*/ 
	TMap<TWeakObjectPtr<AActor>,FMeshSelectedMaterialInfo> CurrentlySelectedActorsMaterialInfo;

	/** The currently selected actor, used to refer into the Map of Selected actor info */
	TWeakObjectPtr<AActor> ActorBeingEdited;
};


/**
* Imports vertex colors from tga file onto selected meshes
*/
class FImportVertexTextureHelper  
{
public:

	/**
	 * Enumerates channel masks.
	 */
	enum ChannelsMask
	{
		/** Red channel. */
		ERed		= 0x1,

		/** Green channel. */
		EGreen		= 0x2,

		/** Blue channel. */
		EBlue		= 0x4,

		/** Alpha channel. */
		EAlpha		= 0x8,
	};

	/**
	 * Color picker function. Retrieves pixel color from coordinates and mask.
	 *
	 * @param NewVertexColor Returned color.
	 * @param MipData Highest mip-map with pixels data.
	 * @param UV Texture coordinate to read.
	 * @param Tex Texture info.
	 * @param ColorMask Mask for filtering which colors to use.
	 */
	void PickVertexColorFromTex(FColor& NewVertexColor, uint8* MipData, FVector2D & UV, UTexture2D* Tex, uint8& ColorMask);
	
	/**
	 * Imports Vertex Color data from texture scanning thought uv vertex coordinates for selected actors.  
	 *
	 * @param ModeTools the mode tools for the current editor
	 * @param Filename path for loading TGA file.
	 * @param UVIndex Coordinate index.
	 * @param ImportLOD LOD level to work with.
	 * @param Tex Texture info.
	 * @param ColorMask Mask for filtering which colors to use.
	 */
	void ImportVertexColors(FEditorModeTools* ModeTools, const FString& Filename, int32 UVIndex, int32 ImportLOD, uint8 ColorMask);
};
