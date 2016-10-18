// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTScreenshotConfigDialog.h"
#include "../SUWindowsStyle.h"
#include "UTPlayerCameraManager.h"

#if !UE_SERVER

#define LOCTEXT_NAMESPACE "SUTScreenshotConfigDialog"

void SUTScreenshotConfigDialog::Construct(const FArguments& InArgs)
{
	// Let the Dialog construct itself.
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
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
	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();
	if (ProfileSettings)
	{
		bCustomPost = ProfileSettings->bReplayCustomPostProcess;
		ReplayCustomBloomIntensity = ProfileSettings->ReplayCustomBloomIntensity / 8.0f;
		ReplayCustomDOFAmount = ProfileSettings->ReplayCustomDOFAmount / 100.0f;
		ReplayCustomDOFDistance = ProfileSettings->ReplayCustomDOFDistance / 10000.0f;
		ReplayCustomDOFScale = ProfileSettings->ReplayCustomDOFScale / 2.0f;
		ReplayCustomDOFNearBlur = ProfileSettings->ReplayCustomDOFNearBlur / 32.0f;
		ReplayCustomDOFFarBlur = ProfileSettings->ReplayCustomDOFFarBlur / 32.0f;
		ReplayCustomMotionBlurAmount = ProfileSettings->ReplayCustomMotionBlurAmount;
		ReplayCustomMotionBlurMax = ProfileSettings->ReplayCustomMotionBlurMax / 100.0f;
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
							.OnGenerateWidget(this, &SUTDialogBase::GenerateStringListWidget)
							.OnSelectionChanged(this, &SUTScreenshotConfigDialog::OnResolutionSelected)
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
						.IndentHandle(false)
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
						.IndentHandle(false)
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
						.IndentHandle(false)
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
						.IndentHandle(false)
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
						.IndentHandle(false)
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
						.IndentHandle(false)
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
						.IndentHandle(false)
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
						.IndentHandle(false)
						.Value(ReplayCustomDOFFarBlur)
					]
				]
			]
		];
	}

	ResolutionComboBox->SetSelectedItem(ResolutionList[1]);
	if (ProfileSettings)
	{
		for (auto Res : ResolutionList)
		{
			if (Res->StartsWith(FString::FromInt(ProfileSettings->ReplayScreenshotResX)))
			{
				ResolutionComboBox->SetSelectedItem(Res);
			}
		}
	}
}

FReply SUTScreenshotConfigDialog::OnButtonClick(uint16 ButtonID)
{
	FString SelectedRes = SelectedResolution->GetText().ToString();
	int32 XLoc;
	SelectedRes.FindChar(TEXT('x'), XLoc);
	int32 SpaceLoc;
	SelectedRes.FindChar(TEXT(' '), SpaceLoc);
	FString ResXText = SelectedRes.Left(XLoc);
	FString ResYText = SelectedRes.Left(SpaceLoc).RightChop(XLoc + 1);
	
	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();
	if (ProfileSettings)
	{
		LexicalConversion::FromString(ProfileSettings->ReplayScreenshotResX, *ResXText);
		LexicalConversion::FromString(ProfileSettings->ReplayScreenshotResY, *ResYText);

		ProfileSettings->bReplayCustomPostProcess = CustomPostCheckBox->IsChecked();
		ProfileSettings->ReplayCustomBloomIntensity = 8.0f * BloomIntensitySlider->GetValue();
		ProfileSettings->ReplayCustomMotionBlurAmount = MotionBlurAmountSlider->GetValue();
		ProfileSettings->ReplayCustomMotionBlurMax = 100.0f * MotionBlurMaxSlider->GetValue();
		ProfileSettings->ReplayCustomDOFDistance = 10000.0f * DOFDistanceSlider->GetValue();
		ProfileSettings->ReplayCustomDOFAmount = 100.0f * DOFAmountSlider->GetValue();
		ProfileSettings->ReplayCustomDOFScale = 2.0f * DOFScaleSlider->GetValue();
		ProfileSettings->ReplayCustomDOFNearBlur = 32.0f * DOFNearBlurSlider->GetValue();
		ProfileSettings->ReplayCustomDOFFarBlur = 32.0f * DOFFarBlurSlider->GetValue();

		GetPlayerOwner()->SaveProfileSettings();
	}

	AUTPlayerController* PC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (PC)
	{
		if (ProfileSettings && ProfileSettings->bReplayCustomPostProcess)
		{
			AUTPlayerCameraManager* CamMan = Cast<AUTPlayerCameraManager>(PC->PlayerCameraManager);
			if (CamMan)
			{
				CamMan->StylizedPPSettings[0].BloomIntensity = ProfileSettings->ReplayCustomBloomIntensity;
				CamMan->StylizedPPSettings[0].DepthOfFieldDepthBlurAmount = ProfileSettings->ReplayCustomDOFAmount;
				CamMan->StylizedPPSettings[0].DepthOfFieldFocalDistance = ProfileSettings->ReplayCustomDOFDistance;
				CamMan->StylizedPPSettings[0].DepthOfFieldScale = ProfileSettings->ReplayCustomDOFScale;
				CamMan->StylizedPPSettings[0].DepthOfFieldNearBlurSize = ProfileSettings->ReplayCustomDOFNearBlur;
				CamMan->StylizedPPSettings[0].DepthOfFieldFarBlurSize = ProfileSettings->ReplayCustomDOFFarBlur;
				CamMan->StylizedPPSettings[0].MotionBlurAmount = ProfileSettings->ReplayCustomMotionBlurAmount;
				CamMan->StylizedPPSettings[0].MotionBlurMax = ProfileSettings->ReplayCustomMotionBlurMax;
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

void SUTScreenshotConfigDialog::OnResolutionSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	TempSelectedRes = *NewSelection.Get();
	int32 XLoc;
	TempSelectedRes.FindChar(TEXT('x'), XLoc);
	int32 SpaceLoc;
	TempSelectedRes.FindChar(TEXT(' '), SpaceLoc);
	FString ResXText = TempSelectedRes.Left(XLoc);
	FString ResYText = TempSelectedRes.Left(SpaceLoc).RightChop(XLoc + 1);

	int32 ResX = FCString::Atoi(*ResXText);
	if (ResX >= 15360)
	{
		// Show warning dialog

		GetPlayerOwner()->ShowMessage(LOCTEXT("ScreenshotWarningTitle", "Are You Sure?"), 
								      LOCTEXT("ScreenshotWarning", "Screenshots this big are only recommended for video cards with 4 gigs of video ram or above.\nAre you sure you want to continue?"), 
								      UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateSP(this, &SUTScreenshotConfigDialog::ScreenshotConfirm));
	}
	else
	{
		SelectedResolution->SetText(*NewSelection.Get());
	}
}

void SUTScreenshotConfigDialog::ScreenshotConfirm(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		SelectedResolution->SetText(TempSelectedRes);
	}
	else
	{
		ResolutionComboBox->SetSelectedItem(ResolutionList[0]);
	}
}

#undef LOCTEXT_NAMESPACE

#endif