// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTMatchmakingRegionDialog.h"
#include "UTGameInstance.h"
#include "UTParty.h"
#include "UTPartyGameState.h"
#include "../SUWindowsStyle.h"
#include "../SUTUtils.h"

#if !UE_SERVER

void SUTMatchmakingRegionDialog::Construct(const FArguments& InArgs)
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

	MatchmakingRegionList.Add(MakeShareable(new FString(TEXT("NA"))));
	MatchmakingRegionList.Add(MakeShareable(new FString(TEXT("EU"))));

	if (DialogContent.IsValid())
	{		
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(650)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(NSLOCTEXT("SUTSystemSettingsDialog", "MatchmakingRegion", "Matchmaking Region"))
						.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTMatchmakingRegionDialog", "MatchmakingRegion_Tooltip", "Which region that ranked matchmaking will use.")))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(MatchmakingRegion, SComboBox< TSharedPtr<FString> >)
					.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
					.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
					.OptionsSource(&MatchmakingRegionList)
					.OnGenerateWidget(this, &SUTDialogBase::GenerateStringListWidget)
					.OnSelectionChanged(this, &SUTMatchmakingRegionDialog::OnMatchmakingRegionSelected)
					.InitiallySelectedItem(MatchmakingRegionList[0])
					.Content()
					[
						SAssignNew(SelectedMatchmakingRegion, STextBlock)
						.Text(FText::FromString(*MatchmakingRegionList[0]))
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
					]
				]
			]
		];
	}
}

void SUTMatchmakingRegionDialog::OnMatchmakingRegionSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedMatchmakingRegion->SetText(*NewSelection.Get());
}

FReply SUTMatchmakingRegionDialog::OnButtonClick(uint16 ButtonID)
{
	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();
	if (ProfileSettings)
	{
		ProfileSettings->MatchmakingRegion = *MatchmakingRegion->GetSelectedItem();
		GetPlayerOwner()->SaveProfileSettings();
	}

	OnDialogResult.ExecuteIfBound(SharedThis(this), ButtonID);
	GetPlayerOwner()->CloseDialog(SharedThis(this));

	return FReply::Handled();
}

#endif