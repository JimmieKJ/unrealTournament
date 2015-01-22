// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Forwards declarations
class FAssetThumbnail;
class SFoliageEditMeshDisplayItem;
struct FFoliageMeshUIInfo;
class FAssetThumbnailPool;

class SFoliageEdit : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFoliageEdit) {}
	SLATE_END_ARGS()

public:
	/** SCompoundWidget functions */
	void Construct(const FArguments& InArgs);

	~SFoliageEdit();

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	// End of SWidget interface

	/** Creates the thumbnail for the passed in Static Mesh. Used by the MeshListView items. */
	TSharedPtr<FAssetThumbnail> CreateThumbnail(UStaticMesh* InStaticMesh);

	/** Does a full refresh on the list. */
	void RefreshFullList();

	/** Adds a static mesh to the list of available meshes for foliage. May be called on by the MeshListView items. */
	void AddItemToScrollbox(FFoliageMeshUIInfo& InFoliageInfoToAdd);

	/** Removes a static mesh from the list of available meshes for foliage. May be called on by the MeshListView items. */
	void RemoveItemFromScrollbox(const TSharedPtr<SFoliageEditMeshDisplayItem> InWidgetToRemove);

	void ReplaceItem(const TSharedPtr<SFoliageEditMeshDisplayItem> InDisplayItemToReplaceIn, UStaticMesh* InNewStaticMesh);

	/** Handles adding a new item to the list and refreshing the list in it's entirety. */
	FReply OnDrop_ListView(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);

	/** Gets FoliageEditMode. Used by the cluster details to notify changes */
	class FEdModeFoliage* GetFoliageEditMode() const { return FoliageEditMode; }

private:

	/** Clears all the tools selection by setting them to false. */
	void ClearAllToolSelection();

	/** Binds UI commands for the toolbar. */
	void BindCommands();

	/** Creates the toolbar. */
	TSharedRef<SWidget> BuildToolBar();

	/** Delegate callbacks for the UI */

	/** Sets the tool mode to Paint. */
	void OnSetPaint();

	/** Checks if the tool mode is Paint. */
	bool IsPaintTool() const;

	/** Sets the tool mode to Reapply Settings. */
	void OnSetReapplySettings();

	/** Checks if the tool mode is Reapply Settings. */
	bool IsReapplySettingsTool() const;

	/** Sets the tool mode to Select. */
	void OnSetSelectInstance();

	/** Checks if the tool mode is Select. */
	bool IsSelectTool() const;

	/** Sets the tool mode to Lasso Select. */
	void OnSetLasso();

	/** Checks if the tool mode is Lasso Select. */
	bool IsLassoSelectTool() const;

	/** Sets the tool mode to Paint Bucket. */
	void OnSetPaintFill();

	/** Checks if the tool mode is Paint Bucket. */
	bool IsPaintFillTool() const;

	/** Sets the brush Radius for the brush. */
	void SetRadius(float InRadius);

	/** Retrieves the brush Radius for the brush. */
	float GetRadius() const;

	/** Sets the Paint Density for the brush. */
	void SetPaintDensity(float InDensity);

	/** Retrieves the Paint Density for the brush. */
	float GetPaintDensity() const;

	/** Sets the Erase Density for the brush. */
	void SetEraseDensity(float InDensity);

	/** Retrieves the Erase Density for the brush. */
	float GetEraseDensity() const;

	/** Creates the list item widget that displays the instance settings. */
	TSharedRef<ITableRow> MakeWidgetFromOption(TSharedPtr<FFoliageMeshUIInfo> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Sets the filter settings for if painting will occur on Landscapes. */
	void OnCheckStateChanged_Landscape(ECheckBoxState InState);

	/** Retrieves the filter settings for painting on Landscapes. */
	ECheckBoxState GetCheckState_Landscape() const;

	/** Sets the filter settings for if painting will occur on Static Meshes. */
	void OnCheckStateChanged_StaticMesh(ECheckBoxState InState);

	/** Retrieves the filter settings for painting on Static Meshes. */
	ECheckBoxState GetCheckState_StaticMesh() const;

	/** Sets the filter settings for if painting will occur on BSPs. */
	void OnCheckStateChanged_BSP(ECheckBoxState InState);

	/** Retrieves the filter settings for painting on BSPs. */
	ECheckBoxState GetCheckState_BSP() const;

	/** Sets the filter settings for if painting will occur on translucent meshes. */
	void OnCheckStateChanged_Translucent(ECheckBoxState InState);

	/** Retrieves the filter settings for painting on translucent meshes. */
	ECheckBoxState GetCheckState_Translucent() const;

	/** Checks if the text in the empty list overlay should appear. If the list is has items but the the drag and drop override is true, it will return EVisibility::Visible. */
	EVisibility GetVisibility_EmptyList() const;

	/** Should the drop area be visible currently?  Happens when the user is dragging static meshes */
	EVisibility GetVisibility_FoliageDropTarget() const;

	/** Checks if the list should appear. */
	EVisibility GetVisibility_NonEmptyList() const;

	/** Checks if the radius spinbox should appear. Dependant on the current tool being used. */
	EVisibility GetVisibility_Radius() const;

	/** Checks if the paint density spinbox should appear. Dependant on the current tool being used. */
	EVisibility GetVisibility_PaintDensity() const;

	/** Checks if the erase density spinbox should appear. Dependant on the current tool being used. */
	EVisibility GetVisibility_EraseDensity() const;

	/** Checks if the filters should appear. Dependant on the current tool being used. */
	EVisibility GetVisibility_Filters() const;


	/**
	 * Checks if a static mesh can be added to the list of Static Meshes available.
	 *
	 * @param InStaticMesh		The static mesh to test.
	 *
	 * @return	Returns true if the static mesh is not currently in the list, false if it is.
	 */
	bool CanAddStaticMesh(const UStaticMesh* const InStaticMesh) const;

	/** Callback handler when user changes the active streaming level with the level browser */
	void NotifyNewCurrentLevel();

private:
	/** The list view object for displaying Static Meshes to use for foliage. */
	TSharedPtr<SListView<TSharedPtr<FFoliageMeshUIInfo>>> MeshListView;

	/** List of Static Meshes being used for foliage, adapted from a list retrieved from Foliage Mode and items should only be added to it in NotifyChanged to keep information
		accurate between it and the Foliage Mode. */
	TArray<TSharedPtr<FFoliageMeshUIInfo>> MeshList;

	/** Pool for maintaining and rendering thumbnails */
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;

	/** Command list for binding functions for the toolbar. */
	TSharedPtr<FUICommandList> UICommandList;

	/** Pointer to the foliage edit mode. */
	FEdModeFoliage* FoliageEditMode;

	/** Scrollbox for slotting foliage items. */
	TSharedPtr<SScrollBox> ItemScrollBox;

	/** List of items currently being displayed. */
	TArray<TSharedRef<SFoliageEditMeshDisplayItem>> DisplayItemList;

};
