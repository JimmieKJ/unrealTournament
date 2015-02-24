// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWSystemSettingsDialog.h"
#include "SUWindowsStyle.h"
#include "UTPlayerInput.h"
#include "Scalability.h"
#include "UTWorldSettings.h"
#include "UTGameEngine.h"

#if !UE_SERVER

SVerticalBox::FSlot& SUWSystemSettingsDialog::AddSectionHeader(const FText& SectionDesc)
{
	return SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		.Padding(FMargin(0.0f, 15.0f, 0.0f, 15.0f))
		[
			SNew(STextBlock)
			.Text(SectionDesc)
			.TextStyle(SUWindowsStyle::Get(),"UT.Common.BoldText")
		];

}

SVerticalBox::FSlot& SUWSystemSettingsDialog::AddGeneralScalabilityWidget(const FString& Desc, TSharedPtr< SComboBox< TSharedPtr<FString> > >& ComboBox, TSharedPtr<STextBlock>& SelectedItemWidget, void (SUWSystemSettingsDialog::*SelectionFunc)(TSharedPtr<FString>, ESelectInfo::Type), int32 SettingValue)
{
	return SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(650)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(Desc)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(ComboBox, SComboBox< TSharedPtr<FString> >)
				.InitiallySelectedItem(GeneralScalabilityList[SettingValue])
				.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
				.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
				.OptionsSource(&GeneralScalabilityList)
				.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
				.OnSelectionChanged(this, SelectionFunc)
				.Content()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					[
						SAssignNew(SelectedItemWidget, STextBlock)
						.Text(*GeneralScalabilityList[SettingValue].Get())
						.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText.Black")
					]
				]
			]
		];
}

SVerticalBox::FSlot& SUWSystemSettingsDialog::AddAAModeWidget(const FString& Desc, TSharedPtr< SComboBox< TSharedPtr<FString> > >& ComboBox, TSharedPtr<STextBlock>& SelectedItemWidget, void (SUWSystemSettingsDialog::*SelectionFunc)(TSharedPtr<FString>, ESelectInfo::Type), int32 SettingValue)
{
	return SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		.Padding(FMargin(10.0f, 15.0f, 10.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(650)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.Text(Desc)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(ComboBox, SComboBox< TSharedPtr<FString> >)
				.InitiallySelectedItem(AAModeList[SettingValue])
				.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
				.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
				.OptionsSource(&AAModeList)
				.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
				.OnSelectionChanged(this, SelectionFunc)
				.Content()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(10.0f, 0.0f, 10.0f, 0.0f)
					[
						SAssignNew(SelectedItemWidget, STextBlock)
						.Text(*AAModeList[SettingValue].Get())
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText.Black")
					]
				]
			]
		];
}

SVerticalBox::FSlot& SUWSystemSettingsDialog::AddGeneralSliderWidget(const FString& Desc, TSharedPtr<SSlider>& SliderWidget, float SettingValue)
{
	return SVerticalBox::Slot()
	.AutoHeight()
	.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(650)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(Desc)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300.0f)
			.Content()
			[
				SAssignNew(SliderWidget, SSlider)
				.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
				.Orientation(Orient_Horizontal)
				.Value(SettingValue)
			]
		]
	];
}

SVerticalBox::FSlot& SUWSystemSettingsDialog::AddGeneralSliderWithLabelWidget(TSharedPtr<SSlider>& SliderWidget, TSharedPtr<STextBlock>& LabelWidget, void(SUWSystemSettingsDialog::*SelectionFunc)(float), const FString& InitialLabel, float SettingValue)
{
	return SVerticalBox::Slot()
	.AutoHeight()
	.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(650)
			[
				SAssignNew(LabelWidget, STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(InitialLabel)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300.0f)
			.Content()
			[
				SAssignNew(SliderWidget, SSlider)
				.Style(SUWindowsStyle::Get(),"UT.Common.Slider")
				.OnValueChanged(this, SelectionFunc)
				.Orientation(Orient_Horizontal)
				.Value(SettingValue)
			]
		]
	];
}

