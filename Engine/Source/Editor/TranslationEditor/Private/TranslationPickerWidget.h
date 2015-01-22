// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SDocumentationToolTip.h"
#include "TranslationPickerFloatingWindow.h"

#define LOCTEXT_NAMESPACE "TranslationPicker"

/** Widget used to launch a 'picking' session */
class STranslationWidgetPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STranslationWidgetPicker) {}

	SLATE_END_ARGS()

	~STranslationWidgetPicker();

	void Construct(const FArguments& InArgs);

private:
	FReply OnClicked();

	bool IsPicking() const;

	/** Picker window widget */
	TWeakPtr<STranslationPickerFloatingWindow> PickerWindowWidget;

	/** Picker window */
	TWeakPtr<SWindow> PickerWindow;

	/**
	* Called by slate to determine if this button should appear checked
	*
	* @return ECheckBoxState::Checked if it should be checked, ECheckBoxState::Unchecked if not.
	*/
	ECheckBoxState OnIsChecked() const;

	/**
	* Called by Slate when this tool bar check box button is toggled
	*/
	void OnCheckStateChanged(const ECheckBoxState NewCheckedState);
};

#undef LOCTEXT_NAMESPACE