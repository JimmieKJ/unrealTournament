// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "SUTProfileItemsDialog.h"
#include "../SUWindowsStyle.h"
#include "UTProfileItem.h"

#if !UE_SERVER

void SUTProfileItemsDialog::Construct(const FArguments& InArgs)
{
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
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

#if WITH_PROFILE
	UUtMcpProfile* Profile = GetPlayerOwner()->GetMcpProfileManager()->GetMcpProfileAs<UUtMcpProfile>(EUtMcpProfile::Profile);
	if (Profile != NULL)
	{
		TArray<TSharedRef<const FMcpItem>> NewItemList;
		Profile->GetItemsByTemplateType(NewItemList, TEXT("Item"));
		for (const TSharedRef<const FMcpItem>& Item : NewItemList)
		{
			UUTProfileItem* ProfileItem = Cast<UUTProfileItem>(Item->Instance);
			if (ProfileItem != NULL)
			{
				Items.Add(MakeShareable(new FProfileItemEntry(ProfileItem, Item->Quantity)));
			}
		}
	}
#endif

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
						.OnGenerateRow(this, &SUTProfileItemsDialog::GenerateItemListRow)
					]
				]
			]
		];
	}
}

TSharedRef<ITableRow> SUTProfileItemsDialog::GenerateItemListRow(TSharedPtr<FProfileItemEntry> TheItem, const TSharedRef<STableViewBase>& OwningList)
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
				.Text(TheItem->Item != NULL ? TheItem->Item->DisplayName : NSLOCTEXT("SUTProfileItemsDialog", "Unknown", "Unknown"))
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