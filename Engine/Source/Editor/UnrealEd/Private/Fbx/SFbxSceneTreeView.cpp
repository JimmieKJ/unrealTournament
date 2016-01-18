// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "SFbxSceneTreeView.h"
#include "ClassIconFinder.h"

#define LOCTEXT_NAMESPACE "SFbxSceneTreeView"

SFbxSceneTreeView::~SFbxSceneTreeView()
{
	FbxRootNodeArray.Empty();
	SceneInfo = NULL;
}

void SFbxSceneTreeView::Construct(const SFbxSceneTreeView::FArguments& InArgs)
{
	SceneInfo = InArgs._SceneInfo;
	//Build the FbxNodeInfoPtr tree data
	check(SceneInfo.IsValid());
	for (auto NodeInfoIt = SceneInfo->HierarchyInfo.CreateIterator(); NodeInfoIt; ++NodeInfoIt)
	{
		FbxNodeInfoPtr NodeInfo = (*NodeInfoIt);

		if (!NodeInfo->ParentNodeInfo.IsValid())
		{
			FbxRootNodeArray.Add(NodeInfo);
		}
	}

	STreeView::Construct
	(
		STreeView::FArguments()
		.TreeItemsSource(&FbxRootNodeArray)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SFbxSceneTreeView::OnGenerateRowFbxSceneTreeView)
		.OnGetChildren(this, &SFbxSceneTreeView::OnGetChildrenFbxSceneTreeView)
		.OnContextMenuOpening(this, &SFbxSceneTreeView::OnOpenContextMenu)
		.OnSelectionChanged(this, &SFbxSceneTreeView::OnSelectionChanged)
	);
}

/** The item used for visualizing the class in the tree. */
class SFbxNodeItem : public STableRow< FbxNodeInfoPtr >
{
public:

	SLATE_BEGIN_ARGS(SFbxNodeItem)
		: _FbxNodeInfo(NULL)
	{}

	/** The item content. */
	SLATE_ARGUMENT(FbxNodeInfoPtr, FbxNodeInfo)
	SLATE_END_ARGS()

	/**
	* Construct the widget
	*
	* @param InArgs   A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		FbxNodeInfo = InArgs._FbxNodeInfo;
		
		//This is suppose to always be valid
		check(FbxNodeInfo.IsValid());
		UClass *IconClass = AActor::StaticClass();
		if (FbxNodeInfo->AttributeInfo.IsValid())
		{
			IconClass = FbxNodeInfo->AttributeInfo->GetType();
		}
		const FSlateBrush* ClassIcon = FClassIconFinder::FindIconForClass(IconClass);

		this->ChildSlot
		[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SFbxNodeItem::OnItemCheckChanged)
				.IsChecked(this, &SFbxNodeItem::IsItemChecked)
			]
		+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 2.0f, 6.0f, 2.0f)
			[
				SNew(SImage)
				.Image(ClassIcon)
				.Visibility(ClassIcon != FEditorStyle::GetDefaultBrush() ? EVisibility::Visible : EVisibility::Collapsed)
			]

		+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 3.0f, 6.0f, 3.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FbxNodeInfo->NodeName))
				.ToolTipText(FText::FromString(FbxNodeInfo->NodeName))
			]

		];

		STableRow< FbxNodeInfoPtr >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true),
			InOwnerTableView
		);
	}

private:

	void OnItemCheckChanged(ECheckBoxState CheckType)
	{
		if (!FbxNodeInfo.IsValid())
			return;
		FbxNodeInfo->bImportNode = CheckType == ECheckBoxState::Checked;
	}

	ECheckBoxState IsItemChecked() const
	{
		return FbxNodeInfo->bImportNode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/** The node info to build the tree view row from. */
	FbxNodeInfoPtr FbxNodeInfo;
};

TSharedRef< ITableRow > SFbxSceneTreeView::OnGenerateRowFbxSceneTreeView(FbxNodeInfoPtr Item, const TSharedRef< STableViewBase >& OwnerTable)
{
	TSharedRef< SFbxNodeItem > ReturnRow = SNew(SFbxNodeItem, OwnerTable)
		.FbxNodeInfo(Item);
	return ReturnRow;
}

void SFbxSceneTreeView::OnGetChildrenFbxSceneTreeView(FbxNodeInfoPtr InParent, TArray< FbxNodeInfoPtr >& OutChildren)
{
	for (auto Child : InParent->Childrens)
	{
		if (Child.IsValid())
		{
			OutChildren.Add(Child);
		}
	}
}

void RecursiveSetImport(FbxNodeInfoPtr NodeInfoPtr, bool ImportStatus)
{
	NodeInfoPtr->bImportNode = ImportStatus;
	for (auto Child : NodeInfoPtr->Childrens)
	{
		RecursiveSetImport(Child, ImportStatus);
	}
}

