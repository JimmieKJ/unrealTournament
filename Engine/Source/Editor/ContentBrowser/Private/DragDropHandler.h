// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Common Content Browser drag-drop handler logic */
namespace DragDropHandler
{
	DECLARE_DELEGATE_TwoParams(FExecuteCopyOrMoveAssets, TArray<FAssetData> /*AssetList*/, FString /*TargetPath*/);
	DECLARE_DELEGATE_TwoParams(FExecuteCopyOrMoveFolders, TArray<FString> /*PathNames*/, FString /*TargetPath*/);

	/** Used by OnDragEnter, OnDragOver, and OnDrop to check and update the validity of a drag-drop operation on an asset folder in the Content Browser */
	bool ValidateDragDropOnAssetFolder(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, const FString& TargetPath, bool& OutIsKnownDragOperation);

	/** Handle assets being dropped onto an asset folder in the Content Browser - this drop should have been externally validated by ValidateDragDropOnAssetFolder */
	void HandleAssetsDroppedOnAssetFolder(const TSharedRef<SWidget>& ParentWidget, const TArray<FAssetData>& AssetList, const FString& TargetPath, const FText& TargetDisplayName, FExecuteCopyOrMoveAssets CopyActionHandler, FExecuteCopyOrMoveAssets MoveActionHandler);

	/** Handle folders being dropped onto an asset folder in the Content Browser - this drop should have been externally validated by ValidateDragDropOnAssetFolder */
	void HandleFoldersDroppedOnAssetFolder(const TSharedRef<SWidget>& ParentWidget, const TArray<FString>& PathNames, const FString& TargetPath, const FText& TargetDisplayName, FExecuteCopyOrMoveFolders CopyActionHandler, FExecuteCopyOrMoveFolders MoveActionHandler);
}
