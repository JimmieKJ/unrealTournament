// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "SWidgetSwitcher.h"
#include "SUTTabButton.h"
#include "../SUWindowsStyle.h"
#if !UE_SERVER

class UNREALTOURNAMENT_API SUTTabWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUTTabWidget)
		: _TabTextStyle(&SUWindowsStyle::Get().GetWidgetStyle< FTextBlockStyle >("UT.TopMenu.Button.SmallTextStyle"))
	{}
		SLATE_EVENT(FOnTextChanged, OnTabButtonSelectionChanged)
		SLATE_EVENT(FOnInt32ValueChanged, OnTabButtonNumberChanged)

		/** The text style of the Tab button */
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TabTextStyle)

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	void AddTab(FText ButtonLabel, TSharedPtr<SWidget> Widget);

	void SelectTab(int32 NewTab);

	FReply OnButtonClicked(FText InLabel);

protected:

	FOnTextChanged OnTabButtonSelectionChanged;
	FOnInt32ValueChanged OnTabButtonNumberChanged;

	TArray<FText> ButtonLabels;
	TArray<TSharedPtr<SUTTabButton> > TabButtons;
	TSharedPtr<SWrapBox> TabsContainer;
	TSharedPtr<SWidgetSwitcher> TabSwitcher;

	FText CurrentTabButton;

	const FTextBlockStyle* TabTextStyle;
};

#endif