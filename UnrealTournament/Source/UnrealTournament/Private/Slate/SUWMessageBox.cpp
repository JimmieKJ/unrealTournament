// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWMessageBox.h"
#include "SUWindowsStyle.h"

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
			.WrapTextAt(ActualSize.X - MessageTextPaddingX * 2.0f - 1.0f)
		];
	}
}

#endif