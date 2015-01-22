// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// Forward declarations
class ULandscapeComponent;
class UFoliageType;

#include "InstancedFoliage.h"

//
// Forward declarations.
//
class UStaticMesh;

// Current user settings in Foliage UI
struct FFoliageUISettings
{
	void Load();
	void Save();

	// Window
	void SetWindowSizePos(int32 NewX, int32 NewY, int32 NewWidth, int32 NewHeight) { WindowX = NewX; WindowY = NewY; WindowWidth = NewWidth; WindowHeight = NewHeight; }
	void GetWindowSizePos(int32& OutX, int32& OutY, int32& OutWidth, int32& OutHeight) { OutX = WindowX; OutY = WindowY; OutWidth = WindowWidth; OutHeight = WindowHeight; }

	// tool
	bool GetPaintToolSelected() const { return bPaintToolSelected ? true : false; }
	void SetPaintToolSelected(bool InbPaintToolSelected) { bPaintToolSelected = InbPaintToolSelected; }
	bool GetReapplyToolSelected() const { return bReapplyToolSelected ? true : false; }
	void SetReapplyToolSelected(bool InbReapplyToolSelected) { bReapplyToolSelected = InbReapplyToolSelected; }
	bool GetSelectToolSelected() const { return bSelectToolSelected ? true : false; }
	void SetSelectToolSelected(bool InbSelectToolSelected) { bSelectToolSelected = InbSelectToolSelected; }
	bool GetLassoSelectToolSelected() const { return bLassoSelectToolSelected ? true : false; }
	void SetLassoSelectToolSelected(bool InbLassoSelectToolSelected) { bLassoSelectToolSelected = InbLassoSelectToolSelected; }
	bool GetPaintBucketToolSelected() const { return bPaintBucketToolSelected ? true : false; }
	void SetPaintBucketToolSelected(bool InbPaintBucketToolSelected) { bPaintBucketToolSelected = InbPaintBucketToolSelected; }
	bool GetReapplyPaintBucketToolSelected() const { return bReapplyPaintBucketToolSelected ? true : false; }
	void SetReapplyPaintBucketToolSelected(bool InbReapplyPaintBucketToolSelected) { bReapplyPaintBucketToolSelected = InbReapplyPaintBucketToolSelected; }

	float GetRadius() const { return Radius; }
	void SetRadius(float InRadius) { Radius = InRadius; }
	float GetPaintDensity() const { return PaintDensity; }
	void SetPaintDensity(float InPaintDensity) { PaintDensity = InPaintDensity; }
	float GetUnpaintDensity() const { return UnpaintDensity; }
	void SetUnpaintDensity(float InUnpaintDensity) { UnpaintDensity = InUnpaintDensity; }
	bool GetFilterLandscape() const { return bFilterLandscape ? true : false; }
	void SetFilterLandscape(bool InbFilterLandscape) { bFilterLandscape = InbFilterLandscape; }
	bool GetFilterStaticMesh() const { return bFilterStaticMesh ? true : false; }
	void SetFilterStaticMesh(bool InbFilterStaticMesh) { bFilterStaticMesh = InbFilterStaticMesh; }
	bool GetFilterBSP() const { return bFilterBSP ? true : false; }
	void SetFilterBSP(bool InbFilterBSP) { bFilterBSP = InbFilterBSP; }
	bool GetFilterTranslucent() const { return bFilterTranslucent; }
	void SetFilterTranslucent(bool InbFilterTranslucent) { bFilterTranslucent = InbFilterTranslucent; }

	FFoliageUISettings()
		: WindowX(-1)
		, WindowY(-1)
		, WindowWidth(284)
		, WindowHeight(400)
		, bPaintToolSelected(true)
		, bReapplyToolSelected(false)
		, bSelectToolSelected(false)
		, bLassoSelectToolSelected(false)
		, bPaintBucketToolSelected(false)
		, bReapplyPaintBucketToolSelected(false)
		, Radius(512.f)
		, PaintDensity(0.5f)
		, UnpaintDensity(0.f)
		, bFilterLandscape(true)
		, bFilterStaticMesh(true)
		, bFilterBSP(true)
	{
	}

	~FFoliageUISettings()
	{
	}

private:
	int32 WindowX;
	int32 WindowY;
	int32 WindowWidth;
	int32 WindowHeight;

