// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SCompoundWidget.h"

#define LOCTEXT_NAMESPACE "TranslationPicker"

/** Translation picker floating window to show details of FText(s) under cursor, and allow in-place translation via TranslationPickerEditWindow */
class STranslationPickerFloatingWindow : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(STranslationPickerFloatingWindow) {}

	SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Pull the FText reference out of an SWidget */
	FText GetTextDescription(TSharedRef<SWidget> Widget);

	/** Format an FText for display */
	FText FormatFTextInfo(FText TextToFormat);

	/** This is what this window displays */
	FText GetPickerStatusText() const
	{
		return FText::Format(LOCTEXT("TootipHint", "{0}\n\n(Esc to pick)"), TranslationInfoPreviewText);
	}

	/** Handle key presses */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Close this window */
	FReply Close();

	/** We need to support keyboard focus to process the 'Esc' key */
	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	/** Handle to the window that contains this widget */
	TWeakPtr<SWindow> ParentWindow;

	/** Contents of the window */
	TSharedPtr<SBox> WindowContents;

	/** The FTexts that we have found under the cursor */
	TArray<FText> PickedTexts;

	/** Preview text for the translation */
	FText TranslationInfoPreviewText;
};

#undef LOCTEXT_NAMESPACE