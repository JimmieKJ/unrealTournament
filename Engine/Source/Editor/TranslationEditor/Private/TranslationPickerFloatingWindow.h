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
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	/** Pull the FText reference out of an SWidget */
	FText GetTextFromWidget(TSharedRef<SWidget> Widget);

	/** Pull the FText reference out of the child widgets of an SWidget */
	void GetTextFromChildWidgets(TSharedRef<SWidget> Widget);

	/** Handle key presses */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** We need to support keyboard focus to process the 'Esc' key */
	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	/** Handle to the window that contains this widget */
	TWeakPtr<SWindow> ParentWindow;

	/** Contents of the window */
	TSharedPtr<SToolTip> WindowContents;

	/** The FTexts that we have found under the cursor */
	TArray<FText> PickedTexts;

	/**
	* The path widgets we were hovering over last tick
	*/
	FWeakWidgetPath LastTickHoveringWidgetPath;
};

#undef LOCTEXT_NAMESPACE