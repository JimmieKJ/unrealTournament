// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"

DECLARE_DELEGATE_RetVal(bool, FIsButtonActive)

/**
*	Very simple implementation of button that will be used to imitate tab behavior.
*	In fact its just a regular button acting like a radio button.*/
class STabButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STabButton)
	{}
		SLATE_ARGUMENT(FText, ButtonLabel)
		SLATE_EVENT(FIsButtonActive, OnIsActive)
		SLATE_EVENT(FOnClicked, OnButtonClicked)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	const FSlateBrush* GetButtonBorderImage() const;

	FLinearColor GetTopbarColor() const;

	FLinearColor ActiveTopbarColor;
	FLinearColor InactiveTopbarColor;

	FIsButtonActive IsActive;
	FOnClicked OnButtonClicked;
};