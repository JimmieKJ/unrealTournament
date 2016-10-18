// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Developer/AssetTools/Public/IAssetTypeActions.h"
#include "Runtime/AssetRegistry/Public/ARFilter.h"


class FAssetData;
class FDragDropOperation;
class FExtender;
class FReply;
class SToolTip;
struct FCollectionNameType;


/** Called when a collection is selected in the collections view */
DECLARE_DELEGATE_OneParam( FOnCollectionSelected, const FCollectionNameType& /*SelectedCollection*/);

/** Called to retrieve the tooltip for the specified asset */
DECLARE_DELEGATE_RetVal_OneParam( TSharedRef< SToolTip >, FConstructToolTipForAsset, const FAssetData& /*Asset*/);

/** Called to check if an asset should be filtered out by external code. Return true to exclude the asset from the view. */
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnShouldFilterAsset, const FAssetData& /*AssetData*/);

/** Called to check if an asset tag should be display in details view. Return false to exclude the asset from the view. */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnShouldDisplayAssetTag, FName /*AssetType*/, FName /*TagName*/);

/** Called to clear the selection of the specified assetdata or all selection if an invalid assetdata is passed */
DECLARE_DELEGATE( FClearSelectionDelegate );

/** Called when thumbnail scale changes and the thumbnail scale is bound to a delegate */
DECLARE_DELEGATE_OneParam( FOnThumbnailScaleChanged, const float /*NewScale*/);

/** Called to retrieve an array of the currently selected asset data */
DECLARE_DELEGATE_RetVal( TArray< FAssetData >, FGetCurrentSelectionDelegate );

/** Called to retrieve an array of the currently selected asset data */
DECLARE_DELEGATE_OneParam(FSyncToAssetsDelegate, const TArray< FAssetData >& /*AssetData*/);

/** Called to set a new filter for an existing asset picker */
DECLARE_DELEGATE_OneParam(FSetARFilterDelegate, const FARFilter& /*NewFilter*/);

/** A pointer to an existing delegate that, when executed, will set the filter an the asset picker after it is created. */
DECLARE_DELEGATE_OneParam(FSetPathPickerPathsDelegate, const TArray<FString>& /*NewPaths*/);

/** Called to adjust the selection from the current assetdata, should be +1 to increment or -1 to decrement */
DECLARE_DELEGATE_OneParam( FAdjustSelectionDelegate, const int32 /*direction*/ );

/** Called when an asset is selected in the asset view */
DECLARE_DELEGATE_OneParam(FOnAssetSelected, const FAssetData& /*AssetData*/);

/** Called when the user double clicks, presses enter, or presses space on an asset */
DECLARE_DELEGATE_TwoParams(FOnAssetsActivated, const TArray<FAssetData>& /*ActivatedAssets*/, EAssetTypeActivationMethod::Type /*ActivationMethod*/);

/** Called when an asset has begun being dragged by the user */
DECLARE_DELEGATE_RetVal_OneParam(FReply, FOnAssetDragged, const TArray<FAssetData>& /*AssetData*/);

/** Called when an asset is clicked on in the asset view */
DECLARE_DELEGATE_OneParam( FOnAssetClicked, const FAssetData& /*AssetData*/ );

/** Called when an asset is double clicked in the asset view */
DECLARE_DELEGATE_OneParam(FOnAssetDoubleClicked, const FAssetData& /*AssetData*/);

/** Called when enter is pressed on an asset in the asset view */
DECLARE_DELEGATE_OneParam(FOnAssetEnterPressed, const TArray<FAssetData>& /*SelectedAssets*/);

/** Called when a new folder is starting to be created */
DECLARE_DELEGATE_TwoParams(FOnCreateNewFolder, const FString& /*FolderName*/, const FString& /*FolderPath*/);

/** Called to request the menu when right clicking on an asset */
DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<SWidget>, FOnGetAssetContextMenu, const TArray<FAssetData>& /*SelectedAssets*/);

/** Called when a path is selected in the path picker */
DECLARE_DELEGATE_OneParam(FOnPathSelected, const FString& /*Path*/);

