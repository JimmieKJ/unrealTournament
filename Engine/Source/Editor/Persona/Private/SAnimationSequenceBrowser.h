// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "AssetData.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "PreviewScene.h"

//////////////////////////////////////////////////////////////////////////
// FAnimationAssetViewportClient
class FAnimationAssetViewportClient : public FEditorViewportClient
{
public:
	FAnimationAssetViewportClient(FPreviewScene& InPreviewScene);

	// FEditorViewportClient interface
	virtual FSceneInterface* GetScene() const override;
	virtual FLinearColor GetBackgroundColor() const override;
	// End of FEditorViewportClient interface
};

//////////////////////////////////////////////////////////////////////////
// SAnimationSequenceBrowser

class SAnimationSequenceBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAnimationSequenceBrowser)
		: _Persona()
		{}

		SLATE_ARGUMENT(TSharedPtr<class FPersona>, Persona)
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs);

	void OnRequestOpenAsset(const FAssetData& AssetData, bool bFromHistory);

	virtual ~SAnimationSequenceBrowser();

	/** Delegate that handles creation of context menu */
	TSharedPtr<SWidget> OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets);

	/** Delegate to handle "Go To" context menu option 
		Sends selected assets to be highlighted in active Content Browser */
	void OnGoToInContentBrowser(TArray<FAssetData> ObjectsToSync);

	/** Delegate to handle "Save" context menu option */
	void SaveSelectedAssets(TArray<FAssetData> ObjectsToSave) const;

	/** Delegate to handle enabling the "Save" context menu option */
	bool CanSaveSelectedAssets(TArray<FAssetData> ObjectsToSave) const;

	/** Delegate to handle compression context menu option 
		Applies a chosen compression method to the selected assets */
	void OnApplyCompression(TArray<FAssetData> SelectedAssets);

	/** Delegate to handle Export FBX context menu option */
	void OnExportToFBX(TArray<FAssetData> SelectedAssets);

	/** Delegate to handle Add looping interpolation context menu option */
	void OnAddLoopingInterpolation(TArray<FAssetData> SelectedAssets);

	/** Delegate to handle set current preview mesh context menu option */
	void OnSetCurrentPreviewMesh(TArray<FAssetData> SelectedAssets);

	/** Delegate to handle reimport selected animations */
	void OnReimportAnimation(TArray<FAssetData> SelectedAssets);

	/** This will allow duplicate the current object, and remap to new skeleton 
	 *	Only allowed for AnimSequence 
	 */
	void OnCreateCopy(TArray<FAssetData> Selected);

	/** Refresh list */
	void SelectAsset(UAnimationAsset * AnimAsset);

protected:
	bool CanShowColumnForAssetRegistryTag(FName AssetType, FName TagName) const;
	
	/** Populate supplied OutPackages with the packages for the supplied Assets array */
	void GetSelectedPackages(const TArray<FAssetData>& Assets, TArray<UPackage*>& OutPackages) const;

	/** Adds the supplied asset to the asset history */
	void AddAssetToHistory(const FAssetData& AssetData);

	void CacheOriginalAnimAssetHistory();

	/** Single step forward in history */
	FReply OnGoForwardInHistory();

	/** Single step back in history */
	FReply OnGoBackInHistory();

	/** Jumps immediately to an index in the history if valid */
	void GoToHistoryIndex(int32 InHistoryIdx);

	/** Returns TRUE if stepping backward in history is allowed */
	bool CanStepBackwardInHistory() const;

	/** Returns TRUE if stepping forward in history is allowed */
	bool CanStepForwardInHistory() const;

	/**
	 * Mouse down callback to display a history menu
	 *
	 * @param InMenuAnchor		This is the anchor the menu will use for positioning
	 */
	FReply OnMouseDownHisory( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TWeakPtr< SMenuAnchor > InMenuAnchor );

	/** 
	 * Callback to create the history menu.
	 *
	 * @param bInBackHistory		TRUE if the back history is requested, FALSE if the forward history is
	 *
	 * @return						The menu widget displaying all available history
	 */
	TSharedRef<SWidget> CreateHistoryMenu(bool bInBackHistory) const;

	/**
	 * Create the viewport and required objects to control the viewport used in tooltips
	 */
	void CreateAssetTooltipResources();

	/**
	 * Create a custom asset view tooltip for the provided asset
	 */
	TSharedRef<SToolTip> CreateCustomAssetToolTip(FAssetData& AssetData);

	/**
	 * Called as a tooltip is about to show;
	 */
	bool OnVisualizeAssetToolTip(const TSharedPtr<SWidget>& TooltipContent, FAssetData& AssetData);

	/**
	* Called as a tooltip is closing
	*/
	void OnAssetToolTipClosing();

	/**
	 * Cleaup a component from the preview scene along with any attached children
	 */
	void CleanupPreviewSceneComponent(USceneComponent* Component);

	/**
	 * Has the animation asset preview tool tip been visualized
	 */
	bool IsToolTipPreviewVisible();

	/** Returns visible when not in a Blueprint mode (anim mode, etc...) */
	EVisibility GetNonBlueprintModeVisibility() const;
protected:
	/**
	 * The actual viewport widget
	 */	
	TSharedPtr<SViewport> ViewportWidget;

	/**
	 * The scene viewport data
	 */
	TSharedPtr<FSceneViewport> SceneViewport;

	/**
	 * Custom viewport client used for tooltip previews
	 */
	TSharedPtr<FAnimationAssetViewportClient> ViewportClient;

	/**
	 * Skeletal component to preview the animation asset on
	 */
	UDebugSkelMeshComponent* PreviewComponent;

	/**
	 * The scene to show in the asset previews
	 */
	FPreviewScene PreviewScene;

	// Pointer back to persona tool that owns us
	TWeakPtr<class FPersona> PersonaPtr;

	// Set of tags to prevent creating details view columns for (infrequently used)
	TSet<FName> AssetRegistryTagsToIgnore;

	// List of recently opened assets
	TArray<FAssetData> AssetHistory;

	// Current position in the asset history
	int32 CurrentAssetHistoryIndex;

	// Max assets to save in history
	static const int32 MaxAssetsHistory;

	// Track if we have tried to cache the first asset we were playing
	bool bTriedToCacheOrginalAsset;

	// delegate to sync the asset picker to selected assets
	FSyncToAssetsDelegate SyncToAssetsDelegate;
	FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;

	void RetargetAnimationHandler(USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bRemapReferencedAssets, bool bConvertSpaces, TArray<TWeakObjectPtr<UObject>> InAnimAssets);

private:
	/** Updates the animation preview in the tooltip */
	EActiveTimerReturnType UpdateTootipPreview( double InCurrentTime, float InDeltaTime );

	/** Whether the tooltip was visualized this frame */
	uint8 bToolTipClosedThisFrame : 1;

	/** Whether the active timer is currently registered */
	uint8 bIsActiveTimerRegistered : 1;

	/** Whether the active timer should stop */
	uint8 bToolTipVisualizedThisFrame : 1;
};