void SUWSystemSettingsDialog::Construct(const FArguments& InArgs)
{
	DecalLifetimeRange = FVector2D(5.0f, 105.0f);
	ScreenPercentageRange = FVector2D(25.0f, 100.0f);

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
			if (ResArray[ModeIndex].Width >= 800 && ResArray[ModeIndex].Height >= 600)
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
	}
	if (CurrentResIndex == INDEX_NONE)
	{
		CurrentResIndex = ResList.Add(MakeShareable(new FString(FString::Printf(TEXT("%ix%i"), int32(ViewportSize.X), int32(ViewportSize.Y)))));
	}

	// find current and available engine scalability options
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	UserSettings->OnSettingsAutodetected().AddSP(this, &SUWSystemSettingsDialog::OnSettingsAutodetected);
	Scalability::FQualityLevels QualitySettings = UserSettings->ScalabilityQuality;
	GeneralScalabilityList.Add(MakeShareable(new FString(NSLOCTEXT("SUWSystemSettingsDialog", "SettingsLow", "Low").ToString())));
	GeneralScalabilityList.Add(MakeShareable(new FString(NSLOCTEXT("SUWSystemSettingsDialog", "SettingsMedium", "Medium").ToString())));
	GeneralScalabilityList.Add(MakeShareable(new FString(NSLOCTEXT("SUWSystemSettingsDialog", "SettingsHigh", "High").ToString())));
	GeneralScalabilityList.Add(MakeShareable(new FString(NSLOCTEXT("SUWSystemSettingsDialog", "SettingsEpic", "Epic").ToString())));
	QualitySettings.TextureQuality = FMath::Clamp<int32>(QualitySettings.TextureQuality, 0, GeneralScalabilityList.Num() - 1);
	QualitySettings.ShadowQuality = FMath::Clamp<int32>(QualitySettings.ShadowQuality, 0, GeneralScalabilityList.Num() - 1);
	QualitySettings.PostProcessQuality = FMath::Clamp<int32>(QualitySettings.PostProcessQuality, 0, GeneralScalabilityList.Num() - 1);
	QualitySettings.EffectsQuality = FMath::Clamp<int32>(QualitySettings.EffectsQuality, 0, GeneralScalabilityList.Num() - 1);

	AAModeList.Add(MakeShareable(new FString(NSLOCTEXT("SUWSystemSettingsDialog", "AAModeNone", "None").ToString())));
	AAModeList.Add(MakeShareable(new FString(NSLOCTEXT("SUWSystemSettingsDialog", "AAModeFXAA", "FXAA").ToString())));
	AAModeList.Add(MakeShareable(new FString(NSLOCTEXT("SUWSystemSettingsDialog", "AAModeTemporal", "Temporal").ToString())));
	auto AAModeCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessAAQuality"));
	int32 AAModeSelection = ConvertAAModeToComboSelection(AAModeCVar->GetValueOnGameThread());

	auto ScreenPercentageCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));
	int32 ScreenPercentage = ScreenPercentageCVar->GetValueOnGameThread();
	ScreenPercentage = int32(FMath::Clamp(float(ScreenPercentage), ScreenPercentageRange.X, ScreenPercentageRange.Y));

	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (UTEngine == NULL) // PIE
	{
		UTEngine = UUTGameEngine::StaticClass()->GetDefaultObject<UUTGameEngine>();
	}

	float DecalSliderSetting = (GetDefault<AUTWorldSettings>()->MaxImpactEffectVisibleLifetime <= 0.0f) ? 1.0f : ((GetDefault<AUTWorldSettings>()->MaxImpactEffectVisibleLifetime - DecalLifetimeRange.X) / (DecalLifetimeRange.Y - DecalLifetimeRange.X));
	float FOVSliderSetting = (GetDefault<AUTPlayerController>()->ConfigDefaultFOV - FOV_CONFIG_MIN) / (FOV_CONFIG_MAX - FOV_CONFIG_MIN);
	float ScreenPercentageSliderSetting = (float(ScreenPercentage) - ScreenPercentageRange.X) / (ScreenPercentageRange.Y - ScreenPercentageRange.X);

	if (DialogContent.IsValid())
	{
		DialogContent->AddSlot()
		[

			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText")
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
					.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
					.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
					.OptionsSource(&ResList)
					.OnGenerateWidget(this, &SUWDialog::GenerateStringListWidget)
					.OnSelectionChanged(this, &SUWSystemSettingsDialog::OnResolutionSelected)
					.Content()
					[
						SAssignNew(SelectedRes, STextBlock)
						.Text(*ResList[CurrentResIndex].Get())
						.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText.Black")
					]
				]
			]

			+ AddGeneralSliderWithLabelWidget(ScreenPercentageSlider, ScreenPercentageLabel, &SUWSystemSettingsDialog::OnScreenPercentageChange, GetScreenPercentageLabelText(ScreenPercentageSliderSetting), ScreenPercentageSliderSetting)

			+ AddSectionHeader(NSLOCTEXT("SUWSystemSettingsDialog", "Options", "- Options -"))

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(650)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(NSLOCTEXT("SUWSystemSettingsDialog", "Fullscreen", "Fullscreen").ToString())
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(Fullscreen, SCheckBox)
					.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					.IsChecked(GetPlayerOwner()->ViewportClient->IsFullScreenViewport() ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(650)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(NSLOCTEXT("SUWSystemSettingsDialog", "Smooth Framerate", "Smooth Framerate").ToString())
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(SmoothFrameRate, SCheckBox)
					.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					.IsChecked(GEngine->bSmoothFrameRate ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(10.0f, 5.0f, 10.0f, 5.0f))
			[

				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(650)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(NSLOCTEXT("SUWSystemSettingsDialog", "Frame Rate Cap:", "Frame Rate Cap:").ToString())
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(FrameRateCap, SEditableTextBox)
					.Style(SUWindowsStyle::Get(),"UT.Common.Editbox.White")
					.ForegroundColor(FLinearColor::Black)
					.MinDesiredWidth(100.0f)
					.Text(FText::AsNumber(UTEngine->FrameRateCap))
				]
			]
			+ AddSectionHeader(NSLOCTEXT("SUWSystemSettingsDialog", "DetailSettings", "- Detail Settings -"))

			// Autodetect settings button
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
				.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
				.Text(NSLOCTEXT("SUWSystemSettingsDialog", "AutoSettingsButtonText", "Autodetect Settings"))
				.OnClicked(this, &SUWSystemSettingsDialog::OnAutodetectClick)
			]

			+ AddGeneralScalabilityWidget(NSLOCTEXT("SUWSystemSettingsDialog", "TextureDetail", "Texture Detail").ToString(), TextureRes, SelectedTextureRes, &SUWSystemSettingsDialog::OnTextureResolutionSelected, QualitySettings.TextureQuality)
			+ AddGeneralScalabilityWidget(NSLOCTEXT("SUWSystemSettingsDialog", "ShadowQuality", "Shadow Quality").ToString(), ShadowQuality, SelectedShadowQuality, &SUWSystemSettingsDialog::OnShadowQualitySelected, QualitySettings.ShadowQuality)
			+ AddGeneralScalabilityWidget(NSLOCTEXT("SUWSystemSettingsDialog", "EffectsQuality", "Effects Quality").ToString(), EffectQuality, SelectedEffectQuality, &SUWSystemSettingsDialog::OnEffectQualitySelected, QualitySettings.EffectsQuality)
			+ AddGeneralScalabilityWidget(NSLOCTEXT("SUWSystemSettingsDialog", "PP Quality", "Post Process Quality").ToString(), PPQuality, SelectedPPQuality, &SUWSystemSettingsDialog::OnPPQualitySelected, QualitySettings.PostProcessQuality)
			+ AddAAModeWidget(NSLOCTEXT("SUWSystemSettingsDialog", "AAMode", "AA Mode").ToString(), AAMode, SelectedAAMode, &SUWSystemSettingsDialog::OnAAModeSelected, AAModeSelection)
				
			+ AddGeneralSliderWithLabelWidget(DecalLifetime, DecalLifetimeLabel, &SUWSystemSettingsDialog::OnDecalLifetimeChange, GetDecalLifetimeLabelText(DecalSliderSetting), DecalSliderSetting)
			+ AddGeneralSliderWithLabelWidget(FOV, FOVLabel, &SUWSystemSettingsDialog::OnFOVChange, GetFOVLabelText(FOVSliderSetting), FOVSliderSetting)
			
			+ AddSectionHeader(NSLOCTEXT("SUWSystemSettingsDialog", "SoundSettings", "- Sound/Voice Settings -"))
			+ AddGeneralSliderWidget(NSLOCTEXT("SUWSystemSettingsDialog", "MasterSoundVolume", "Master Sound Volume").ToString(), SoundVolumes[EUTSoundClass::Master], UserSettings->GetSoundClassVolume(EUTSoundClass::Master))
			+ AddGeneralSliderWidget(NSLOCTEXT("SUWSystemSettingsDialog", "MusicVolume", "Music Volume").ToString(), SoundVolumes[EUTSoundClass::Music], UserSettings->GetSoundClassVolume(EUTSoundClass::Music))
			+ AddGeneralSliderWidget(NSLOCTEXT("SUWSystemSettingsDialog", "SFXVolume", "Effects Volume").ToString(), SoundVolumes[EUTSoundClass::SFX], UserSettings->GetSoundClassVolume(EUTSoundClass::SFX))
			+ AddGeneralSliderWidget(NSLOCTEXT("SUWSystemSettingsDialog", "VoiceVolume", "Voice Volume").ToString(), SoundVolumes[EUTSoundClass::Voice], UserSettings->GetSoundClassVolume(EUTSoundClass::Voice))

		];
	}
}

