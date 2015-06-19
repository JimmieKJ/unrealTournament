// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"
#include "PropertyEditorDelegates.h"

class ULocalizationTargetSet;
class ULocalizationTarget;

class SLocalizationTargetEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLocalizationTargetEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, ULocalizationTargetSet* const InProjectSettings, ULocalizationTarget* const InTarget, const FIsPropertyEditingEnabled& IsPropertyEditingEnabled);
};

