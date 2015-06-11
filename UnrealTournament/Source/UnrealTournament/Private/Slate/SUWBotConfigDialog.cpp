// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWindowsStyle.h"
#include "SUWBotConfigDialog.h"
#include "UTBotCharacter.h"

#if !UE_SERVER

void SUWBotConfigDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments()
		.PlayerOwner(InArgs._PlayerOwner)
		.DialogTitle(InArgs._DialogTitle)
		.DialogSize(InArgs._DialogSize)
		.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
		.DialogPosition(InArgs._DialogPosition)
		.DialogAnchorPoint(InArgs._DialogAnchorPoint)
		.ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
		);

	GameClass = InArgs._GameClass;
	MaxSelectedBots = InArgs._NumBots;

	const TArray<FSelectedBot>& SelectedBots = GameClass.GetDefaultObject()->SelectedBots;

	TArray<FAssetData> Assets;
	GetAllAssetData(UUTBotCharacter::StaticClass(), Assets);
	for (const FAssetData& Asset : Assets)
	{
		TSharedPtr<FAssetData> AssetCopy = MakeShareable(new FAssetData(Asset));
		if (SelectedBots.ContainsByPredicate([&](const FSelectedBot& TestItem) { return TestItem.BotAsset == FStringAssetReference(AssetCopy->ObjectPath.ToString()); }))
		{
			IncludedBots.Add(AssetCopy);
		}
		else
		{
			AvailableBots.Add(AssetCopy);
		}
	}

	DialogContent->AddSlot()
	[
		SNew(SBorder)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.BorderImage(SUWindowsStyle::Get().GetBrush("UWindows.Standard.MenuBar.Background"))
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.WidthOverride(700)
					[
						SNew(SVerticalBox)
						// Heading
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0, 0, 0, 20))
						[
							SNew(STextBlock)
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.BoldText")
							.Text(NSLOCTEXT("SUWBotConfigDialog", "Bots", "Bots:"))
						]
						+ SVerticalBox::Slot()
						[
							SNew(SGridPanel)
							// available bots
							+ SGridPanel::Slot(0, 0)
							[
								SNew(SBorder)
								[
									SNew(SBorder)
									.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Background.Dark"))
									.Padding(FMargin(5))
									[
										SNew(SBox)
										.WidthOverride(300)
										[
											SAssignNew(BotAvailableList, SListView< TSharedPtr<FAssetData> >)
											.SelectionMode(ESelectionMode::Single)
											.ListItemsSource(&AvailableBots)
											.OnGenerateRow(this, &SUWBotConfigDialog::GenerateBotListRow)
										]
									]
								]
							]
							// switch buttons
							+ SGridPanel::Slot(1, 0)
							[
								SNew(SBox)
								.VAlign(VAlign_Center)
								.Padding(FMargin(10, 0, 10, 0))
								[
									SNew(SVerticalBox)
									// Add button
									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(FMargin(0, 0, 0, 10))
									[
										SNew(SButton)
										.HAlign(HAlign_Left)
										.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
										.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
										.OnClicked(this, &SUWBotConfigDialog::AddBot)
										[
											SNew(STextBlock)
											.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText.Black")
											.Text(NSLOCTEXT("SUWBotConfigDialog", "BotAdd", "-->"))
										]
									]
									// Remove Button
									+ SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SButton)
										.HAlign(HAlign_Left)
										.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
										.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
										.OnClicked(this, &SUWBotConfigDialog::RemoveBot)
										[
											SNew(STextBlock)
											.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText.Black")
											.Text(NSLOCTEXT("SUWBotConfigDialog", "BotRemove", "<--"))
										]
									]
								]
							]
							// Enabled bots
							+ SGridPanel::Slot(2, 0)
							[
								SNew(SBorder)
								[
									SNew(SBorder)
									.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Background.Dark"))
									.Padding(FMargin(5))
									[
										SNew(SBox)
										.WidthOverride(300)
										[
											SAssignNew(BotIncludeList, SListView< TSharedPtr<FAssetData> >)
											.SelectionMode(ESelectionMode::Single)
											.ListItemsSource(&IncludedBots)
											.OnGenerateRow(this, &SUWBotConfigDialog::GenerateBotListRow)
										]
									]
								]
							]
						]
					]
				]
			]
		]
	];
}

TSharedRef<ITableRow> SUWBotConfigDialog::GenerateBotListRow(TSharedPtr<FAssetData> BotEntry, const TSharedRef<STableViewBase>& OwningList)
{
	return SNew(STableRow< TSharedPtr<FAssetData> >, OwningList)
		.Padding(5)
		[
			SNew(STextBlock)
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.Text(FText::FromName(BotEntry.IsValid() ? BotEntry->AssetName : NAME_None))
		];
}

FReply SUWBotConfigDialog::AddBot()
{
	TArray< TSharedPtr<FAssetData> > SelectedItems = BotAvailableList->GetSelectedItems();
	if (SelectedItems.Num() > 0 && SelectedItems[0].IsValid())
	{
		AvailableBots.Remove(SelectedItems[0]);
		IncludedBots.Add(SelectedItems[0]);
		BotAvailableList->RequestListRefresh();
		BotIncludeList->RequestListRefresh();
	}
	return FReply::Handled();
}
FReply SUWBotConfigDialog::RemoveBot()
{
	TArray< TSharedPtr<FAssetData> > SelectedItems = BotIncludeList->GetSelectedItems();
	if (SelectedItems.Num() > 0 && SelectedItems[0].IsValid())
	{
		IncludedBots.Remove(SelectedItems[0]);
		AvailableBots.Add(SelectedItems[0]);
		BotAvailableList->RequestListRefresh();
		BotIncludeList->RequestListRefresh();
	}
	return FReply::Handled();
}

FReply SUWBotConfigDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK)
	{
		AUTGameMode* DefaultGame = GameClass.GetDefaultObject();
		DefaultGame->SelectedBots.Empty(IncludedBots.Num());
		for (TSharedPtr<FAssetData> Bot : IncludedBots)
		{
			if (Bot.IsValid())
			{
				new(DefaultGame->SelectedBots) FSelectedBot(Bot->ObjectPath.ToString(), 255);
			}
		}
	}
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

#endif