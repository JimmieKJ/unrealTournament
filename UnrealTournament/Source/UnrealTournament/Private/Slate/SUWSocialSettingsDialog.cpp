// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWSocialSettingsDialog.h"
#include "SUWindowsStyle.h"
#include "UTPlayerInput.h"
#include "Scalability.h"
#include "UTWorldSettings.h"
#include "UTGameEngine.h"
#include "SUTUtils.h"

#if !UE_SERVER


void SUWSocialSettingsDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.bShadow(false)
							.OnDialogResult(InArgs._OnDialogResult)
						);

	if (DialogContent.IsValid())
	{
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
			.HAlign(HAlign_Right)
			[
				SAssignNew(SuppressToastsInGame, SCheckBox)
				.ForegroundColor(FLinearColor::White)
				.IsChecked(GetPlayerOwner()->bSuppressToastsInGame ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
				.Content()
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(NSLOCTEXT("SUWSocialSettingsDialog", "SuppressToastsInGame", "Suppress Toasts While In-Game").ToString())
					.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUWSocialSettingsDialog","SuppressToastsInGameTT","Turn this option on to stop toasts from appearing while you are in a multiplayer game.")))
				]
			]
		];
	}
}


FReply SUWSocialSettingsDialog::OKClick()
{
	GetPlayerOwner()->bSuppressToastsInGame = SuppressToastsInGame->IsChecked();
	GetPlayerOwner()->SaveProfileSettings();
	GetPlayerOwner()->CloseDialog(SharedThis(this));

	return FReply::Handled();
}

FReply SUWSocialSettingsDialog::CancelClick()
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}


FReply SUWSocialSettingsDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) OKClick();
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL) CancelClick();
	return FReply::Handled();
}


#endif