	bool bPaintToolSelected;
	bool bReapplyToolSelected;
	bool bSelectToolSelected;
	bool bLassoSelectToolSelected;
	bool bPaintBucketToolSelected;
	bool bReapplyPaintBucketToolSelected;

	float Radius;
	float PaintDensity;
	float UnpaintDensity;

public:
	bool bFilterLandscape;
	bool bFilterStaticMesh;
	bool bFilterBSP;
	bool bFilterTranslucent;
};


struct FFoliageMeshUIInfo
{
	UFoliageType* Settings;
	FFoliageMeshInfo* MeshInfo;

	FFoliageMeshUIInfo(UFoliageType* InSettings, FFoliageMeshInfo* InMeshInfo)
		: Settings(InSettings)
		, MeshInfo(InMeshInfo)
	{}
};

// Snapshot of current MeshInfo state. Created at start of a brush stroke to store the existing instance info.
class FMeshInfoSnapshot
{
	FFoliageInstanceHash Hash;
	TArray<FVector> Locations;
public:
	FMeshInfoSnapshot(FFoliageMeshInfo* MeshInfo)
		: Hash(*MeshInfo->InstanceHash)
	{
		Locations.AddUninitialized(MeshInfo->Instances.Num());
		for (int32 Idx = 0; Idx < MeshInfo->Instances.Num(); Idx++)
		{
			Locations[Idx] = MeshInfo->Instances[Idx].Location;
		}
	}

	int32 CountInstancesInsideSphere(const FSphere& Sphere)
	{
		int32 Count = 0;

		auto TempInstances = Hash.GetInstancesOverlappingBox(FBox::BuildAABB(Sphere.Center, FVector(Sphere.W)));
		for (int32 Idx : TempInstances)
		{
			if (FSphere(Locations[Idx], 0.f).IsInside(Sphere))
			{
				Count++;
			}
		}

		return Count;
	}
};


/**
 * Foliage editor mode
 */
class FEdModeFoliage : public FEdMode
{
public:
	FFoliageUISettings UISettings;

	/** Constructor */
	FEdModeFoliage();

	/** Destructor */
	virtual ~FEdModeFoliage();

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	/** FEdMode: Called when the mode is entered */
	virtual void Enter() override;

	/** FEdMode: Called when the mode is exited */
	virtual void Exit() override;

	/** FEdMode: Called after an Undo operation */
	virtual void PostUndo() override;

	/** Called when the current level changes */
	void NotifyNewCurrentLevel();

	/** Called when the user changes the current tool in the UI */
	void NotifyToolChanged();

	/**
	 * Called when the mouse is moved over the viewport
	 *
	 * @param	InViewportClient	Level editor viewport client that captured the mouse input
	 * @param	InViewport			Viewport that captured the mouse input
	 * @param	InMouseX			New mouse cursor X coordinate
	 * @param	InMouseY			New mouse cursor Y coordinate
	 *
	 * @return	true if input was handled
	 */
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override;

	/**
	 * FEdMode: Called when the mouse is moved while a window input capture is in effect
	 *
	 * @param	InViewportClient	Level editor viewport client that captured the mouse input
	 * @param	InViewport			Viewport that captured the mouse input
	 * @param	InMouseX			New mouse cursor X coordinate
	 * @param	InMouseY			New mouse cursor Y coordinate
	 *
	 * @return	true if input was handled
	 */
	virtual bool CapturedMouseMove(FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY) override;

	/** FEdMode: Called when a mouse button is pressed */
	virtual bool StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;

	/** FEdMode: Called when a mouse button is released */
	virtual bool EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;

	/** FEdMode: Called once per frame */
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;

	/** FEdMode: Called when a key is pressed */
	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override;

	/** FEdMode: Called when mouse drag input it applied */
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override;

	/** FEdMode: Render elements for the Foliage tool */
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;

	/** FEdMode: Render HUD elements for this tool */
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;

	/** FEdMode: Handling SelectActor */
	virtual bool Select(AActor* InActor, bool bInSelected) override;

	/** FEdMode: Check to see if an actor can be selected in this mode - no side effects */
	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override;

	/** FEdMode: Called when the currently selected actor has changed */
	virtual void ActorSelectionChangeNotify() override;