void SFbxSceneTreeView::OnToggleSelectAll(ECheckBoxState CheckType)
{
	//check all actor for import
	for (auto NodeInfoIt = SceneInfo->HierarchyInfo.CreateIterator(); NodeInfoIt; ++NodeInfoIt)
	{
		FbxNodeInfoPtr NodeInfo = (*NodeInfoIt);

		if (!NodeInfo->ParentNodeInfo.IsValid())
		{
			RecursiveSetImport(NodeInfo, CheckType == ECheckBoxState::Checked);
		}
	}
}

void RecursiveSetExpand(SFbxSceneTreeView *TreeView, FbxNodeInfoPtr NodeInfoPtr, bool ExpandState)
{
	TreeView->SetItemExpansion(NodeInfoPtr, ExpandState);
	for (auto Child : NodeInfoPtr->Childrens)
	{
		RecursiveSetExpand(TreeView, Child, ExpandState);
	}
}

FReply SFbxSceneTreeView::OnExpandAll()
{
	for (auto NodeInfoIt = SceneInfo->HierarchyInfo.CreateIterator(); NodeInfoIt; ++NodeInfoIt)
	{
		FbxNodeInfoPtr NodeInfo = (*NodeInfoIt);

		if (!NodeInfo->ParentNodeInfo.IsValid())
		{
			RecursiveSetExpand(this, NodeInfo, true);
		}
	}
	return FReply::Handled();
}

FReply SFbxSceneTreeView::OnCollapseAll()
{
	for (auto NodeInfoIt = SceneInfo->HierarchyInfo.CreateIterator(); NodeInfoIt; ++NodeInfoIt)
	{
		FbxNodeInfoPtr NodeInfo = (*NodeInfoIt);

		if (!NodeInfo->ParentNodeInfo.IsValid())
		{
			RecursiveSetExpand(this, NodeInfo, false);
		}
	}
	return FReply::Handled();
}

TSharedPtr<SWidget> SFbxSceneTreeView::OnOpenContextMenu()
{
	// Build up the menu for a selection
	const bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder(bCloseAfterSelection, TSharedPtr<FUICommandList>());

	//Get the different type of the multi selection
	TSet<TSharedPtr<FFbxAttributeInfo>> SelectAssets;
	TArray<FbxNodeInfoPtr> SelectedItems;
	const auto NumSelectedItems = GetSelectedItems(SelectedItems);
	for (auto Item : SelectedItems)
	{
		FbxNodeInfoPtr ItemPtr = Item;
		if (ItemPtr->AttributeInfo.IsValid())
		{
			SelectAssets.Add(ItemPtr->AttributeInfo);
		}
	}

	// We always create a section here, even if there is no parent so that clients can still extend the menu
	MenuBuilder.BeginSection("FbxSceneTreeViewContextMenuImportSection");
	{
		const FSlateIcon PlusIcon(FEditorStyle::GetStyleSetName(), "Plus");
		MenuBuilder.AddMenuEntry(LOCTEXT("CheckForImport", "Add Selection To Import"), FText(), PlusIcon, FUIAction(FExecuteAction::CreateSP(this, &SFbxSceneTreeView::AddSelectionToImport)));
		const FSlateIcon MinusIcon(FEditorStyle::GetStyleSetName(), "PropertyWindow.Button_RemoveFromArray");
		MenuBuilder.AddMenuEntry(LOCTEXT("UncheckForImport", "Remove Selection From Import"), FText(), MinusIcon, FUIAction(FExecuteAction::CreateSP(this, &SFbxSceneTreeView::RemoveSelectionFromImport)));
	}
	MenuBuilder.EndSection();
	if (SelectAssets.Num() > 0)
	{
		MenuBuilder.AddMenuSeparator();
		MenuBuilder.BeginSection("FbxSceneTreeViewContextMenuGotoAssetSection", LOCTEXT("Gotoasset","Go to asset:"));
		{
			for (auto AssetItem : SelectAssets)
			{
				//const FSlateBrush* ClassIcon = FClassIconFinder::FindIconForClass(AssetItem->GetType());
				MenuBuilder.AddMenuEntry(FText::FromString(AssetItem->Name), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SFbxSceneTreeView::GotoAsset, AssetItem)));
			}
		}
		MenuBuilder.EndSection();
	}
	

	return MenuBuilder.MakeWidget();
}

void SFbxSceneTreeView::AddSelectionToImport()
{
	SetSelectionImportState(true);
}

void SFbxSceneTreeView::RemoveSelectionFromImport()
{
	SetSelectionImportState(false);
}

void SFbxSceneTreeView::SetSelectionImportState(bool MarkForImport)
{
	TArray<FbxNodeInfoPtr> SelectedItems;
	GetSelectedItems(SelectedItems);
	for (auto Item : SelectedItems)
	{
		FbxNodeInfoPtr ItemPtr = Item;
		ItemPtr->bImportNode = MarkForImport;
	}
}

void SFbxSceneTreeView::OnSelectionChanged(FbxNodeInfoPtr Item, ESelectInfo::Type SelectionType)
{
}

void SFbxSceneTreeView::GotoAsset(TSharedPtr<FFbxAttributeInfo> AssetAttribute)
{
	//Switch to the asset tab and select the asset
}

#undef LOCTEXT_NAMESPACE
