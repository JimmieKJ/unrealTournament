// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SWidgetSwitcher.h"
#include "SSlider.h"
#include "SCheckBox.h"
#include "SEditableTextBox.h"
#include "../Base/SUTDialogBase.h"
#include "../Widgets/SUTTabButton.h"
#include "UTAudioSettings.h"

#if !UE_SERVER

class UUTProfileSettings;

class SHUDSettingInfo :  public TSharedFromThis<SHUDSettingInfo>
{
private:
	float FloatValue;
	int32 Int32Value;
	bool BoolValue;

	FText Prefix;

	float Min;
	float Max;

public:

	TSharedPtr<SEditableTextBox> EditBox;
	TSharedPtr<SSlider> Slider;
	TSharedPtr<SCheckBox> CheckBox;

	SHUDSettingInfo(float inValue, FText inPrefix, float inMin, float inMax)
	{
		FloatValue = inValue;
		Min = inMin;
		Max = inMax;
		Prefix = inPrefix;
	}

	SHUDSettingInfo(int32 inValue, FText inPrefix, int32 inMin, int32 inMax)
	{
		Int32Value = inValue;
		Min = float(inMin);
		Max = float(inMax);
		Prefix = inPrefix;
	}

	SHUDSettingInfo(bool inValue)
	{
		BoolValue = inValue;
	}

	float GetActualValue_float()
	{
		return FloatValue;
	}

	int32 GetActualValue_int32()
	{
		return Int32Value;
	}

	bool GetActualValue_bool()
	{
		return BoolValue;
	}

	FText GetText_float() const
	{
		FNumberFormattingOptions Options;
		Options.MaximumFractionalDigits = 2;
		Options.MinimumFractionalDigits = 2;

		return Prefix.IsEmpty() ? FText::AsNumber(FloatValue, &Options) : FText::Format(NSLOCTEXT("SUTHUDSettingsDialog","Combier","{0}{1}"), FText::AsNumber(FloatValue, &Options), Prefix);
	}

	void Set_float(float NewValue)
	{
		FloatValue = FMath::Clamp<float>(NewValue, Min, Max);
		if (Slider.IsValid())
		{
			// Figure out the value based on the min/max
			float NewSliderValue = (NewValue - Min) / (Max-Min);
			Slider->SetValue(NewSliderValue);
		}

		FText NewValueText = GetText_float();
		if (EditBox.IsValid() && EditBox->GetText().ToString() != NewValueText.ToString())
		{
			EditBox->SetText(NewValueText);
		}
	}

	float GetSliderValue_float() const
	{
		return FloatValue;
	}

	void TextCommited_float(const FText& NewText, ETextCommit::Type CommitType)
	{
		float NewValue = FCString::Atof( *NewText.ToString());
		Set_float(NewValue);
	}

	void FloatValueChanged(float InSliderValue)
	{
		float NewFloatValue = Min + ((Max - Min) * InSliderValue);
		Set_float(NewFloatValue);
	}

	float GetSliderValue_int32() const
	{
		return float(Int32Value);
	}

	FText GetText_int32() const
	{
		return Prefix.IsEmpty() ? FText::AsNumber(Int32Value) : FText::Format(NSLOCTEXT("SUTHUDSettingsDialog","Combier","{0}{1}"), FText::AsNumber(Int32Value), Prefix);
	}

	void Set_int32(int32 NewValue)
	{
		Int32Value = FMath::Clamp<int32>(NewValue, int32(Min), int32(Max));
		if (Slider.IsValid())
		{
			float NewSliderValue = (float(NewValue) - Min) / (Max-Min);
			Slider->SetValue(float(NewSliderValue));
		}

		FText NewValueText = GetText_int32();
		if (EditBox.IsValid() && EditBox->GetText().ToString() != NewValueText.ToString())
		{
			EditBox->SetText(NewValueText);
		}
	}


	void TextCommited_int32(const FText& NewText, ETextCommit::Type CommitType)
	{
		int32 NewValue = FCString::Atoi( *NewText.ToString());
		Set_int32(NewValue);
	}

	void Int32ValueChanged(float InSliderValue)
	{
		int32 NewInt32Value = int32(Min) + int32((Max - Min) * InSliderValue);
		Set_int32(NewInt32Value);
	}

	ECheckBoxState GetCheckBoxState() const
	{
		return BoolValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void CheckboxChanged(ECheckBoxState NewState)
	{
		BoolValue = (NewState == ECheckBoxState::Checked);
	}

};

class UNREALTOURNAMENT_API SUTHUDSettingsDialog : public SUTDialogBase
{
public:
	SLATE_BEGIN_ARGS(SUTHUDSettingsDialog)
	: _DialogTitle(NSLOCTEXT("SUTHUDSettingsDialog","Title","HUD Settings"))
	, _DialogSize(FVector2D(1100, 900))
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
	TSharedRef<class SWidget> BuildCustomButtonBar();
protected:
	SVerticalBox::FSlot& AddFloatOption(FName OptionTag, FText CaptionText, FText ToolTip, FText Prefix, float InitialValue, float Min, float Max);
	SVerticalBox::FSlot& AddIntOption(FName OptionTag, FText CaptionText, FText ToolTip, FText Prefix, int InitialValue, int32 Min, int32 Max);
	SVerticalBox::FSlot& AddBoolOption(FName OptionTag, FText CaptionText, FText ToolTip, bool bInitialValue);

	TMap<FName, TSharedPtr<SHUDSettingInfo>> SettingsInfos;

	TSharedPtr<SWidgetSwitcher> TabWidget;
	TSharedPtr<SUTTabButton> GeneralSettingsTabButton;
	TSharedPtr<SUTTabButton> WeaponBarSettingsTabButton;
	TSharedPtr<SUTTabButton> NotificationsSettingsTabButton;
	TSharedPtr<SUTTabButton> QuickStatsSettingsTabButton;

	TSharedRef<SWidget> BuildGeneralTab();
	TSharedRef<SWidget> BuildWeaponBarTab();
	TSharedRef<SWidget> BuildNotificationsTab();
	TSharedRef<SWidget> BuildQuickStatsTab();

	FReply OnTabClickGeneral();
	FReply OnTabClickWeaponBar();
	FReply OnTabClickNotifications();
	FReply OnTabClickQuickStats();

	void MakeTabs();
	void ApplySettings();
	void ResetSettings();

	FReply OnButtonClick(uint16 ButtonID);
	FReply OKClick();
	FReply CancelClick();
	FReply ResetClick();

	TWeakObjectPtr<UUTProfileSettings> ProfileSettings;

	TArray< TSharedPtr<FString> > QuickStatTypesList;
	TSharedPtr<STextBlock> SelectedLayout;

	TArray< TSharedPtr<FString> > WeaponBarOrientationList;
	TSharedPtr<STextBlock> SelectedWeaponBarOrientation;

	void OnLayoutChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnWeaponBarOrientationChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

private:
	bool bInGame;
};

#endif