FString SUWSystemSettingsDialog::GetFOVLabelText(float SliderValue)
{
	int32 FOVAngle = FMath::TruncToInt(SliderValue * (FOV_CONFIG_MAX - FOV_CONFIG_MIN) + FOV_CONFIG_MIN);
	return FText::Format(NSLOCTEXT("SUWPlayerSettingsDialog", "FOV", "Field of View ({Value})"), FText::FromString(FString::Printf(TEXT("%i"), FOVAngle))).ToString();
}

void SUWSystemSettingsDialog::OnFOVChange(float NewValue)
{
	FOVLabel->SetText(GetFOVLabelText(NewValue));
}

FString SUWSystemSettingsDialog::GetScreenPercentageLabelText(float SliderValue)
{
	// Increments of 5, so divide by 5 and multiply by 5
	int32 ScreenPercentage = FMath::TruncToInt(SliderValue * (ScreenPercentageRange.Y - ScreenPercentageRange.X) + ScreenPercentageRange.X) / 5 * 5;
	return FText::Format(NSLOCTEXT("SUWPlayerSettingsDialog", "ScreenPercentage", "Screen Percentage ({Value}%)"), FText::FromString(FString::Printf(TEXT("%i"), ScreenPercentage))).ToString();
}

void SUWSystemSettingsDialog::OnScreenPercentageChange(float NewValue)
{
	ScreenPercentageLabel->SetText(GetScreenPercentageLabelText(NewValue));
}

