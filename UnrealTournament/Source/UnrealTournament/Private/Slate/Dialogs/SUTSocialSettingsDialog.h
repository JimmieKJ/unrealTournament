// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "../Base/SUTDialogBase.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTSocialSettingsDialog : public SUTDialogBase
{
public:
	SLATE_BEGIN_ARGS(SUTSocialSettingsDialog)
	: _DialogTitle(NSLOCTEXT("SUTSocialSettingsDialog","Title","Social Settings"))
	, _DialogSize(FVector2D(500, 300))
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
	SVerticalBox::FSlot& AddSliderWidget(TSharedPtr<STextBlock>& LabelWidget, TSharedPtr<SSlider>& SliderWidget, float StartingValue);

	TSharedPtr<SCheckBox> SuppressToastsInGame;

	FReply OnButtonClick(uint16 ButtonID);
	FReply OKClick();
	FReply CancelClick();

};

#endif