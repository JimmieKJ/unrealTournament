// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "../Base/SUTDialogBase.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTScreenshotConfigDialog : public SUTDialogBase
{
	SLATE_BEGIN_ARGS(SUTScreenshotConfigDialog)
	: _DialogSize(FVector2D(700, 400))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f, 0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f, 0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	, _ButtonMask(UTDIALOG_BUTTON_OK)
	, _IsSuppressible(false)
	, _MessageTextStyleName(TEXT("UT.Common.NormalText"))

	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)
		SLATE_ARGUMENT(FVector2D, DialogSize)
		SLATE_ARGUMENT(bool, bDialogSizeIsRelative)
		SLATE_ARGUMENT(FVector2D, DialogPosition)
		SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)
		SLATE_ARGUMENT(FVector2D, ContentPadding)
		SLATE_ARGUMENT(uint16, ButtonMask)
		SLATE_EVENT(FDialogResultDelegate, OnDialogResult)
		SLATE_ARGUMENT(bool, IsSuppressible)
		SLATE_ARGUMENT(FString, MessageTextStyleName)
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	FReply OnButtonClick(uint16 ButtonID);

	TArray<TSharedPtr<FString>> ResolutionList;
	TSharedPtr< SComboBox< TSharedPtr<FString> > > ResolutionComboBox;
	TSharedPtr<STextBlock> SelectedResolution;
	void OnResolutionSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	TSharedPtr< SCheckBox > CustomPostCheckBox;
	TSharedPtr< SSlider > BloomIntensitySlider;
	TSharedPtr< SSlider > MotionBlurMaxSlider;
	TSharedPtr< SSlider > MotionBlurAmountSlider;
	TSharedPtr< SSlider > DOFScaleSlider;
	TSharedPtr< SSlider > DOFDistanceSlider;
	TSharedPtr< SSlider > DOFAmountSlider;
	TSharedPtr< SSlider > DOFNearBlurSlider;
	TSharedPtr< SSlider > DOFFarBlurSlider;
	
	FString TempSelectedRes;
	void ScreenshotConfirm(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonID);
};

#endif