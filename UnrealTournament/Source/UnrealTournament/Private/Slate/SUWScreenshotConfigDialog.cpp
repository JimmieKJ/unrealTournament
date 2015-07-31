// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWScreenshotConfigDialog.h"
#include "SUWindowsStyle.h"
#include "UTPlayerCameraManager.h"

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

	bool bCustomPost = false;
	float ReplayCustomBloomIntensity = 0;
	float ReplayCustomDOFAmount = 0;
	float ReplayCustomDOFDistance = 0;
	float ReplayCustomDOFScale = 0;
	float ReplayCustomDOFNearBlur = 0;
	float ReplayCustomDOFFarBlur = 0;
	float ReplayCustomMotionBlurAmount = 0;
	float ReplayCustomMotionBlurMax = 0;
	AUTPlayerController* PC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (PC)
	{
		bCustomPost = GetPlayerOwner()->GetProfileSettings()->bReplayCustomPostProcess;
		ReplayCustomBloomIntensity = GetPlayerOwner()->GetProfileSettings()->ReplayCustomBloomIntensity / 8.0f;
		ReplayCustomDOFAmount = GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFAmount / 100.0f;
		ReplayCustomDOFDistance = GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFDistance / 10000.0f;
		ReplayCustomDOFScale = GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFScale / 2.0f;
		ReplayCustomDOFNearBlur = GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFNearBlur / 32.0f;
		ReplayCustomDOFFarBlur = GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFFarBlur / 32.0f;
		ReplayCustomMotionBlurAmount = GetPlayerOwner()->GetProfileSettings()->ReplayCustomMotionBlurAmount;
		ReplayCustomMotionBlurMax = GetPlayerOwner()->GetProfileSettings()->ReplayCustomMotionBlurMax / 100.0f;
	}

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
					+ SGridPanel::Slot(0, 1)
					.Padding(NameColumnPadding)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(LOCTEXT("CustomPostProcess", "Custom Post Process"))
					]
					+ SGridPanel::Slot(1, 1)
					.Padding(ValueColumnPadding)
					[
						SAssignNew(CustomPostCheckBox, SCheckBox)
						.IsChecked(bCustomPost ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
						.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					]
					+ SGridPanel::Slot(0, 2)
					.Padding(NameColumnPadding)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(LOCTEXT("BloomIntensity", "Bloom Intensity"))
					]
					+ SGridPanel::Slot(1, 2)
					.Padding(ValueColumnPadding)
					[
						SAssignNew(BloomIntensitySlider, SSlider)
						.Value(ReplayCustomBloomIntensity)
					]
					+ SGridPanel::Slot(0, 3)
					.Padding(NameColumnPadding)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(LOCTEXT("MotionBlurAmount", "Motion Blur"))
					]
					+ SGridPanel::Slot(1, 3)
					.Padding(ValueColumnPadding)
					[
						SAssignNew(MotionBlurAmountSlider, SSlider)
						.Value(ReplayCustomMotionBlurAmount)
					]
					+ SGridPanel::Slot(0, 4)
					.Padding(NameColumnPadding)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(LOCTEXT("MotionBlurMax", "Motion Blur Max"))
					]
					+ SGridPanel::Slot(1, 4)
					.Padding(ValueColumnPadding)
					[
						SAssignNew(MotionBlurMaxSlider, SSlider)
						.Value(ReplayCustomMotionBlurMax)
					]
					+ SGridPanel::Slot(0, 5)
					.Padding(NameColumnPadding)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(LOCTEXT("DOFAmount", "Depth Of Field Amount"))
					]
					+ SGridPanel::Slot(1, 5)
					.Padding(ValueColumnPadding)
					[
						SAssignNew(DOFAmountSlider, SSlider)
						.Value(ReplayCustomDOFAmount)
					]
					+ SGridPanel::Slot(0, 6)
					.Padding(NameColumnPadding)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(LOCTEXT("DOFDistance", "Depth Of Field Distance"))
					]
					+ SGridPanel::Slot(1, 6)
					.Padding(ValueColumnPadding)
					[
						SAssignNew(DOFDistanceSlider, SSlider)
						.Value(ReplayCustomDOFDistance)
					]
					+ SGridPanel::Slot(0, 7)
					.Padding(NameColumnPadding)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(LOCTEXT("DOFScale", "Depth Of Field Scale"))
					]
					+ SGridPanel::Slot(1, 7)
					.Padding(ValueColumnPadding)
					[
						SAssignNew(DOFScaleSlider, SSlider)
						.Value(ReplayCustomDOFScale)
					]
					+ SGridPanel::Slot(0, 8)
					.Padding(NameColumnPadding)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(LOCTEXT("DOFNearBlur", "Depth Of Field Near Blur"))
					]
					+ SGridPanel::Slot(1, 8)
					.Padding(ValueColumnPadding)
					[
						SAssignNew(DOFNearBlurSlider, SSlider)
						.Value(ReplayCustomDOFNearBlur)
					]
					+ SGridPanel::Slot(0, 9)
					.Padding(NameColumnPadding)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(LOCTEXT("DOFFarBlur", "Depth Of Field Far Blur"))
					]
					+ SGridPanel::Slot(1, 9)
					.Padding(ValueColumnPadding)
					[
						SAssignNew(DOFFarBlurSlider, SSlider)
						.Value(ReplayCustomDOFFarBlur)
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

	GetPlayerOwner()->GetProfileSettings()->bReplayCustomPostProcess = CustomPostCheckBox->IsChecked();
	GetPlayerOwner()->GetProfileSettings()->ReplayCustomBloomIntensity = 8.0f * BloomIntensitySlider->GetValue();
	GetPlayerOwner()->GetProfileSettings()->ReplayCustomMotionBlurAmount = MotionBlurAmountSlider->GetValue();
	GetPlayerOwner()->GetProfileSettings()->ReplayCustomMotionBlurMax = 100.0f * MotionBlurMaxSlider->GetValue();
	GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFDistance = 10000.0f * DOFDistanceSlider->GetValue();
	GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFAmount = 100.0f * DOFAmountSlider->GetValue();
	GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFScale = 2.0f * DOFScaleSlider->GetValue();
	GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFNearBlur = 32.0f * DOFNearBlurSlider->GetValue();
	GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFFarBlur = 32.0f * DOFFarBlurSlider->GetValue();

	GetPlayerOwner()->SaveProfileSettings();

	AUTPlayerController* PC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (PC)
	{
		if (GetPlayerOwner()->GetProfileSettings()->bReplayCustomPostProcess)
		{
			AUTPlayerCameraManager* CamMan = Cast<AUTPlayerCameraManager>(PC->PlayerCameraManager);
			if (CamMan)
			{
				CamMan->StylizedPPSettings[0].BloomIntensity = GetPlayerOwner()->GetProfileSettings()->ReplayCustomBloomIntensity;
				CamMan->StylizedPPSettings[0].DepthOfFieldDepthBlurAmount = GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFAmount;
				CamMan->StylizedPPSettings[0].DepthOfFieldFocalDistance = GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFDistance;
				CamMan->StylizedPPSettings[0].DepthOfFieldScale = GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFScale;
				CamMan->StylizedPPSettings[0].DepthOfFieldNearBlurSize = GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFNearBlur;
				CamMan->StylizedPPSettings[0].DepthOfFieldFarBlurSize = GetPlayerOwner()->GetProfileSettings()->ReplayCustomDOFFarBlur;
				CamMan->StylizedPPSettings[0].MotionBlurAmount = GetPlayerOwner()->GetProfileSettings()->ReplayCustomMotionBlurAmount;
				CamMan->StylizedPPSettings[0].MotionBlurMax = GetPlayerOwner()->GetProfileSettings()->ReplayCustomMotionBlurMax;
			}
			PC->SetStylizedPP(0);
		}
		else
		{
			PC->SetStylizedPP(INDEX_NONE);
		}
	}

	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

void SUWScreenshotConfigDialog::OnResolutionSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedResolution->SetText(*NewSelection.Get());
}

#undef LOCTEXT_NAMESPACE

#endif