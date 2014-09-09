// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWInputBox.h"
#include "SUWindowsStyle.h"

void SUWInputBox::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments().PlayerOwner(InArgs._PlayerOwner));

	OnDialogResult = InArgs._OnDialogResult;
	TextFilter = InArgs._TextFilter;

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	FVector2D WindowSize = FVector2D(400, 200); // TODO: scaling
	FVector2D WindowPosition = FVector2D( (ViewportSize.X * 0.5), (ViewportSize.Y * 0.5));

	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(Canvas,SCanvas)
			+SCanvas::Slot()
			.Position(WindowPosition)
			.Size(WindowSize)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.Dialog.Background"))
					]
				]
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.Padding(0.0f,5.0f,0.0f,5.0f)
					.AutoHeight()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(InArgs._MessageTitle)
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.TextStyle")

					]
					+SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Padding(FMargin(10.0f,5.0f,10.0f,5.0f))
					[
						SNew(STextBlock)
						.Text(InArgs._MessageText)
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.Dialog.TextStyle")
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
					[
						SAssignNew(EditBox, SEditableTextBox)
						.OnTextChanged(this, &SUWInputBox::OnTextChanged)
						.OnTextCommitted(this, &SUWInputBox::OnTextCommited)
						.MinDesiredWidth(300.0f)
						.Text(FText::FromString(InArgs._DefaultInput))
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Bottom)
					.HAlign(HAlign_Right)
					.Padding(5.0f,5.0f,5.0f,5.0f)
					[
						BuildButtonBar()
					]

				]
			]
		];
}

void SUWInputBox::OnDialogOpened()
{
	SUWDialog::OnDialogOpened();
	// start with the editbox focused
	FSlateApplication::Get().SetKeyboardFocus(EditBox, EKeyboardFocusCause::Keyboard);
}

void SUWInputBox::BuildButton(TSharedPtr<SUniformGridPanel> Bar, FText ButtonText, uint16 ButtonID, uint32& ButtonCount)
{
	if (Bar.IsValid())
	{
		Bar->AddSlot(ButtonCount,0)
			.HAlign(HAlign_Fill)
//			.Padding(0.0f,0.0f,5.0f,0.0f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
				.Text(ButtonText.ToString())
				.OnClicked(this, &SUWInputBox::OnButtonClick, ButtonID)
			];
		ButtonCount++;
	};
}

TSharedRef<SWidget> SUWInputBox::BuildButtonBar()
{
	uint32 ButtonCount = 0;

	TSharedPtr<SUniformGridPanel> Bar;
	SAssignNew(Bar,SUniformGridPanel)
		.SlotPadding(FMargin(0.0f,0.0f, 10.0f, 10.0f));

	if ( Bar.IsValid() )
	{
		uint32 ButtonCount = 0;
		BuildButton(Bar, NSLOCTEXT("SUWMessageBox","OKButton","OK"), UTDIALOG_BUTTON_OK, ButtonCount);
		BuildButton(Bar, NSLOCTEXT("SUWMessageBox","CancelButton","Cancel"), UTDIALOG_BUTTON_CANCEL, ButtonCount);
	}

	return Bar.ToSharedRef();
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

FReply SUWInputBox::OnButtonClick(uint16 ButtonID)
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	OnDialogResult.ExecuteIfBound(EditBox->GetText().ToString(), ButtonID == UTDIALOG_BUTTON_CANCEL);
	return FReply::Handled();
}

