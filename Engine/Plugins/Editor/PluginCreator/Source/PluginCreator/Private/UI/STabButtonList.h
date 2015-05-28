// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"

/**
*	A container for TabButtons.
*	Handles switching between 'tabs'.*/

class STabButtonList : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STabButtonList)
	{}
		SLATE_ARGUMENT(TArray<FText>, ButtonLabels)
		SLATE_EVENT(FOnTextChanged, OnTabButtonSelectionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void AddTabButton(FText ButtonLabel);

	FReply OnButtonClicked(FText InLabel);

	bool IsButtonActive(FText InLabel);

	FOnTextChanged OnTabButtonSelectionChanged;
	TArray<FText> ButtonLabels;
	TSharedPtr<SWrapBox> TabsContainer;

	FText CurrentTabButton;
};

