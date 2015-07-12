// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "SWidgetSwitcher.h"
#include "SUTTabButton.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTTabWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUTTabWidget)
	{}
		SLATE_EVENT(FOnTextChanged, OnTabButtonSelectionChanged)
		SLATE_EVENT(FOnInt32ValueChanged, OnTabButtonNumberChanged)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	void AddTab(FText ButtonLabel, TSharedPtr<SWidget> Widget);

	FReply OnButtonClicked(FText InLabel);

protected:

	FOnTextChanged OnTabButtonSelectionChanged;
	FOnInt32ValueChanged OnTabButtonNumberChanged;

	TArray<FText> ButtonLabels;
	TArray<TSharedPtr<SUTTabButton> > TabButtons;
	TSharedPtr<SWrapBox> TabsContainer;
	TSharedPtr<SWidgetSwitcher> TabSwitcher;

	FText CurrentTabButton;

};

#endif