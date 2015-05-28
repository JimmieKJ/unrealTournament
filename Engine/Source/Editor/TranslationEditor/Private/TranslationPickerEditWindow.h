// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SCompoundWidget.h"

#define LOCTEXT_NAMESPACE "TranslationPicker"

/** Translation picker edit Widget to handle the display and editing of a single selected FText */
class STranslationPickerEditWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(STranslationPickerEditWidget) {}

	SLATE_ARGUMENT(FText, PickedText)

	SLATE_ARGUMENT(bool, bAllowEditing)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Return the translation unit for this text, with any modifications */
	UTranslationUnit* GetTranslationUnitWithAnyChanges();

private:

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	FReply SaveAndPreview();

	/** The FText that we are using this widget to translate */
	FText PickedText;

	/** The translation we're editing represented in a UTranslationUnit object */
	UTranslationUnit* TranslationUnit;

	/** The text box for entering/modifying a translation */
	TSharedPtr<SMultiLineEditableTextBox> TextBox;

	/** Whether or not to show the save button*/
	bool bAllowEditing;
};

/** Translation picker edit window to allow you to translate selected FTexts in place */
class STranslationPickerEditWindow : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(STranslationPickerEditWindow) {}

	SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)

	SLATE_ARGUMENT(TArray<FText>, PickedTexts)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Default dimensions of the Translation Picker edit window (floating window also uses these sizes, so it matches roughly)
	static const int32 DefaultEditWindowWidth;
	static const int32 DefaultEditWindowHeight;

private:

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

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

	/** All of our current edit widgets */
	TArray<TSharedRef<STranslationPickerEditWidget>> EditWidgets;

	/** Save all translations and close */
	FReply SaveAllAndClose();
};

#undef LOCTEXT_NAMESPACE