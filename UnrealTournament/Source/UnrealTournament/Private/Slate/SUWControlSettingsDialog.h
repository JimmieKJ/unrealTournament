// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SWidgetSwitcher.h"
#include "SUWDialog.h"
#include "UTPlayerInput.h"
#include "SKeyBind.h"
#include "SNumericEntryBox.h"
#include "Widgets/SUTTabButton.h"

#if !UE_SERVER

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
	void WriteBind();
};

class UNREALTOURNAMENT_API SUWControlSettingsDialog : public SUWDialog
{
public:

	SLATE_BEGIN_ARGS(SUWControlSettingsDialog)
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

	virtual TSharedRef<class SWidget> BuildCustomButtonBar();

	void CreateBinds();

	TArray<TSharedPtr<FSimpleBind> > Binds;

	virtual FReply OnButtonClick(uint16 ButtonID);	

	FReply OKClick();
	FReply CancelClick();
	FReply OnBindDefaultClick();

	TSharedPtr<SVerticalBox> ControlList;
	TSharedPtr<SScrollBox> ScrollBox;
	TSharedPtr<SWidgetSwitcher> TabWidget;
	TSharedPtr<SButton> ResetToDefaultsButton;
	FReply OnTabClickKeyboard()
	{
		ResetToDefaultsButton->SetVisibility(EVisibility::All);
		TabWidget->SetActiveWidgetIndex(0);
		MovementSettingsTabButton->UnPressed();
		KeyboardSettingsTabButton->BePressed();
		MouseSettingsTabButton->UnPressed();

		return FReply::Handled();
	}
	FReply OnTabClickMouse()
	{
		ResetToDefaultsButton->SetVisibility(EVisibility::Hidden);
		TabWidget->SetActiveWidgetIndex(1);
		MovementSettingsTabButton->UnPressed();
		KeyboardSettingsTabButton->UnPressed();
		MouseSettingsTabButton->BePressed();
		return FReply::Handled();
	}
	FReply OnTabClickMovement()
	{
		TabWidget->SetActiveWidgetIndex(2);
		ResetToDefaultsButton->SetVisibility(EVisibility::Hidden);
		MovementSettingsTabButton->BePressed();
		KeyboardSettingsTabButton->UnPressed();
		MouseSettingsTabButton->UnPressed();
		return FReply::Handled();
	}

	//mouse settings
	TSharedPtr<SSlider> MouseSensitivity;
	TSharedPtr<SCheckBox> MouseSmoothing;
	TSharedPtr<SCheckBox> MouseInvert;
	TSharedPtr<SEditableTextBox> MouseSensitivityEdit;
	/** range of values passed to PlayerInput->SetMouseSensitivity() which will be normalized to 0.0 - 1.0 for the slider widget */
	FVector2D MouseSensitivityRange;
	void EditSensitivity(const FText& Input, ETextCommit::Type);

	TSharedPtr<SCheckBox> MouseAccelerationCheckBox;
	TSharedPtr<SEditableTextBox> MouseAccelerationEdit;
	TSharedPtr<SSlider> MouseAcceleration;
	FVector2D MouseAccelerationRange;
	void EditAcceleration(const FText& Input, ETextCommit::Type);

	TSharedPtr<SCheckBox> MouseAccelerationMaxCheckBox;
	TSharedPtr<SEditableTextBox> MouseAccelerationMaxEdit;
	TSharedPtr<SSlider> MouseAccelerationMax;
	FVector2D MouseAccelerationMaxRange;
	void EditAccelerationMax(const FText& Input, ETextCommit::Type);

	//movement settings
	TSharedPtr<SCheckBox> SingleTapWallDodge;
	TSharedPtr<SCheckBox> TapCrouchToSlide;
	TSharedPtr<SCheckBox> SingleTapAfterJump;
	TSharedPtr<SCheckBox> AutoSlide;
	TSharedPtr<SNumericEntryBox<float> > MaxDodgeClickTime;
	TSharedPtr<SNumericEntryBox<float> > MaxDodgeTapTime;
	TSharedPtr<SCheckBox> DeferFireInput;


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

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void OnKeyBindingChanged( FKey PreviousKey, FKey NewKey, TSharedPtr<FSimpleBind> BindingThatChanged, bool bPrimaryKey );

	TSharedPtr <SUTTabButton> KeyboardSettingsTabButton;
	TSharedPtr <SUTTabButton> MouseSettingsTabButton;
	TSharedPtr <SUTTabButton> MovementSettingsTabButton;
	
	TSharedRef<SWidget> BuildKeyboardTab();
	TSharedRef<SWidget> BuildMouseTab();
	TSharedRef<SWidget> BuildMovementTab();
};

#endif
