// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SWidgetSwitcher.h"
#include "SUWDialog.h"
#include "Widgets/SUTTabButton.h"
#include "UTAudioSettings.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUWSystemSettingsDialog : public SUWDialog
{
public:
	SLATE_BEGIN_ARGS(SUWSystemSettingsDialog)
	: _DialogSize(FVector2D(1000,900))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	, _ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_ARGUMENT(FText, DialogTitle)											
	SLATE_ARGUMENT(FVector2D, DialogSize)										
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)									
	SLATE_ARGUMENT(FVector2D, DialogPosition)									
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)								
	SLATE_ARGUMENT(FVector2D, ContentPadding)									
	SLATE_ARGUMENT(uint16, ButtonMask)
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)							
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
protected:

	TArray< TSharedPtr<FString> > ResList;
	TSharedPtr<STextBlock> SelectedRes;
	TArray< TSharedPtr<FString> > DisplayModeList;
	TSharedPtr< SComboBox< TSharedPtr<FString> > > DisplayModeComboBox;
	TSharedPtr<STextBlock> SelectedDisplayMode;
	TSharedPtr<SCheckBox> SmoothFrameRate;
	TSharedPtr<SCheckBox> VSync;
	TSharedPtr<SEditableTextBox> FrameRateCap;

	TSharedPtr<SCheckBox> HRTFCheckBox;
	TSharedPtr<SSlider> SoundVolumes[EUTSoundClass::MAX];
	TSharedPtr<STextBlock> SoundVolumesLabels[EUTSoundClass::MAX];
	void OnSoundVolumeChangedMaster(float NewValue);
	void OnSoundVolumeChangedMusic(float NewValue);
	void OnSoundVolumeChangedSFX(float NewValue);
	void OnSoundVolumeChangedVoice(float NewValue);

	/** list of display values for general scalability setting that are all set the same way (e.g. low/medium/high) */
	TArray< TSharedPtr<FString> > GeneralScalabilityList;
	TSharedPtr< SComboBox< TSharedPtr<FString> > > TextureRes;
	TSharedPtr<STextBlock> SelectedTextureRes;
	TSharedPtr< SComboBox< TSharedPtr<FString> > > ShadowQuality;
	TSharedPtr<STextBlock> SelectedShadowQuality;
	TSharedPtr< SComboBox< TSharedPtr<FString> > > PPQuality;
	TSharedPtr<STextBlock> SelectedPPQuality;
	TSharedPtr< SComboBox< TSharedPtr<FString> > > EffectQuality;
	TSharedPtr<STextBlock> SelectedEffectQuality;
	TSharedPtr<SSlider> DecalLifetime;
	TSharedPtr<STextBlock> DecalLifetimeLabel;

	TArray< TSharedPtr<FString> > AAModeList;
	TSharedPtr< SComboBox< TSharedPtr<FString> > > AAMode;
	TSharedPtr<STextBlock> SelectedAAMode;

	TSharedPtr<SSlider> ScreenPercentageSlider;
	TSharedPtr<STextBlock> ScreenPercentageLabel;
	FVector2D ScreenPercentageRange;

	/** range of values passed to PlayerInput->SetMouseSensitivity() which will be normalized to 0.0 - 1.0 for the slider widget */
	FVector2D MouseSensitivityRange;
	/** range of values for decal lifetime which will be normalized to 0.0 - 1.0 for the slider widget
	 * note that the max value (1.0 on the slider) becomes infinite lifetime
	 */
	FVector2D DecalLifetimeRange;

	TSharedPtr<SWidgetSwitcher> TabWidget;
	TSharedPtr<SUTTabButton> GeneralSettingsTabButton;
	TSharedPtr<SUTTabButton> GraphicsSettingsTabButton;
	TSharedPtr<SUTTabButton> AudioSettingsTabButton;

	TSharedRef<SWidget> BuildGeneralTab();
	TSharedRef<SWidget> BuildGraphicsTab();
	TSharedRef<SWidget> BuildAudioTab();

	FReply OnTabClickGeneral();
	FReply OnTabClickGraphics();
	FReply OnTabClickAudio();

	virtual FReply OnButtonClick(uint16 ButtonID);	

	FReply OnAutodetectClick();
	FReply OKClick();
	FReply CancelClick();
	void OnResolutionSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnDisplayModeSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnTextureResolutionSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnShadowQualitySelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnPPQualitySelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnEffectQualitySelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);


	void OnAAModeSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	int32 ConvertAAModeToComboSelection(int32 AAMode);
	int32 ConvertComboSelectionToAAMode(const FString& Selection);

	void OnDecalLifetimeChange(float NewValue);
	FString GetDecalLifetimeLabelText(float SliderValue);

	void OnScreenPercentageChange(float NewValue);
	FString GetScreenPercentageLabelText(float SliderValue);

	void OnSettingsAutodetected(const Scalability::FQualityLevels& DetectedQuality);

	FText GetVSyncText() const;

	SVerticalBox::FSlot& AddSectionHeader(const FText& SectionDesc);
	SVerticalBox::FSlot& AddGeneralScalabilityWidget(const FString& Desc, TSharedPtr< SComboBox< TSharedPtr<FString> > >& ComboBox, TSharedPtr<STextBlock>& SelectedItemWidget, void (SUWSystemSettingsDialog::*SelectionFunc)(TSharedPtr<FString>, ESelectInfo::Type), int32 SettingValue, const TAttribute<FText>& TooltipText = TAttribute<FText>());
	SVerticalBox::FSlot& AddAAModeWidget(const FString& Desc, TSharedPtr< SComboBox< TSharedPtr<FString> > >& ComboBox, TSharedPtr<STextBlock>& SelectedItemWidget, void (SUWSystemSettingsDialog::*SelectionFunc)(TSharedPtr<FString>, ESelectInfo::Type), int32 SettingValue, const TAttribute<FText>& TooltipText = TAttribute<FText>());
	SVerticalBox::FSlot& AddGeneralSliderWidget(const FString& Desc, TSharedPtr<SSlider>& SliderWidget, float SettingValue, const TAttribute<FText>& TooltipText = TAttribute<FText>());

	SVerticalBox::FSlot& AddGeneralSliderWithLabelWidget(TSharedPtr<SSlider>& SliderWidget, TSharedPtr<STextBlock>& LabelWidget, void(SUWSystemSettingsDialog::*SelectionFunc)(float), const FString& InitialLabel, float SettingValue, const TAttribute<FText>& TooltipText = TAttribute<FText>());
};

#endif