// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"
#include "DragDropHandler.h"

#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/AssetPathDragDropOp.h"
#include "ContentBrowserUtils.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

bool DragDropHandler::ValidateDragDropOnAssetFolder(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, const FString& TargetPath, bool& OutIsKnownDragOperation)
{
	OutIsKnownDragOperation = false;

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return false;
	}

	bool bIsValidDrag = false;
	TOptional<EMouseCursor::Type> NewDragCursor;

	const bool bIsAssetPath = !ContentBrowserUtils::IsClassPath(TargetPath);

	if (Operation->IsOfType<FAssetDragDropOp>())
	{
		TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);

		OutIsKnownDragOperation = true;

		if (bIsAssetPath)
		{
			int32 NumAssetItems, NumClassItems;
			ContentBrowserUtils::CountItemTypes(DragDropOp->AssetData, NumAssetItems, NumClassItems);

			if (NumAssetItems > 0)
			{
				const FText MoveOrCopyText = (NumAssetItems > 1)
					? FText::Format(LOCTEXT("OnDragAssetsOverFolder_MultipleAssets", "Move or copy '{0}' and {1} other(s)"), FText::FromName(DragDropOp->AssetData[0].AssetName), FText::AsNumber(NumAssetItems - 1))
					: FText::Format(LOCTEXT("OnDragAssetsOverFolder_SingularAsset", "Move or copy '{0}'"), FText::FromName(DragDropOp->AssetData[0].AssetName));

				if (NumClassItems > 0)
				{
					DragDropOp->SetToolTip(FText::Format(LOCTEXT("OnDragAssetsOverFolder_AssetsAndClasses", "{0}\n\n{1} C++ class(es) will be ignored as they cannot be moved or copied"), MoveOrCopyText, FText::AsNumber(NumClassItems)), FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OKWarn")));
					bIsValidDrag = true;
				}
				else
				{
					DragDropOp->SetToolTip(MoveOrCopyText, FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")));
					bIsValidDrag = true;
				}
			}
			else if (NumClassItems > 0)
			{
				DragDropOp->SetToolTip(LOCTEXT("OnDragAssetsOverFolder_OnlyClasses", "C++ classes cannot be moved or copied"), FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
			}
		}
		else
		{
			DragDropOp->SetToolTip(FText::Format(LOCTEXT("OnDragAssetsOverFolder_InvalidFolder", "'{0}' is not a valid place to drop assets"), FText::FromString(TargetPath)), FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
		}
	}
	else if (Operation->IsOfType<FAssetPathDragDropOp>())
	{
		TSharedPtr<FAssetPathDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetPathDragDropOp>(Operation);

		OutIsKnownDragOperation = true;

		if (DragDropOp->PathNames.Num() == 1 && DragDropOp->PathNames[0] == TargetPath)
		{
			DragDropOp->SetToolTip(LOCTEXT("OnDragFoldersOverFolder_CannotSelfDrop", "Cannot move or copy a folder onto itself"), FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
		}
		else if (bIsAssetPath)
		{
			int32 NumAssetPaths, NumClassPaths;
			ContentBrowserUtils::CountPathTypes(DragDropOp->PathNames, NumAssetPaths, NumClassPaths);

			if (NumAssetPaths > 0)
			{
				const FText MoveOrCopyText = (NumAssetPaths > 1)
					? FText::Format(LOCTEXT("OnDragFoldersOverFolder_MultipleFolders", "Move or copy '{0}' and {1} other(s)"), FText::FromString(DragDropOp->PathNames[0]), FText::AsNumber(NumAssetPaths - 1))
					: FText::Format(LOCTEXT("OnDragFoldersOverFolder_SingularFolder", "Move or copy '{0}'"), FText::FromString(DragDropOp->PathNames[0]));

				if (NumClassPaths > 0)
				{
					DragDropOp->SetToolTip(FText::Format(LOCTEXT("OnDragFoldersOverFolder_AssetsAndClasses", "{0}\n\n{1} C++ class folder(s) will be ignored as they cannot be moved or copied"), MoveOrCopyText, FText::AsNumber(NumClassPaths)), FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OKWarn")));
					bIsValidDrag = true;
				}
				else
				{
					DragDropOp->SetToolTip(MoveOrCopyText, FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")));
					bIsValidDrag = true;
				}
			}
			else if (NumClassPaths > 0)
			{
				DragDropOp->SetToolTip(LOCTEXT("OnDragFoldersOverFolder_OnlyClasses", "C++ class folders cannot be moved or copied"), FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
			}
		}
		else
		{
			DragDropOp->SetToolTip(FText::Format(LOCTEXT("OnDragFoldersOverFolder_InvalidFolder", "'{0}' is not a valid place to drop folders"), FText::FromString(TargetPath)), FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
		}
	}
	else if (Operation->IsOfType<FExternalDragOperation>())
	{
		TSharedPtr<FExternalDragOperation> DragDropOp = StaticCastSharedPtr<FExternalDragOperation>(Operation);
		OutIsKnownDragOperation = true;
		bIsValidDrag = DragDropOp->HasFiles() && bIsAssetPath;
	}

	// Set the default slashed circle if this drag is invalid and a drag operation hasn't set NewDragCursor to something custom
	if (!bIsValidDrag && !NewDragCursor.IsSet())
	{
		NewDragCursor = EMouseCursor::SlashedCircle;
	}
	Operation->SetCursorOverride(NewDragCursor);

	return bIsValidDrag;
}

void DragDropHandler::HandleAssetsDroppedOnAssetFolder(const TSharedRef<SWidget>& ParentWidget, const TArray<FAssetData>& AssetList, const FString& TargetPath, const FText& TargetDisplayName, FExecuteCopyOrMoveAssets CopyActionHandler, FExecuteCopyOrMoveAssets MoveActionHandler)
{
	TArray<FAssetData> FinalAssetList;
	FinalAssetList.Reserve(AssetList.Num());

	// Remove any classes from the asset list
	for (const FAssetData& AssetData : AssetList)
	{
		if (AssetData.AssetClass != NAME_Class)
		{
			FinalAssetList.Add(AssetData);
		}
	}

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);
	const FText MoveCopyHeaderString = FText::Format(LOCTEXT("AssetViewDropMenuHeading", "Move/Copy to {0}"), TargetDisplayName);
	MenuBuilder.BeginSection("PathAssetMoveCopy", MoveCopyHeaderString);
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DragDropCopy", "Copy Here"),
			LOCTEXT("DragDropCopyTooltip", "Creates a copy of all dragged files in this folder."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([=]() { CopyActionHandler.ExecuteIfBound(FinalAssetList, TargetPath); }))
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DragDropMove", "Move Here"),
			LOCTEXT("DragDropMoveTooltip", "Moves all dragged files to this folder."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([=]() { MoveActionHandler.ExecuteIfBound(FinalAssetList, TargetPath); }))
			);
	}
	MenuBuilder.EndSection();

	FSlateApplication::Get().PushMenu(
		ParentWidget,
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);
}

void DragDropHandler::HandleFoldersDroppedOnAssetFolder(const TSharedRef<SWidget>& ParentWidget, const TArray<FString>& PathNames, const FString& TargetPath, const FText& TargetDisplayName, FExecuteCopyOrMoveFolders CopyActionHandler, FExecuteCopyOrMoveFolders MoveActionHandler)
{
	TArray<FString> FinalPathNames;
	FinalPathNames.Reserve(PathNames.Num());

	// Remove any class paths from the list
	for (const FString& PathName : PathNames)
	{
		if (!ContentBrowserUtils::IsClassPath(PathName))
		{
			FinalPathNames.Add(PathName);
		}
	}

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);
	MenuBuilder.BeginSection("PathFolderMoveCopy", FText::Format(LOCTEXT("AssetViewDropMenuHeading", "Move/Copy to {0}"), TargetDisplayName));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DragDropCopyFolder", "Copy Folder Here"),
			LOCTEXT("DragDropCopyFolderTooltip", "Creates a copy of all assets in the dragged folders to this folder, preserving folder structure."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([=]() { CopyActionHandler.ExecuteIfBound(FinalPathNames, TargetPath); }))
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DragDropMoveFolder", "Move Folder Here"),
			LOCTEXT("DragDropMoveFolderTooltip", "Moves all assets in the dragged folders to this folder, preserving folder structure."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([=]() { MoveActionHandler.ExecuteIfBound(FinalPathNames, TargetPath); }))
			);
	}
	MenuBuilder.EndSection();

	FSlateApplication::Get().PushMenu(
		ParentWidget,
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);
}

#undef LOCTEXT_NAMESPACE
