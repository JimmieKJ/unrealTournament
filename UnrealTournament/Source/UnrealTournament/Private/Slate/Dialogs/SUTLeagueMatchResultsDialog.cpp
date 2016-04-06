// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTLeagueMatchResultsDialog.h"
#include "../SUWindowsStyle.h"
#include "SUTStyle.h"

#if !UE_SERVER
FText SUTLeagueMatchResultsDialog::LeagueTierToText(int32 Tier)
{
	switch (Tier)
	{
	case 5:
		return NSLOCTEXT("SUTLeagueMatchResultsDialog", "GrandMasterLeague", "Grand Master");
	case 4:
		return NSLOCTEXT("SUTLeagueMatchResultsDialog", "MasterLeague", "Master");
	case 3:
		return NSLOCTEXT("SUTLeagueMatchResultsDialog", "PlatinumLeague", "Platinum");
	case 2:
		return NSLOCTEXT("SUTLeagueMatchResultsDialog", "GoldLeague", "Gold");
	case 1:
		return NSLOCTEXT("SUTLeagueMatchResultsDialog", "SilverLeague", "Silver");
	}

	return NSLOCTEXT("SUTLeagueMatchResultsDialog", "BronzeLeague", "Bronze");
}

FString SUTLeagueMatchResultsDialog::LeagueTierToBrushName(int32 Tier)
{
	switch (Tier)
	{
	case 5:
	case 4:
		return L"UT.RankedMaster";
	case 3:
		return L"UT.RankedPlatinum";
	case 2:
		return L"UT.RankedGold";
	case 1:
		return L"UT.RankedSilver";
	}

	return L"UT.RankedBronze";
}

void SUTLeagueMatchResultsDialog::Construct(const FArguments& InArgs)
{
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
		.PlayerOwner(InArgs._PlayerOwner)
		.DialogTitle(InArgs._DialogTitle)
		.DialogSize(InArgs._DialogSize)
		.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
		.DialogPosition(InArgs._DialogPosition)
		.DialogAnchorPoint(InArgs._DialogAnchorPoint)
		.ContentPadding(InArgs._ContentPadding)
		.ButtonMask(InArgs._ButtonMask)
		.OnDialogResult(InArgs._OnDialogResult)
		);


	FText PlacementText = FText::Format(NSLOCTEXT("SUTLeagueMatchResultsDialog", "3v3ShowdownPlacement", "{0} {1}"), LeagueTierToText(InArgs._Tier), FText::AsNumber(InArgs._Division + 1));

	if (DialogContent.IsValid())
	{
		DialogContent->AddSlot()
		[
			// I would really love for this content to be centered vertically, but I can't figure out the proper slate
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.FillHeight(1.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SRichTextBlock)
					.TextStyle(SUWindowsStyle::Get(), *InArgs._MessageTextStyleName)
					.Justification(ETextJustify::Center)
					.DecoratorStyleSet(&SUWindowsStyle::Get())
					.AutoWrapText(true)
					.Text(InArgs._MessageText)
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush(*LeagueTierToBrushName(InArgs._Tier)))
					]
					+ SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.MaxHeight(128)
						[
							SNew(STextBlock)
							.Text(FText::AsNumber(InArgs._Division + 1))
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large.Bold")
							.ColorAndOpacity(FLinearColor::White)
						]
					]
				]
				+SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(PlacementText)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
					.ColorAndOpacity(FLinearColor::Gray)
				]
			]
		];
	}
}
#endif