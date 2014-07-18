// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWSystemSettingsDialog.h"
#include "SUWindowsStyle.h"
#include "UTPlayerInput.h"
#include "Scalability.h"

SVerticalBox::FSlot& SUWSystemSettingsDialog::AddGeneralScalabilityWidget(const FString& Desc, TSharedPtr< SComboBox< TSharedPtr<FString> > >& ComboBox, TSharedPtr<STextBlock>& SelectedItemWidget, void (SUWSystemSettingsDialog::*SelectionFunc)(TSharedPtr<FString>, ESelectInfo::Type), int32 SettingValue)
{
	return SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(Desc)
			]
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SAssignNew(ComboBox, SComboBox< TSharedPtr<FString> >)
				.InitiallySelectedItem(GeneralScalabilityList[SettingValue])
				.ComboBoxStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				.OptionsSource(&GeneralScalabilityList)
				.OnGenerateWidget(this, &SUWSystemSettingsDialog::GenerateTextWidget)
				.OnSelectionChanged(this, SelectionFunc)
				.Content()
				[
					SAssignNew(SelectedItemWidget, STextBlock)
					.Text(*GeneralScalabilityList[SettingValue].Get())
				]
			]
		];
}

SVerticalBox::FSlot& SUWSystemSettingsDialog::AddGeneralSliderWidget(const FString& Desc, TSharedPtr<SSlider>& SliderWidget, float SettingValue)
{
	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	return 
		SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(Desc)
			]
			+ SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f, 10.0f, 0.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.WidthOverride(250.0f * ViewportSize.X / 1280.0f)
				.Content()
				[
					SAssignNew(SliderWidget, SSlider)
					.Orientation(Orient_Horizontal)
					.Value(SettingValue)
				]
			]
		];
}

FString SUWSystemSettingsDialog::GetFOVLabelText(int32 FOVAngle)
{
	return FText::Format(NSLOCTEXT("SUWPlayerSettingsDialog", "FOV", "Field of View ({Value})"), FText::FromString(FString::Printf(TEXT("%i"), FOVAngle))).ToString();
}

