// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTMessageBoxDialog.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

void SUTMessageBoxDialog::Construct(const FArguments& InArgs)
{
	// These must be set before the parent Construct is called.
	bIsSuppressible = InArgs._IsSuppressible;

	OnSuppressibleCheckStateChanged = InArgs._OnSuppressibleCheckStateChanged;
	SuppressibleCheckBoxState = InArgs._SuppressibleCheckBoxState;

	// Let the Dialog construct itself.
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



	// At this point, the DialogContent should be ready to have slots added.
	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 50.0f;
		TSharedPtr<SRichTextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[			
				SAssignNew(MessageTextBlock, SRichTextBlock)
				.TextStyle(SUWindowsStyle::Get(), *InArgs._MessageTextStyleName)
				.Justification(ETextJustify::Center)
				.DecoratorStyleSet(&SUWindowsStyle::Get())
				.AutoWrapText(true)
				.Text(InArgs._MessageText)
			]
		];
	}
}

TSharedRef<SWidget> SUTMessageBoxDialog::BuildCustomButtonBar()
{
	if( bIsSuppressible )
	{
		return 
			SNew(SCheckBox)
			.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
			.ForegroundColor(FLinearColor::White)
			.IsChecked(SuppressibleCheckBoxState)
			.OnCheckStateChanged(OnSuppressibleCheckStateChanged)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("SUTMessageBoxDialog", "SuppressibleMessage", "Don't show this again"))
			];
	}
	else
	{
		return SUTDialogBase::BuildCustomButtonBar();
	}
}

#endif