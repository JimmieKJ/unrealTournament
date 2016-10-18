// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTDownloadAllDialog.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "UTLobbyPlayerState.h"

#if !UE_SERVER

void SUTDownloadAllDialog::Construct(const FArguments& InArgs)
{
	bTransitionWhenDone = InArgs._bTransitionWhenDone;
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.IsScrollable(false)
						);

	bActive = false;
	bDone = false;

	if (DialogContent.IsValid())
	{
		DialogContent->AddSlot()
		[
			SAssignNew(MessageBox,SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(10.0f,5.0f,10.0f,5.0f))
			[ 

				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().FillWidth(1.0)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUTDownloadAllDialog","DownloadingFile","Downloading File"))
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().FillWidth(1.0)
					[
						SNew(STextBlock)
						.Text(this, &SUTDownloadAllDialog::GetFilename)
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SProgressBar)
						.Percent(this, &SUTDownloadAllDialog::GetProgress)
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().FillWidth(1.0)
					[
						SNew(STextBlock)
						.Text(this, &SUTDownloadAllDialog::GetNumFilesLeft)
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
					]
				]
			]
		];
	}
}

FText SUTDownloadAllDialog::GetFilename() const
{
	if (PlayerOwner.IsValid())
	{
		return PlayerOwner->GetDownloadFilename();
	}

	return NSLOCTEXT("SUTDownloadAllDialog","UnknowFile","Unknown File...");
}

void SUTDownloadAllDialog::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if ( !PlayerOwner->IsDownloadInProgress() )
	{
		// We are done.. auto hide..
		PlayerOwner->HideRedirectDownload();
	}
}

FText SUTDownloadAllDialog::GetNumFilesLeft() const
{
	return FText::Format(NSLOCTEXT("SUTDownloadAllDialog","NumFilesFormat","{0} files left in queue."), FText::AsNumber(PlayerOwner.IsValid() ? PlayerOwner->GetNumberOfPendingDownloads() - 1 : 0));
}

TOptional<float> SUTDownloadAllDialog::GetProgress() const
{
	return PlayerOwner.IsValid() ? PlayerOwner->GetDownloadProgress() : 0.0f;
}

FReply SUTDownloadAllDialog::OnButtonClick(uint16 ButtonID)
{
	PlayerOwner->CancelDownload();
	return SUTDialogBase::OnButtonClick(ButtonID);
}


#endif