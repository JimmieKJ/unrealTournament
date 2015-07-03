// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "../Public/UnrealTournament.h"
#include "SUWProfileItemsDialog.h"
#include "SUWindowsStyle.h"
#include "../Public/UTProfileItem.h"

#if !UE_SERVER

void SUWProfileItemsDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments()
		.PlayerOwner(InArgs._PlayerOwner)
		.DialogTitle(NSLOCTEXT("SUWindowsDesktop", "CollectableItems", "Collectable Items"))
		.DialogSize(InArgs._DialogSize)
		.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
		.DialogPosition(InArgs._DialogPosition)
		.DialogAnchorPoint(InArgs._DialogAnchorPoint)
		.ContentPadding(InArgs._ContentPadding)
		.ButtonMask(UTDIALOG_BUTTON_OK)
		.OnDialogResult(InArgs._OnDialogResult)
		);

	const TArray<FProfileItemEntry> ProfileItems = GetPlayerOwner()->GetProfileItems();
	for (const FProfileItemEntry& Item : ProfileItems)
	{
		Items.Add(MakeShareable(new FProfileItemEntry(Item)));
	}

	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 10.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Background.Dark"))
				.Padding(FMargin(5))
				[
					SNew(SBox)
					.WidthOverride(650.0f)
					[
						SAssignNew(ItemList, SListView< TSharedPtr<FProfileItemEntry> >)
						.SelectionMode(ESelectionMode::Single)
						.ListItemsSource(&Items)
						.OnGenerateRow(this, &SUWProfileItemsDialog::GenerateItemListRow)
					]
				]
			]
		];
	}
}

TSharedRef<ITableRow> SUWProfileItemsDialog::GenerateItemListRow(TSharedPtr<FProfileItemEntry> TheItem, const TSharedRef<STableViewBase>& OwningList)
{
	return SNew(STableRow< TSharedPtr<FProfileItemEntry> >, OwningList)
		.Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 10.0f, 0.0f, 5.0f)
			.FillWidth(0.75)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(TheItem->Item != NULL ? TheItem->Item->DisplayName : NSLOCTEXT("SUWProfileItemsDialog", "Unknown", "Unknown"))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
			]
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 10.0f, 0.0f, 5.0f)
			.FillWidth(0.25)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(TheItem->Count))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
			]
		];
}

#endif