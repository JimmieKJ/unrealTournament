// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWMessageBox.h"
#include "SUWindowsStyle.h"
#include "SlateWordWrapper.h"

#if !UE_SERVER

void SUWMessageBox::Construct(const FArguments& InArgs)
{
	// Let the Dialog construct itself.
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

	// At this point, the DialogContent should be ready to have slots added.

	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SAssignNew(MessageTextBlock, STextBlock)
			.Text(InArgs._MessageText)
			.TextStyle(SUWindowsStyle::Get(), *InArgs._MessageTextStyleName)
		];

		// we have to manually apply the wrapping because SlateWordWrapper::WrapText() has a bug where it discards multi-line breaks and replaces with a single line break
		FWordWrapper::FWrappedLineData WrapData;
		SlateWordWrapper::WrapText(InArgs._MessageText.ToString(), SUWindowsStyle::Get().GetWidgetStyle<FTextBlockStyle>(*InArgs._MessageTextStyleName).Font, ActualSize.X - MessageTextPaddingX * 2.0f - 1.0f, 1.0f, &WrapData);
		
		FString WrappedText = InArgs._MessageText.ToString();
		for (int32 i = 0; i < WrapData.Num(); i++)
		{
			WrappedText.InsertAt(WrapData[i].Value + i, TEXT("\n"));
		}
		MessageTextBlock->SetText(FText::FromString(WrappedText));
	}
}

#endif