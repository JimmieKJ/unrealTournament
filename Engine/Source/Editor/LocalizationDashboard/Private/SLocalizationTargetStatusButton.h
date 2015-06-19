// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SCompoundWidget.h"
#include "LocalizationTargetTypes.h"
#include "LocalizationConfigurationScript.h"

class SLocalizationTargetStatusButton : public SButton
{
public:
	SLATE_BEGIN_ARGS( SLocalizationTargetStatusButton ){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, ULocalizationTarget& Target);

private:
	const FSlateBrush* GetImageBrush() const;
	FSlateColor GetColorAndOpacity() const;
	FText GetToolTipText() const;
	FReply OnClicked();

private:
	ULocalizationTarget* Target;
};