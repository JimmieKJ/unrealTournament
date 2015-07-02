// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUWVideoCompressionDialog.h"
#include "SUWindowsStyle.h"
#include "UTVideoRecordingFeature.h"

#if !UE_SERVER
void SUWVideoCompressionDialog::Construct(const FArguments& InArgs)
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
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock).Text(NSLOCTEXT("SUWVideoCompressionDialog", "Compressing", "Compressing"))
				]
			]			
			+ SVerticalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			//.AutoHeight()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SProgressBar)
					.Percent(this, &SUWVideoCompressionDialog::GetProgressCompression)
				]
			]
		];
	}

	VideoRecorder = InArgs._VideoRecorder;
	VideoRecorder->StartCompressing(InArgs._VideoFilename);
	VideoRecorder->OnCompressingComplete().AddRaw(this, &SUWVideoCompressionDialog::CompressingComplete);
}

FReply SUWVideoCompressionDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_CANCEL)
	{
		// kill the compression here
		//VideoRecorder->CancelCompression();

		OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_CANCEL);
		GetPlayerOwner()->CloseDialog(SharedThis(this));
	}
	return FReply::Handled();
}

TOptional<float> SUWVideoCompressionDialog::GetProgressCompression() const
{
	return VideoRecorder->GetCompressionCompletionPercent();
}

void SUWVideoCompressionDialog::CompressingComplete()
{
	OnDialogResult.ExecuteIfBound(SharedThis(this), UTDIALOG_BUTTON_OK);
	GetPlayerOwner()->CloseDialog(SharedThis(this));
}
#endif