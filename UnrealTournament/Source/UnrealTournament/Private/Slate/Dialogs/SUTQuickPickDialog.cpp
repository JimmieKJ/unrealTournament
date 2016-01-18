// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTQuickPickDialog.h"
#include "../SUTStyle.h"
#include "../Widgets/SUTButton.h"

#if !UE_SERVER

void SUTQuickPickDialog::Construct(const FArguments& InArgs)
{
	OptionIndex = 0;
	ResultDelegate = InArgs._OnPickResult;
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(FVector2D(600,800))
							.bDialogSizeIsRelative(false)
							.DialogPosition(FVector2D(0.5,0.5))
							.DialogAnchorPoint(FVector2D(0.5,0.5))
							.ContentPadding(FVector2D(10.0f, 5.0f))
							.ButtonMask(UTDIALOG_BUTTON_CANCEL)
							.IsScrollable(true)
						);

	if (DialogContent.IsValid())
	{
		DialogContent->AddSlot()
		[
			SAssignNew(ContentBox, SVerticalBox)
		];
	}

}

void SUTQuickPickDialog::AddOption(TSharedPtr<SWidget> NewOption, bool bLast)
{
	if (ContentBox.IsValid() && NewOption.IsValid())
	{
		ContentBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Fill).Padding(5.0f,20.0f,5.0f, 20.0f)
		[
			SNew(SUTButton)
			.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
			.OnClicked(this, &SUTQuickPickDialog::OnOptionClicked, OptionIndex)
			[
				NewOption.ToSharedRef()
			]
		];

		if (!bLast)
		{
			ContentBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Fill).Padding(25.0f,0.0f,25.0f, 0.0f)
			[
				SNew(SBox)
				.HeightOverride(3)
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
				]
			];
		}
		OptionIndex++;
	}
}
	
FReply SUTQuickPickDialog::OnOptionClicked(int32 PickedIndex)
{
	ResultDelegate.ExecuteIfBound(PickedIndex);
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

#endif