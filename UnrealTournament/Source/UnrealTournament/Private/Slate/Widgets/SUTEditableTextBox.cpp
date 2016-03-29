// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTEditableTextBox.h"

#if !UE_SERVER

void SUTEditableTextBox::Construct(const FArguments& InArgs)
{
	SEditableTextBox::Construct( SEditableTextBox::FArguments()
		.Style(InArgs._Style)
		.Text(InArgs._Text)
		.HintText(InArgs._HintText)
		.Font(InArgs._Font)
		.ForegroundColor(InArgs._ForegroundColor)
		.ReadOnlyForegroundColor(InArgs._ReadOnlyForegroundColor)
		.IsReadOnly( InArgs._IsReadOnly)
		.IsPassword( InArgs._IsPassword )
		.IsCaretMovedWhenGainFocus ( InArgs._IsCaretMovedWhenGainFocus )
		.SelectAllTextWhenFocused( InArgs._SelectAllTextWhenFocused )
		.RevertTextOnEscape( InArgs._RevertTextOnEscape )
		.ClearKeyboardFocusOnCommit( InArgs._ClearKeyboardFocusOnCommit )
		.MinDesiredWidth( InArgs._MinDesiredWidth )
		.SelectAllTextOnCommit( InArgs._SelectAllTextOnCommit )
		.BackgroundColor(InArgs._BackgroundColor)		
		.Padding(InArgs._Padding)
		.ErrorReporting(InArgs._ErrorReporting)
		.OnTextChanged(InArgs._OnTextChanged)
		.OnTextCommitted(InArgs._OnTextCommitted)
	);
}

void SUTEditableTextBox::ForceFocus(const FCharacterEvent& InCharacterEvent)
{
	if (!HasKeyboardFocus() && EditableText.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(EditableText,EFocusCause::SetDirectly);

		// JoeW FIXME
		//EditableText->TypeChar(InCharacterEvent.GetCharacter());
	}
}

void SUTEditableTextBox::JumpToEnd()
{
	// JoeW FIXME
	/*
	if (EditableText.IsValid())
	{
		EditableText->JumpTo(ETextLocation::EndOfDocument,ECursorAction::MoveCursor);
	}*/
}

#endif