	/** Notifies all active modes of mouse click messages. */
	bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click);

	/** FEdMode: widget handling */
	virtual FVector GetWidgetLocation() const override;
	virtual bool AllowWidgetMove();
	virtual bool ShouldDrawWidget() const override;
	virtual bool UsesTransformWidget() const override;
	virtual EAxisList::Type GetWidgetAxisToDraw(FWidget::EWidgetMode InWidgetMode) const override;

	virtual bool DisallowMouseDeltaTracking() const override;

	/** Forces real-time perspective viewports */
	void ForceRealTimeViewports(const bool bEnable, const bool bStoreCurrentState);

	/** Trace under the mouse cursor and update brush position */
	void FoliageBrushTrace(FEditorViewportClient* ViewportClient, int32 MouseX, int32 MouseY);

	/** Generate start/end points for a random trace inside the sphere brush.
	    returns a line segment inside the sphere parallel to the view direction */
	void GetRandomVectorInBrush(FVector& OutStart, FVector& OutEnd);

	/** Apply brush */
	void ApplyBrush(FEditorViewportClient* ViewportClient);

	/** Update existing mesh info for current level */
	void UpdateFoliageMeshList();

	TArray<struct FFoliageMeshUIInfo>& GetFoliageMeshList();

	/** Add a new mesh */
	void AddFoliageMesh(UStaticMesh* Mesh);
	void AddFoliageMesh(UFoliageType* Settings);

	/** Remove a mesh */
	bool RemoveFoliageMesh(UFoliageType* Settings);

	/** Reapply cluster settings to all the instances */
	void ReallocateClusters(UFoliageType* Settings);

	/** Replace a mesh with another one */
	bool ReplaceStaticMesh(UFoliageType* Settings, UStaticMesh* NewStaticMesh, bool& bOutMeshMerged);

	/** Bake meshes to StaticMeshActors */
	void BakeFoliage(UFoliageType* Settings, bool bSelectedOnly);

	/**
	 * Copy the settings object for this static mesh
	 *
	 * @param StaticMesh	The static mesh to copy the settings of.
	 *
	 * @return The duplicated settings now assigned to the static mesh.
	 */
	UFoliageType* CopySettingsObject(UFoliageType* Settings);

	/** Replace the settings object for this static mesh with the one specified */
	void ReplaceSettingsObject(UFoliageType* OldSettings, UFoliageType* NewSettings);

	/**
	 * Save the settings object
	 *
	 * @param InSettingsPackageName		The name for the settings package.
	 * @param StaticMesh				The static mesh to save the settings of.
	 *
	 * @return The settings the static mesh is now using.
	 */
	UFoliageType* SaveSettingsObject(const FText& InSettingsPackageName, UFoliageType* Settings);

	/** Apply paint bucket to actor */
	void ApplyPaintBucket(AActor* Actor, bool bRemove);
private:
	/** Add instances inside the brush to match DesiredInstanceCount */
	void AddInstancesForBrush(UWorld* InWorld, AInstancedFoliageActor* IFA, UFoliageType* Settings, FFoliageMeshInfo& MeshInfo, int32 DesiredInstanceCount, const TArray<int32>& ExistingInstances, float Pressure);

	/** Remove instances inside the brush to match DesiredInstanceCount. NOTE: PotentialInstancesToRemove array is modified by this function. */
	void RemoveInstancesForBrush(AInstancedFoliageActor* IFA, FFoliageMeshInfo& MeshInfo, int32 DesiredInstanceCount, TArray<int32>& PotentialInstancesToRemove, float Pressure);

	/** Reapply instance settings to exiting instances */
	void ReapplyInstancesForBrush(UWorld* InWorld, AInstancedFoliageActor* IFA, UFoliageType* Settings, const TArray<int32>& ExistingInstances);

	/** Lookup the vertex color corresponding to a location traced on a static mesh */
	bool GetStaticMeshVertexColorForHit(UStaticMeshComponent* InStaticMeshComponent, int32 InTriangleIndex, const FVector& InHitLocation, FColor& OutVertexColor) const;

	bool bBrushTraceValid;
	FVector BrushLocation;
	FVector BrushTraceDirection;
	UStaticMeshComponent* SphereBrushComponent;

	// Landscape layer cache data
	TMap<FName, TMap<ULandscapeComponent*, TArray<uint8> > > LandscapeLayerCaches;

	// Placed level data
	TArray<FFoliageMeshUIInfo> FoliageMeshList;

	// Cache of instance positions at the start of the transaction
	TMap<UFoliageType*, FMeshInfoSnapshot> InstanceSnapshot;

	bool bToolActive;
	bool bCanAltDrag;

	// The actor that owns any currently visible selections
	AInstancedFoliageActor* SelectionIFA;
};