FString SUWSystemSettingsDialog::GetDecalLifetimeLabelText(float SliderValue)
{
	if (SliderValue == 1.0f)
	{
		return NSLOCTEXT("SUWPlayerSettingsDialog", "DecalLifetimeInf", "Decal Lifetime (INF)").ToString();
	}
	
	int32 DecalLifetime = FMath::TruncToInt(SliderValue * (DecalLifetimeRange.Y - DecalLifetimeRange.X) + DecalLifetimeRange.X);
	return FText::Format(NSLOCTEXT("SUWPlayerSettingsDialog", "DecalLifetime", "Decal Lifetime ({Value} seconds)"), FText::FromString(FString::Printf(TEXT("%i"), DecalLifetime))).ToString();
}

void SUWSystemSettingsDialog::OnDecalLifetimeChange(float NewValue)
{
	DecalLifetimeLabel->SetText(GetDecalLifetimeLabelText(NewValue));
}

void SUWSystemSettingsDialog::OnSettingsAutodetected(const Scalability::FQualityLevels& DetectedQuality)
{
	TextureRes->SetSelectedItem(GeneralScalabilityList[DetectedQuality.TextureQuality]);
	ShadowQuality->SetSelectedItem(GeneralScalabilityList[DetectedQuality.ShadowQuality]);
	EffectQuality->SetSelectedItem(GeneralScalabilityList[DetectedQuality.EffectsQuality]);
	PPQuality->SetSelectedItem(GeneralScalabilityList[DetectedQuality.PostProcessQuality]);

	int32 AAModeInt = UUTGameUserSettings::ConvertAAScalabilityQualityToAAMode(DetectedQuality.AntiAliasingQuality);
	int32 AAModeSelection = ConvertAAModeToComboSelection(AAModeInt);
	AAMode->SetSelectedItem(AAModeList[AAModeSelection]);

	int32 ScreenPercentage = DetectedQuality.ResolutionQuality;
	ScreenPercentage = int32(FMath::Clamp(float(ScreenPercentage), ScreenPercentageRange.X, ScreenPercentageRange.Y));
	float ScreenPercentageSliderSetting = (float(ScreenPercentage) - ScreenPercentageRange.X) / (ScreenPercentageRange.Y - ScreenPercentageRange.X);
	ScreenPercentageSlider->SetValue(ScreenPercentageSliderSetting);
	ScreenPercentageLabel->SetText(GetScreenPercentageLabelText(ScreenPercentageSliderSetting));
}

