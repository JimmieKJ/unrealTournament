// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWScreenshotConfigDialog.h"
#include "SUWindowsStyle.h"

#if !UE_SERVER

#define LOCTEXT_NAMESPACE "SUWScreenshotConfigDialog"

void SUWScreenshotConfigDialog::Construct(const FArguments& InArgs)
{
	// Let the Dialog construct itself.
	SUWDialog::Construct(SUWDialog::FArguments()
		.PlayerOwner(InArgs._PlayerOwner)
		.DialogTitle(LOCTEXT("ScreenshotConfigTitle", "Screenshot Configuration"))
		.DialogSize(InArgs._DialogSize)
		.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
		.DialogPosition(InArgs._DialogPosition)
		.DialogAnchorPoint(InArgs._DialogAnchorPoint)
		.ContentPadding(InArgs._ContentPadding)
		.ButtonMask(InArgs._ButtonMask)
		.OnDialogResult(InArgs._OnDialogResult)
		);
	
	ResolutionList.Add(MakeShareable(new FString(TEXT("1920x1080 HDTV"))));
	ResolutionList.Add(MakeShareable(new FString(TEXT("3840x2160 4K UHD"))));
	ResolutionList.Add(MakeShareable(new FString(TEXT("7680x4320 8K UHD"))));
	ResolutionList.Add(MakeShareable(new FString(TEXT("15360x8640 16K UHD"))));
	// GMaxTextureDimensions on DX11 is 16384
	//ResolutionList.Add(MakeShareable(new FString(TEXT("30720x17280 32K UHD"))));

	FMargin NameColumnPadding = FMargin(10, 4);
	FMargin ValueColumnPadding = FMargin(0, 4);

	if (DialogContent.IsValid())
	{
		DialogContent->AddSlot()
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(46)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.MidFill"))
						]
					]
				]
			]			
			+SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SGridPanel)
					+ SGridPanel::Slot(0, 0)
					.Padding(NameColumnPadding)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(LOCTEXT("Resolution", "Resolution"))
					]
					+ SGridPanel::Slot(1, 0)
					.Padding(ValueColumnPadding)
					[
						SNew( SBox )
						.WidthOverride(300.f)
						[
							SAssignNew(ResolutionComboBox, SComboBox< TSharedPtr<FString> >)
							.InitiallySelectedItem(0)
							.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
							.OptionsSource(&ResolutionList)
							.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUWScreenshotConfigDialog::OnResolutionSelected)
							.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
							.Content()
							[
								SAssignNew(SelectedResolution, STextBlock)
								.Text(FText::FromString(TEXT("15360x8640 16K UHD")))
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
							]
						]
					]
				]
			]
		];
	}

	ResolutionComboBox->SetSelectedItem(ResolutionList[1]);
	for (auto Res : ResolutionList)
	{
		if (Res->StartsWith(FString::FromInt(GetPlayerOwner()->GetProfileSettings()->ReplayScreenshotResX)))
		{
			ResolutionComboBox->SetSelectedItem(Res);
		}
	}
}

FReply SUWScreenshotConfigDialog::OnButtonClick(uint16 ButtonID)
{
	FString SelectedRes = SelectedResolution->GetText().ToString();
	int32 XLoc;
	SelectedRes.FindChar(TEXT('x'), XLoc);
	int32 SpaceLoc;
	SelectedRes.FindChar(TEXT(' '), SpaceLoc);
	FString ResXText = SelectedRes.Left(XLoc);
	FString ResYText = SelectedRes.Left(SpaceLoc).RightChop(XLoc + 1);
	LexicalConversion::FromString(GetPlayerOwner()->GetProfileSettings()->ReplayScreenshotResX, *ResXText);
	LexicalConversion::FromString(GetPlayerOwner()->GetProfileSettings()->ReplayScreenshotResY, *ResYText);
	GetPlayerOwner()->SaveProfileSettings();

	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

void SUWScreenshotConfigDialog::OnResolutionSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedResolution->SetText(*NewSelection.Get());
}

#undef LOCTEXT_NAMESPACE

#endif