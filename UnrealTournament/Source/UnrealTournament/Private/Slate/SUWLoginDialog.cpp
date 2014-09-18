// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWLoginDialog.h"
#include "SUWindowsStyle.h"

#if !UE_SERVER

void SUWLoginDialog::Construct(const FArguments& InArgs)
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
							.OnDialogResult(InArgs._OnDialogResult)
						);

	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		TSharedPtr<SVerticalBox> VBox;
		DialogContent->AddSlot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SImage )		
				.Image(SUWindowsStyle::Get().GetBrush("UWindows.Logos.Epic_Logo200"))
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				SAssignNew(VBox,SVerticalBox)
			]
		];

		VBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(10.0f,5.0f,10.0f,5.0f))
			[ 
				SNew(STextBlock)
				.Text(NSLOCTEXT("MCPMessages","EpicID","Forum Email:"))
				.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.TextStyle")
				.AutoWrapText(true)
			];

		VBox->AddSlot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Fill)
			.Padding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
			[
				SAssignNew(UserEditBox, SEditableTextBox)
				.MinDesiredWidth(300.0f)
				.Text(FText::FromString(InArgs._UserIDText))
			];

		VBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(10.0f,5.0f,10.0f,5.0f))
			[ 
				SNew(STextBlock)
				.Text(NSLOCTEXT("MCPMessages","EpicPass","Password:"))
				.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.TextStyle")
				.AutoWrapText(true)
			];

		VBox->AddSlot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Fill)
			.Padding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
			[
				SAssignNew(PassEditBox, SEditableTextBox)
				.IsPassword(true)
				.MinDesiredWidth(300.0f)
			];


		if (!InArgs._ErrorText.IsEmpty())
		{
			VBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(10.0f,5.0f,10.0f,5.0f))
			[ 
				SNew(STextBlock)
				.Text(InArgs._ErrorText)
				.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.ErrorTextStyle")
				.AutoWrapText(true)
			];
		}

		TabTable.Insert(PassEditBox,0);
		TabTable.Insert(UserEditBox,0);
	}

}

void SUWLoginDialog::OnDialogOpened()
{
	SUWDialog::OnDialogOpened();

	if (UserEditBox->GetText().IsEmpty())
	{
		FSlateApplication::Get().SetKeyboardFocus(UserEditBox, EKeyboardFocusCause::Keyboard);
	}
	else
	{
		FSlateApplication::Get().SetKeyboardFocus(PassEditBox, EKeyboardFocusCause::Keyboard);
	}
}


FString SUWLoginDialog::GetEpicID()
{
	return UserEditBox->GetText().ToString();
}

FString SUWLoginDialog::GetPassword()
{
	return PassEditBox->GetText().ToString();
}


#endif