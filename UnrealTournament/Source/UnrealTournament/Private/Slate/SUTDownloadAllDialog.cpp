// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUTDownloadAllDialog.h"
#include "SUWindowsStyle.h"
#include "SUTStyle.h"

#if !UE_SERVER

void SUTDownloadAllDialog::Construct(const FArguments& InArgs)
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
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().FillWidth(1.0)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUTDownloadAllDialog","ReceivingList","Receiving Content list..."))
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				]
			]
		];
	}
}

FText SUTDownloadAllDialog::GetFilename() const
{
	if (PlayerOwner.IsValid())
	{
		return FText::FromString(PlayerOwner->Download_CurrentFile);
	}

	return NSLOCTEXT("SUTDownloadAllDialog","UnknowFile","Unknown File...");
}

void SUTDownloadAllDialog::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if ( bActive && !PlayerOwner->IsDownloadInProgress() )
	{
		Done();
	}
}

void SUTDownloadAllDialog::OnDialogClosed()
{
	if (PlayerOwner->IsDownloadInProgress())
	{
		PlayerOwner->CancelDownload();
	}
	PlayerOwner->CloseDownloadAll();
}

void SUTDownloadAllDialog::Start()
{

	MessageBox->ClearChildren();
	MessageBox->AddSlot()
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
	];

	bActive = true;
}

void SUTDownloadAllDialog::Done()
{
	MessageBox->ClearChildren();
	MessageBox->AddSlot()
	.FillHeight(1.0)
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	.Padding(FMargin(10.0f,5.0f,10.0f,5.0f))
	[ 
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(1.0)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("SUTDownloadAllDialog","HaveAllContent","You have all of the content on this server."))
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
		]
	];


	bActive = false;
	bDone = true;
}

FText SUTDownloadAllDialog::GetNumFilesLeft() const
{
	return FText::Format(NSLOCTEXT("SUTDownloadAllDialog","NumFilesFormat","{0} files left in queue."), FText::AsNumber(PlayerOwner.IsValid() ? PlayerOwner->Download_NumFilesLeft-1 : 0));
}

TOptional<float> SUTDownloadAllDialog::GetProgress() const
{
	return PlayerOwner.IsValid() ? PlayerOwner->Download_Percentage : 0;
}

#endif