/** Called when a path is double clicked in the asset view */
DECLARE_DELEGATE_OneParam(FOnPathDoubleClicked, const FString& /*Path*/);

/** Called to request the menu when right clicking on a path */
DECLARE_DELEGATE_RetVal(TSharedRef<FExtender>, FContentBrowserMenuExtender);
DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<FExtender>, FContentBrowserMenuExtender_SelectedAssets, const TArray<FAssetData>& /*SelectedAssets*/);
DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<FExtender>, FContentBrowserMenuExtender_SelectedPaths, const TArray<FString>& /*SelectedPaths*/);

/** Called to request the menu when right clicking on an asset */
DECLARE_DELEGATE_RetVal_ThreeParams(TSharedPtr<SWidget>, FOnGetFolderContextMenu, const TArray<FString>& /*SelectedPaths*/, FContentBrowserMenuExtender_SelectedPaths /*MenuExtender*/, FOnCreateNewFolder /*CreationDelegate*/);

/** Called to request a custom asset item tooltip */
DECLARE_DELEGATE_RetVal_OneParam( TSharedRef<SToolTip>, FOnGetCustomAssetToolTip, FAssetData& /*AssetData*/);

/** Called when an asset item visualizes its tooltip */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnVisualizeAssetToolTip, const TSharedPtr<SWidget>& /*ToolTipContent*/, FAssetData& /*AssetData*/);

/** Called from the Asset Dialog when a non-modal dialog is closed or cancelled */
DECLARE_DELEGATE(FOnAssetDialogCancelled);

/** Called when an asset item's tooltip is closing */
DECLARE_DELEGATE( FOnAssetToolTipClosing );

/** Called from the Asset Dialog when assets are chosen in non-modal Open dialogs */
DECLARE_DELEGATE_OneParam(FOnAssetsChosenForOpen, const TArray<FAssetData>& /*SelectedAssets*/);

/** Called from the Asset Dialog when an asset name is chosen in non-modal Save dialogs */
DECLARE_DELEGATE_OneParam(FOnObjectPathChosenForSave, const FString& /*ObjectPath*/);


/** Contains the delegates used to handle a custom drag-and-drop in the asset view */
struct FAssetViewDragAndDropExtender
{
	struct FPayload
	{
		FPayload(TSharedPtr<FDragDropOperation> InDragDropOp, const TArray<FName>& InPackagePaths, const TArray<FCollectionNameType>& InCollections)
			: DragDropOp(MoveTemp(InDragDropOp))
			, PackagePaths(InPackagePaths)
			, Collections(InCollections)
		{ }

		TSharedPtr<FDragDropOperation> DragDropOp;
		const TArray<FName>& PackagePaths;
		const TArray<FCollectionNameType>& Collections;
	};

	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnDropDelegate, const FPayload&);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnDragOverDelegate, const FPayload&);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnDragLeaveDelegate, const FPayload&);

	FAssetViewDragAndDropExtender(FOnDropDelegate InOnDropDelegate)
		: OnDropDelegate(MoveTemp(InOnDropDelegate))
		, OnDragOverDelegate()
		, OnDragLeaveDelegate()
	{ }

	FAssetViewDragAndDropExtender(FOnDropDelegate InOnDropDelegate, FOnDragOverDelegate InOnDragOverDelegate)
		: OnDropDelegate(MoveTemp(InOnDropDelegate))
		, OnDragOverDelegate(MoveTemp(InOnDragOverDelegate))
		, OnDragLeaveDelegate()
	{ }

	FAssetViewDragAndDropExtender(FOnDropDelegate InOnDropDelegate, FOnDragOverDelegate InOnDragOverDelegate, FOnDragLeaveDelegate InOnDragLeaveDelegate)
		: OnDropDelegate(MoveTemp(InOnDropDelegate))
		, OnDragOverDelegate(MoveTemp(InOnDragOverDelegate))
		, OnDragLeaveDelegate(MoveTemp(InOnDragLeaveDelegate))
	{ }

	FOnDropDelegate OnDropDelegate;
	FOnDragOverDelegate OnDragOverDelegate;
	FOnDragLeaveDelegate OnDragLeaveDelegate;
};
