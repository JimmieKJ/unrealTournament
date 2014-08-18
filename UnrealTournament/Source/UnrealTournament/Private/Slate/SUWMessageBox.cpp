// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWMessageBox.h"
#include "SUWindowsStyle.h"
#include "SlateWordWrapper.h"

void SUWMessageBox::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments().PlayerOwner(InArgs._PlayerOwner));

	OnDialogResult = InArgs._OnDialogResult;

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	TSharedPtr<STextBlock> MessageTextBlock;

	ChildSlot
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SBorder)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.BorderImage(SUWindowsStyle::Get().GetBrush("UWindows.Standard.MenuBar.Background"))
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
						.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.MenuBar.Background"))
					]
				]
				+ SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(0.0f, 5.0f, 0.0f, 5.0f)
					.AutoHeight()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::Black)
						.Text(InArgs._MessageTitle)
					]
					+ SVerticalBox::Slot()
					.MaxHeight(ViewportSize.Y * 0.85f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
						[
							SAssignNew(MessageTextBlock, STextBlock)
							.ColorAndOpacity(FLinearColor::Black)
							.Text(InArgs._MessageText)
							//.AutoWrapText(true) // manually wrapped due to bug, see below
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Bottom)
					.HAlign(HAlign_Right)
					.Padding(5.0f, 5.0f, 5.0f, 5.0f)
					[
						BuildButtonBar(InArgs._ButtonsMask)
					]
				]
			]
		];

	// we have to manually apply the wrapping because SlateWordWrapper::WrapText() has a bug where it discards multi-line breaks and replaces with a single line break
	FWordWrapper::FWrappedLineData WrapData;
	SlateWordWrapper::WrapText(InArgs._MessageText.ToString(), FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText").Font, ViewportSize.X * InArgs._MaxWidthPct, 1.0f, &WrapData);
	FString WrappedText = InArgs._MessageText.ToString();
	for (int32 i = 0; i < WrapData.Num(); i++)
	{
		WrappedText.InsertAt(WrapData[i].Value + i * 2, TEXT("\n"));
	}
	MessageTextBlock->SetText(FText::FromString(WrappedText));
}

void SUWMessageBox::BuildButton(TSharedPtr<SUniformGridPanel> Bar, FText ButtonText, uint16 ButtonID, uint32 &ButtonCount)
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
				.OnClicked(this, &SUWMessageBox::OnButtonClick, ButtonID)
			];

		ButtonCount++;
	};
}

TSharedRef<class SWidget> SUWMessageBox::BuildButtonBar(uint16 ButtonMask)
{
	uint32 ButtonCount = 0;

	TSharedPtr<SUniformGridPanel> Bar;
	SAssignNew(Bar,SUniformGridPanel)
		.SlotPadding(FMargin(0.0f,0.0f, 10.0f, 10.0f));

	if ( Bar.IsValid() )
	{
		if (ButtonMask & UTDIALOG_BUTTON_OK) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","OKButton","OK"), UTDIALOG_BUTTON_OK,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_CANCEL) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","CancelButton","Cancel"), UTDIALOG_BUTTON_CANCEL,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_YES) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","YesButton","Yes"), UTDIALOG_BUTTON_YES,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_NO) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","NoButton","No"), UTDIALOG_BUTTON_NO,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_HELP) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","HelpButton","Help"), UTDIALOG_BUTTON_HELP,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_RECONNECT) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","ReconnectButton","Reconnect"), UTDIALOG_BUTTON_RECONNECT,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_EXIT) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","ExitButton","Exit"), UTDIALOG_BUTTON_EXIT,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_QUIT) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","QuitButton","Quit"), UTDIALOG_BUTTON_QUIT,ButtonCount);
		if (ButtonMask & UTDIALOG_BUTTON_VIEW) BuildButton(Bar, NSLOCTEXT("SUWMessageBox","ViewButton","View"), UTDIALOG_BUTTON_QUIT,ButtonCount);
		
	}

	return Bar.ToSharedRef();
}

FReply SUWMessageBox::OnButtonClick(uint16 ButtonID)
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	OnDialogResult.ExecuteIfBound(ButtonID);
	return FReply::Handled();

}