void SUWSystemSettingsDialog::Construct(const FArguments& InArgs)
{
	MouseSensitivityRange = FVector2D(0.01f, 0.15f);

	SUWDialog::Construct(SUWDialog::FArguments().PlayerOwner(InArgs._PlayerOwner));

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	// find current and available screen resolutions
	int32 CurrentResIndex = INDEX_NONE;
	FScreenResolutionArray ResArray;
	if (RHIGetAvailableResolutions(ResArray, false))
	{
		TArray<FIntPoint> AddedRes; // used to more efficiently avoid duplicates
		for (int32 ModeIndex = 0; ModeIndex < ResArray.Num(); ModeIndex++)
		{
			FIntPoint NewRes(int32(ResArray[ModeIndex].Width), int32(ResArray[ModeIndex].Height));
			if (!AddedRes.Contains(NewRes))
			{
				ResList.Add(MakeShareable(new FString(FString::Printf(TEXT("%ix%i"), NewRes.X, NewRes.Y))));
				if (NewRes.X == int32(ViewportSize.X) && NewRes.Y == int32(ViewportSize.Y))
				{
					CurrentResIndex = ResList.Num() - 1;
				}
				AddedRes.Add(NewRes);
			}
		}
	}
	if (CurrentResIndex == INDEX_NONE)
	{
		CurrentResIndex = ResList.Add(MakeShareable(new FString(FString::Printf(TEXT("%ix%i"), int32(ViewportSize.X), int32(ViewportSize.Y)))));
	}

	// find current and available engine scalability options
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	Scalability::FQualityLevels QualitySettings = UserSettings->ScalabilityQuality;
	GeneralScalabilityList.Add(MakeShareable(new FString(NSLOCTEXT("SUWSystemSettingsDialog", "SettingsLow", "Low").ToString())));
	GeneralScalabilityList.Add(MakeShareable(new FString(NSLOCTEXT("SUWSystemSettingsDialog", "SettingsMedium", "Medium").ToString())));
	GeneralScalabilityList.Add(MakeShareable(new FString(NSLOCTEXT("SUWSystemSettingsDialog", "SettingsHigh", "High").ToString())));
	GeneralScalabilityList.Add(MakeShareable(new FString(NSLOCTEXT("SUWSystemSettingsDialog", "SettingsEpic", "Epic").ToString())));
	QualitySettings.TextureQuality = FMath::Clamp<int32>(QualitySettings.TextureQuality, 0, GeneralScalabilityList.Num() - 1);
	QualitySettings.ShadowQuality = FMath::Clamp<int32>(QualitySettings.ShadowQuality, 0, GeneralScalabilityList.Num() - 1);
	QualitySettings.PostProcessQuality = FMath::Clamp<int32>(QualitySettings.PostProcessQuality, 0, GeneralScalabilityList.Num() - 1);
	QualitySettings.EffectsQuality = FMath::Clamp<int32>(QualitySettings.EffectsQuality, 0, GeneralScalabilityList.Num() - 1);

	ChildSlot
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SBorder)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.BorderImage(SUWindowsStyle::Get().GetBrush("UWindows.Standard.MenuBar.Background"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0.0f, 5.0f, 0.0f, 5.0f)
				.AutoHeight()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::Black)
						.Text(NSLOCTEXT("SUWSystemSettingsDialog", "Resolution", "Resolution:").ToString())
					]
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SComboBox< TSharedPtr<FString> >)
						.InitiallySelectedItem(ResList[CurrentResIndex])
						.ComboBoxStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
						.OptionsSource(&ResList)
						.OnGenerateWidget(this, &SUWSystemSettingsDialog::GenerateTextWidget)
						.OnSelectionChanged(this, &SUWSystemSettingsDialog::OnResolutionSelected)
						.Content()
						[
							SAssignNew(SelectedRes, STextBlock)
							.Text(*ResList[CurrentResIndex].Get())
						]
					]
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
				[
					SAssignNew(Fullscreen, SCheckBox)
					.ForegroundColor(FLinearColor::Black)
					.IsChecked(GetPlayerOwner()->ViewportClient->IsFullScreenViewport() ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
					.Content()
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::Black)
						.Text(NSLOCTEXT("SUWSystemSettingsDialog", "Fullscreen", "Fullscreen").ToString())
					]
				]
				+ SVerticalBox::Slot()
				.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::Black)
						.Text(NSLOCTEXT("SUWSystemSettingsDialog", "MouseSensitivity", "Mouse Sensitivity").ToString())
					]
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(250.0f * ViewportSize.X / 1280.0f)
						.Content()
						[
							SAssignNew(MouseSensitivity, SSlider)
							.Orientation(Orient_Horizontal)
							.Value((UUTPlayerInput::StaticClass()->GetDefaultObject<UUTPlayerInput>()->GetMouseSensitivity() - MouseSensitivityRange.X) / (MouseSensitivityRange.Y - MouseSensitivityRange.X))
						]
					]
				]
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
				[
					SAssignNew(MouseSmoothing, SCheckBox)
					.ForegroundColor(FLinearColor::Black)
					.IsChecked(GetDefault<UInputSettings>()->bEnableMouseSmoothing ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
					.Content()
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::Black)
						.Text(NSLOCTEXT("SUWSystemSettingsDialog", "MouseSmoothing", "Mouse Smoothing").ToString())
					]
				]
				+ AddGeneralScalabilityWidget(NSLOCTEXT("SUWSystemSettingsDialog", "TextureDetail", "Texture Detail").ToString(), TextureRes, SelectedTextureRes, &SUWSystemSettingsDialog::OnTextureResolutionSelected, QualitySettings.TextureQuality)
				+ AddGeneralScalabilityWidget(NSLOCTEXT("SUWSystemSettingsDialog", "ShadowQuality", "Shadow Quality").ToString(), ShadowQuality, SelectedShadowQuality, &SUWSystemSettingsDialog::OnShadowQualitySelected, QualitySettings.ShadowQuality)
				+ AddGeneralScalabilityWidget(NSLOCTEXT("SUWSystemSettingsDialog", "EffectsQuality", "Effects Quality").ToString(), EffectQuality, SelectedEffectQuality, &SUWSystemSettingsDialog::OnEffectQualitySelected, QualitySettings.EffectsQuality)
				+ AddGeneralScalabilityWidget(NSLOCTEXT("SUWSystemSettingsDialog", "PP Quality", "Post Process Quality").ToString(), PPQuality, SelectedPPQuality, &SUWSystemSettingsDialog::OnPPQualitySelected, QualitySettings.PostProcessQuality)
				+ SVerticalBox::Slot()
				.Padding(0.0f, 10.0f, 0.0f, 10.0f)
				.AutoHeight()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SBox)
						.HAlign(HAlign_Center)
						.WidthOverride(80.0f * ViewportSize.X / 1280.0f)
						.Content()
						[
							SAssignNew(FOVLabel, STextBlock)
							.ColorAndOpacity(FLinearColor::Black)
							.Text(GetFOVLabelText(int(GetDefault<AUTPlayerController>()->ConfigDefaultFOV)))
						]
					]
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(250.0f * ViewportSize.X / 1280.0f)
						.Content()
						[
							SAssignNew(FOV, SSlider)
							.OnValueChanged(this, &SUWSystemSettingsDialog::OnFOVChange)
							.Orientation(Orient_Horizontal)
							.Value((GetDefault<AUTPlayerController>()->ConfigDefaultFOV - FOV_CONFIG_MIN) / (FOV_CONFIG_MAX - FOV_CONFIG_MIN))
						]
					]
				]
				+ AddGeneralSliderWidget(NSLOCTEXT("SUWSystemSettingsDialog", "MasterSoundVolume", "Master Sound Volume").ToString(), SoundVolumes[EUTSoundClass::Master], UserSettings->GetSoundClassVolume(EUTSoundClass::Master))
				+ AddGeneralSliderWidget(NSLOCTEXT("SUWSystemSettingsDialog", "MusicVolume", "Music Volume").ToString(), SoundVolumes[EUTSoundClass::Music], UserSettings->GetSoundClassVolume(EUTSoundClass::Music))
				+ AddGeneralSliderWidget(NSLOCTEXT("SUWSystemSettingsDialog", "SFXVolume", "Effects Volume").ToString(), SoundVolumes[EUTSoundClass::SFX], UserSettings->GetSoundClassVolume(EUTSoundClass::SFX))
				+ AddGeneralSliderWidget(NSLOCTEXT("SUWSystemSettingsDialog", "VoiceVolume", "Voice Volume").ToString(), SoundVolumes[EUTSoundClass::Voice], UserSettings->GetSoundClassVolume(EUTSoundClass::Voice))
				+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Right)
				.Padding(5.0f, 5.0f, 5.0f, 5.0f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FMargin(10.0f, 10.0f, 10.0f, 10.0f))
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
						.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
						.Text(NSLOCTEXT("SUWMessageBox", "OKButton", "OK").ToString())
						.OnClicked(this, &SUWSystemSettingsDialog::OKClick)
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
						.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
						.Text(NSLOCTEXT("SUWMessageBox", "CancelButton", "Cancel").ToString())
						.OnClicked(this, &SUWSystemSettingsDialog::CancelClick)
					]
				]
			]
		];
}

