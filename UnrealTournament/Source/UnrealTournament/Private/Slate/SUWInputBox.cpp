// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWInputBox.h"
#include "SUWindowsStyle.h"

#if !UE_SERVER

void SUWInputBox::Construct(const FArguments& InArgs)
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
							.IsScrollable(InArgs._IsScrollable)
						);
	TextFilter = InArgs._TextFilter;
	IsPassword = InArgs._IsPassword;

	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(10.0f,5.0f,10.0f,5.0f))
			[ 
				SNew(STextBlock)
				.Text(InArgs._MessageText)
				.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.TextStyle")
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Fill)
			.Padding(FMargin(10.0f, 0.0f, 10.0f, 5.0f))
			[
				SAssignNew(EditBox, SEditableTextBox)
				.Style(SUWindowsStyle::Get(), "UT.Common.Editbox.Dark")
				.OnTextChanged(this, &SUWInputBox::OnTextChanged)
				.OnTextCommitted(this, &SUWInputBox::OnTextCommited)
				.IsPassword(IsPassword)
				.MinDesiredWidth(300.0f)
				.Text(FText::FromString(InArgs._DefaultInput))
			]
		];
	}

}

void SUWInputBox::OnDialogOpened()
{
	SUWDialog::OnDialogOpened();
	// start with the editbox focused
	FSlateApplication::Get().SetKeyboardFocus(EditBox, EKeyboardFocusCause::Keyboard);
}


void SUWInputBox::OnTextChanged(const FText& NewText)
{
	if (TextFilter.IsBound())
	{
		FString Result = NewText.ToString();
		for (int32 i = Result.Len() - 1; i >= 0; i--)
		{
			if (!TextFilter.Execute(Result.GetCharArray()[i]))
			{
				Result.GetCharArray().RemoveAt(i);
			}
		}
		EditBox->SetText(FText::FromString(Result));
	}
}

void SUWInputBox::OnTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		OnButtonClick(UTDIALOG_BUTTON_OK);
	}
}

FString SUWInputBox::GetInputText()
{
	if (EditBox.IsValid())
	{
		return EditBox->GetText().ToString();
	}
	return TEXT("");
}

#endif