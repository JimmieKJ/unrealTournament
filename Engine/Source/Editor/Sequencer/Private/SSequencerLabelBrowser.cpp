// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SSequencerLabelListRow.h"


#define LOCTEXT_NAMESPACE "SSequencerLabelBrowser"


/* SSequencerLabelBrowser structors
 *****************************************************************************/

SSequencerLabelBrowser::~SSequencerLabelBrowser()
{
}


/* SSequencerLabelBrowser interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SSequencerLabelBrowser::Construct(const FArguments& InArgs)
{
	OnSelectionChanged = InArgs._OnSelectionChanged;

	ChildSlot
	[
		SAssignNew(LabelTreeView, STreeView<TSharedPtr<FSequencerLabelTreeNode>>)
			.ItemHeight(20.0f)
			.OnContextMenuOpening(this, &SSequencerLabelBrowser::HandleLabelTreeViewContextMenuOpening)
			.OnGenerateRow(this, &SSequencerLabelBrowser::HandleLabelTreeViewGenerateRow)
			.OnGetChildren(this, &SSequencerLabelBrowser::HandleLabelTreeViewGetChildren)
			.OnSelectionChanged(this, &SSequencerLabelBrowser::HandleLabelTreeViewSelectionChanged)
			.SelectionMode(ESelectionMode::Single)
			.TreeItemsSource(&LabelList)
	];

	ReloadLabelList(true);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


/* SSequencerLabelBrowser implementation
 *****************************************************************************/

void SSequencerLabelBrowser::ReloadLabelList(bool FullyReload)
{
	LabelList.Reset();

	// test
	LabelList.Add(MakeShareable(new FSequencerLabelTreeNode(TEXT(""))));

	LabelTreeView->RequestTreeRefresh();
}


/* SSequencerLabelBrowser callbacks
 *****************************************************************************/

TSharedPtr<SWidget> SSequencerLabelBrowser::HandleLabelTreeViewContextMenuOpening()
{
	FMenuBuilder MenuBuilder(true /*bInShouldCloseWindowAfterMenuSelection*/, nullptr);
	{
		MenuBuilder.BeginSection("Customize", LOCTEXT("CustomizeContextMenuSectionName", "Customize"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("SetColorMenuEntryLabel", "Set Color"),
				LOCTEXT("SetColorMenuEntryTip", "Set the background color of this label"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SSequencerLabelBrowser::HandleSetColorMenuEntryExecute),
					FCanExecuteAction::CreateSP(this, &SSequencerLabelBrowser::HandleSetColorMenuEntryCanExecute)
				)
			);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("Edit", LOCTEXT("EditContextMenuSectionName", "Edit"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("RemoveLabelMenuEntryLabel", "Remove"),
				LOCTEXT("RemoveLabelMenuEntryTip", "Remove this label from this list and all tracks"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SSequencerLabelBrowser::HandleRemoveLabelMenuEntryExecute),
					FCanExecuteAction::CreateSP(this, &SSequencerLabelBrowser::HandleRemoveLabelMenuEntryCanExecute)
				)
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("RenameLabelMenuEntryLabel", "Rename"),
				LOCTEXT("RenameLabelMenuEntryTip", "Change the name of this label"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SSequencerLabelBrowser::HandleRenameLabelMenuEntryExecute),
					FCanExecuteAction::CreateSP(this, &SSequencerLabelBrowser::HandleRenameLabelMenuEntryCanExecute)
				)
			);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}


 TSharedRef<ITableRow> SSequencerLabelBrowser::HandleLabelTreeViewGenerateRow(TSharedPtr<FSequencerLabelTreeNode> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SSequencerLabelListRow, OwnerTable)
		.Node(Item);
}


void SSequencerLabelBrowser::HandleLabelTreeViewGetChildren(TSharedPtr<FSequencerLabelTreeNode> Item, TArray<TSharedPtr<FSequencerLabelTreeNode>>& OutChildren)
{
	if (Item.IsValid())
	{
		OutChildren = Item->GetChildren();
	}
}


void SSequencerLabelBrowser::HandleLabelTreeViewSelectionChanged(TSharedPtr<FSequencerLabelTreeNode> InItem, ESelectInfo::Type SelectInfo)
{
	OnSelectionChanged.ExecuteIfBound(
		InItem.IsValid()
			? InItem->GetLabel()
			: FString(),
		SelectInfo
	);
}


void SSequencerLabelBrowser::HandleRemoveLabelMenuEntryExecute()
{
}


bool SSequencerLabelBrowser::HandleRemoveLabelMenuEntryCanExecute() const
{
	return false;
}


void SSequencerLabelBrowser::HandleRenameLabelMenuEntryExecute()
{
}


bool SSequencerLabelBrowser::HandleRenameLabelMenuEntryCanExecute() const
{
	return false;
}


void SSequencerLabelBrowser::HandleSetColorMenuEntryExecute()
{
}


bool SSequencerLabelBrowser::HandleSetColorMenuEntryCanExecute() const
{
	return false;
}


#undef LOCTEXT_NAMESPACE