void SUWSystemSettingsDialog::OnFOVChange(float NewValue)
{
	FOVLabel->SetText(GetFOVLabelText(FMath::TruncToInt(NewValue * (FOV_CONFIG_MAX - FOV_CONFIG_MIN) + FOV_CONFIG_MIN)));
}

FReply SUWSystemSettingsDialog::OKClick()
{
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	// sound settings
	for (int32 i = 0; i < ARRAY_COUNT(SoundVolumes); i++)
	{
		UserSettings->SetSoundClassVolume(EUTSoundClass::Type(i), SoundVolumes[i]->GetValue());
	}
	// mouse sensitivity
	float NewSensitivity = MouseSensitivity->GetValue() * (MouseSensitivityRange.Y - MouseSensitivityRange.X) + MouseSensitivityRange.X;
	for (TObjectIterator<UUTPlayerInput> It(RF_NoFlags); It; ++It)
	{
		It->SetMouseSensitivity(NewSensitivity);
	}
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	for (FInputAxisConfigEntry& Entry : InputSettings->AxisConfig)
	{
		if (Entry.AxisKeyName == EKeys::MouseX || Entry.AxisKeyName == EKeys::MouseY)
		{
			Entry.AxisProperties.Sensitivity = NewSensitivity;
		}
	}
	InputSettings->bEnableMouseSmoothing = MouseSmoothing->IsChecked();
	InputSettings->SaveConfig();
	// engine scalability
	UserSettings->ScalabilityQuality.TextureQuality = GeneralScalabilityList.Find(TextureRes->GetSelectedItem());
	UserSettings->ScalabilityQuality.ShadowQuality = GeneralScalabilityList.Find(ShadowQuality->GetSelectedItem());
	UserSettings->ScalabilityQuality.PostProcessQuality = GeneralScalabilityList.Find(PPQuality->GetSelectedItem());
	UserSettings->ScalabilityQuality.EffectsQuality = GeneralScalabilityList.Find(EffectQuality->GetSelectedItem());
	Scalability::SetQualityLevels(UserSettings->ScalabilityQuality);
	Scalability::SaveState(GGameUserSettingsIni);
	// resolution
	GetPlayerOwner()->ViewportClient->ConsoleCommand(*FString::Printf(TEXT("setres %s%s"), *SelectedRes->GetText(), Fullscreen->IsChecked() ? TEXT("f") : TEXT("w")));
	// FOV
	float NewFOV = FMath::TruncToFloat(FOV->GetValue() * (FOV_CONFIG_MAX - FOV_CONFIG_MIN) + FOV_CONFIG_MIN);
	AUTPlayerController* PC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (PC != NULL)
	{
		PC->FOV(NewFOV);
	}
	else
	{
		AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>()->ConfigDefaultFOV = NewFOV;
		AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>()->SaveConfig();
	}

	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

FReply SUWSystemSettingsDialog::CancelClick()
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

TSharedRef<SWidget> SUWSystemSettingsDialog::GenerateTextWidget(TSharedPtr<FString> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::Black)
			.Text(*InItem.Get())
		];
}

void SUWSystemSettingsDialog::OnResolutionSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedRes->SetText(*NewSelection.Get());
}
void SUWSystemSettingsDialog::OnTextureResolutionSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedTextureRes->SetText(*NewSelection.Get());
}
void SUWSystemSettingsDialog::OnShadowQualitySelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedShadowQuality->SetText(*NewSelection.Get());
}
void SUWSystemSettingsDialog::OnPPQualitySelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedPPQuality->SetText(*NewSelection.Get());
}
void SUWSystemSettingsDialog::OnEffectQualitySelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedEffectQuality->SetText(*NewSelection.Get());
}