FReply SUWSystemSettingsDialog::OnAutodetectClick()
{
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	UUTLocalPlayer* LocalPlayer = GetPlayerOwner().Get();
	if (ensure(LocalPlayer))
	{
		UserSettings->BenchmarkDetailSettings(LocalPlayer, false);
	}

	return FReply::Handled();
}

FReply SUWSystemSettingsDialog::OKClick()
{
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	// sound settings
	for (int32 i = 0; i < ARRAY_COUNT(SoundVolumes); i++)
	{
		UserSettings->SetSoundClassVolume(EUTSoundClass::Type(i), SoundVolumes[i]->GetValue());
	}
	// engine scalability
	UserSettings->ScalabilityQuality.TextureQuality = GeneralScalabilityList.Find(TextureRes->GetSelectedItem());
	UserSettings->ScalabilityQuality.ShadowQuality = GeneralScalabilityList.Find(ShadowQuality->GetSelectedItem());
	UserSettings->ScalabilityQuality.PostProcessQuality = GeneralScalabilityList.Find(PPQuality->GetSelectedItem());
	UserSettings->ScalabilityQuality.EffectsQuality = GeneralScalabilityList.Find(EffectQuality->GetSelectedItem());
	Scalability::SetQualityLevels(UserSettings->ScalabilityQuality);
	Scalability::SaveState(GGameUserSettingsIni);
	// resolution
	GetPlayerOwner()->ViewportClient->ConsoleCommand(*FString::Printf(TEXT("setres %s%s"), *SelectedRes->GetText().ToString(), Fullscreen->IsChecked() ? TEXT("f") : TEXT("w")));

	UserSettings->SetAAMode(ConvertComboSelectionToAAMode(*AAMode->GetSelectedItem().Get()));

	// Increments of 5, so divide by 5 and multiply by 5
	int32 NewScreenPercentage = FMath::TruncToInt(ScreenPercentageSlider->GetValue() * (ScreenPercentageRange.Y - ScreenPercentageRange.X) + ScreenPercentageRange.X) / 5 * 5;
	UserSettings->SetScreenPercentage(NewScreenPercentage);

	const TCHAR* Cmd = *SelectedRes->GetText().ToString();
	int32 X=FCString::Atoi(Cmd);
	const TCHAR* CmdTemp = FCString::Strchr(Cmd,'x') ? FCString::Strchr(Cmd,'x')+1 : FCString::Strchr(Cmd,'X') ? FCString::Strchr(Cmd,'X')+1 : TEXT("");
	int32 Y=FCString::Atoi(CmdTemp);
	UserSettings->SetScreenResolution(FIntPoint(X, Y));
	UserSettings->SetFullscreenMode(Fullscreen->IsChecked() ? EWindowMode::Fullscreen : EWindowMode::Windowed);
	UserSettings->SaveConfig();

	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (UTEngine == NULL) // PIE
	{
		UTEngine = UUTGameEngine::StaticClass()->GetDefaultObject<UUTGameEngine>();
	}
	if (FrameRateCap->GetText().ToString().IsNumeric())
	{
		UTEngine->FrameRateCap = FCString::Atoi(*FrameRateCap->GetText().ToString());
	}
	GEngine->bSmoothFrameRate = SmoothFrameRate->IsChecked();
	GEngine->SaveConfig();
	UTEngine->SaveConfig();

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

	// impact effect lifetime - note that 1.0 on the slider is infinite lifetime
	float NewDecalLifetime = (DecalLifetime->GetValue() * (DecalLifetimeRange.Y - DecalLifetimeRange.X) + DecalLifetimeRange.X);
	AUTWorldSettings* DefaultWS = AUTWorldSettings::StaticClass()->GetDefaultObject<AUTWorldSettings>();
	DefaultWS->MaxImpactEffectVisibleLifetime = NewDecalLifetime;
	DefaultWS->MaxImpactEffectInvisibleLifetime = NewDecalLifetime * 0.5f;
	DefaultWS->SaveConfig();
	if (GetPlayerOwner()->PlayerController != NULL)
	{
		AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetPlayerOwner()->PlayerController->GetWorld()->GetWorldSettings());
		if (WS != NULL)
		{
			WS->MaxImpactEffectVisibleLifetime = DefaultWS->MaxImpactEffectVisibleLifetime;
			WS->MaxImpactEffectInvisibleLifetime = DefaultWS->MaxImpactEffectInvisibleLifetime;
		}
	}
	
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

FReply SUWSystemSettingsDialog::CancelClick()
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
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

void SUWSystemSettingsDialog::OnAAModeSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedAAMode->SetText(*NewSelection.Get());
}

int32 SUWSystemSettingsDialog::ConvertAAModeToComboSelection(int32 AAMode)
{
	// 0:off, 1:very low (faster FXAA), 2:low (FXAA), 3:medium (faster TemporalAA), 4:high (default TemporalAA)
	if (AAMode == 0)
	{
		return 0;
	}
	else if (AAMode == 1 || AAMode == 2)
	{
		return 1;
	}
	else
	{
		return 2;
	}
}

int32 SUWSystemSettingsDialog::ConvertComboSelectionToAAMode(const FString& Selection)
{
	// 0:off, 1:very low (faster FXAA), 2:low (FXAA), 3:medium (faster TemporalAA), 4:high (default TemporalAA)
	if (Selection == *AAModeList[0].Get())
	{
		return 0;
	}
	else if (Selection == *AAModeList[1].Get())
	{
		return 2;
	}
	else
	{
		return 4;
	}
}

FReply SUWSystemSettingsDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) OKClick();
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL) CancelClick();
	return FReply::Handled();
}

#endif