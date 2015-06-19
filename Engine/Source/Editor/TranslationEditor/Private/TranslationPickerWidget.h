// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SDocumentationToolTip.h"
#include "TranslationPickerFloatingWindow.h"

#define LOCTEXT_NAMESPACE "TranslationPicker"

class TranslationPickerManager
{
public:
	static TSharedPtr<SWindow> PickerWindow;
	static TSharedPtr<STranslationPickerFloatingWindow> PickerWindowWidget;

	static bool IsPickerWindowOpen() { return PickerWindow.IsValid(); }

	static bool OpenPickerWindow();

	static void ClosePickerWindow();
};

/** Widget used to launch a 'picking' session */
class STranslationWidgetPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STranslationWidgetPicker) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	/**
	* Called by slate to determine if this button should appear checked
	*
	* @return ECheckBoxState::Checked if it should be checked, ECheckBoxState::Unchecked if not.
	*/
	ECheckBoxState IsChecked() const;

	/**
	* Called by Slate when this tool bar check box button is toggled
	*/
	void OnCheckStateChanged(const ECheckBoxState NewCheckedState);
};

#undef LOCTEXT_NAMESPACE