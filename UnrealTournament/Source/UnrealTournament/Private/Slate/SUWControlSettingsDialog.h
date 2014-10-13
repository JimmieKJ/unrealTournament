// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "SUWDialog.h"
#include "UTPlayerInput.h"
#include "SKeyBind.h"
#include "SNumericEntryBox.h"

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
	void WriteBind() const;
};

class SUWControlSettingsDialog : public SUWDialog
{
public:

	SLATE_BEGIN_ARGS(SUWControlSettingsDialog)
	: _DialogSize(FVector2D(0.5f,0.8f))
	, _bDialogSizeIsRelative(true)
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

	void CreateBinds();
	SVerticalBox::FSlot& AddKeyBind(TSharedPtr<FSimpleBind> Bind);
	SVerticalBox::FSlot& AddHeader(const FString& Header);

	TArray<TSharedPtr<FSimpleBind> > Binds;

	virtual FReply OnButtonClick(uint16 ButtonID);	

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
	TSharedPtr<STextBlock> MouseSensitivityText;
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

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
};

#endif