// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "SUWDialog.h"
#include "UTPlayerInput.h"
#include "SKeyBind.h"
#include "SNumericEntryBox.h"

struct FSimpleBind
{
	FString DisplayName;
	TSharedPtr<FKey> Key;
	TSharedPtr<FKey> AltKey;
	FKey DefaultKey;
	FKey DefaultAltKey;
	TSharedPtr<SKeyBind> KeyWidget;
	TSharedPtr<SKeyBind> AltKeyWidget;
	bool bHeader;

	explicit FSimpleBind(const FText& InDisplayName);

	TArray<FInputActionKeyMapping> ActionMappings;
	TArray<FInputAxisKeyMapping> AxisMappings;
	TArray<FCustomKeyBinding>  CustomBindings;

	FSimpleBind* AddMapping(const FString& Mapping, float Scale = 0.0f);
	FSimpleBind* AddDefaults(FKey InDefaultKey, FKey InDefaultAltKey = FKey())
	{
		DefaultKey = InDefaultKey;
		DefaultAltKey = InDefaultAltKey;
		return this;
	}
	FSimpleBind* MakeHeader()
	{
		bHeader = true;
		return this;
	}
	void WriteBind() const;
};

class SUWControlSettingsDialog : public SUWDialog
{
public:
	void Construct(const FArguments& InArgs);
protected:

	void CreateBinds();
	SVerticalBox::FSlot& AddKeyBind(TSharedPtr<FSimpleBind> Bind);
	SVerticalBox::FSlot& AddHeader(const FString& Header);

	TArray<TSharedPtr<FSimpleBind> > Binds;

	FReply OKClick();
	FReply CancelClick();
	FReply OnBindDefaultClick();

	TSharedPtr<SVerticalBox> ControlList;
	TSharedPtr<SScrollBox> ScrollBox;
	TSharedPtr<SWidgetSwitcher> TabWidget;
	FReply OnTabClickKeyboard()
	{
		TabWidget->SetActiveWidgetIndex(0);
		return FReply::Handled();
	}
	FReply OnTabClickMouse()
	{
		TabWidget->SetActiveWidgetIndex(1);
		return FReply::Handled();
	}
	FReply OnTabClickMovement()
	{
		TabWidget->SetActiveWidgetIndex(2);
		return FReply::Handled();
	}

	//mouse settings
	TSharedPtr<SSlider> MouseSensitivity;
	TSharedPtr<SCheckBox> MouseSmoothing;
	TSharedPtr<SCheckBox> MouseInvert;
	/** range of values passed to PlayerInput->SetMouseSensitivity() which will be normalized to 0.0 - 1.0 for the slider widget */
	FVector2D MouseSensitivityRange;

	//movement settings
	TSharedPtr<SCheckBox> SingleTapWallDodge;
	TSharedPtr<SNumericEntryBox<float> > MaxDodgeClickTime;
	TSharedPtr<SNumericEntryBox<float> > MaxDodgeTapTime;

	float MaxDodgeClickTimeValue;
	float MaxDodgeTapTimeValue;

	void SetMaxDodgeClickTimeValue(float InMaxDodgeClickTimeValue, ETextCommit::Type)
	{
		MaxDodgeClickTimeValue = InMaxDodgeClickTimeValue;
	}
	void SetMaxDodgeTapTimeValue(float InMaxDodgeTapTimeValue, ETextCommit::Type)
	{
		MaxDodgeTapTimeValue = InMaxDodgeTapTimeValue;
	}
	TOptional<float> GetMaxDodgeClickTimeValue() const
	{
		return TOptional<float>(MaxDodgeClickTimeValue);
	}
	TOptional<float> GetMaxDodgeTapTimeValue() const
	{
		return TOptional<float>(MaxDodgeTapTimeValue);
	}

};