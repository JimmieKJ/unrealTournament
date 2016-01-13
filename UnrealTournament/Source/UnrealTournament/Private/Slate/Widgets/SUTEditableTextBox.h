// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "../SUTStyle.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTEditableTextBox : public SEditableTextBox
{
	SLATE_BEGIN_ARGS(SUTEditableTextBox)
		: _Style(&FCoreStyle::Get().GetWidgetStyle< FEditableTextBoxStyle >("NormalEditableTextBox"))
		, _Text()
		, _HintText()
		, _Font()
		, _ForegroundColor()
		, _ReadOnlyForegroundColor()
		, _IsReadOnly( false )
		, _IsPassword( false )
		, _IsCaretMovedWhenGainFocus ( true )
		, _SelectAllTextWhenFocused( false )
		, _RevertTextOnEscape( false )
		, _ClearKeyboardFocusOnCommit( true )
		, _MinDesiredWidth( 0.0f )
		, _SelectAllTextOnCommit( false )
		, _BackgroundColor()		
		, _Padding()
		, _ErrorReporting()
		{}

		/** The styling of the textbox */
		SLATE_STYLE_ARGUMENT( FEditableTextBoxStyle, Style )

		/** Sets the text content for this editable text box widget */
		SLATE_ATTRIBUTE( FText, Text )

		/** Hint text that appears when there is no text in the text box */
		SLATE_ATTRIBUTE( FText, HintText )

		/** Font color and opacity (overrides Style) */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )

		/** Text color and opacity (overrides Style) */
		SLATE_ATTRIBUTE( FSlateColor, ForegroundColor )
		
		/** Text color and opacity when read-only (overrides Style) */
		SLATE_ATTRIBUTE( FSlateColor, ReadOnlyForegroundColor )

		/** Sets whether this text box can actually be modified interactively by the user */
		SLATE_ATTRIBUTE( bool, IsReadOnly )

		/** Sets whether this text box is for storing a password */
		SLATE_ATTRIBUTE( bool, IsPassword )

		/** Workaround as we loose focus when the auto completion closes. */
		SLATE_ATTRIBUTE( bool, IsCaretMovedWhenGainFocus )

		/** Whether to select all text when the user clicks to give focus on the widget */
		SLATE_ATTRIBUTE( bool, SelectAllTextWhenFocused )

		/** Whether to allow the user to back out of changes when they press the escape key */
		SLATE_ATTRIBUTE( bool, RevertTextOnEscape )

		/** Whether to clear keyboard focus when pressing enter to commit changes */
		SLATE_ATTRIBUTE( bool, ClearKeyboardFocusOnCommit )

		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT( FOnTextChanged, OnTextChanged )

		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )

		/** Minimum width that a text block should be */
		SLATE_ATTRIBUTE( float, MinDesiredWidth )

		/** Whether to select all text when pressing enter to commit changes */
		SLATE_ATTRIBUTE( bool, SelectAllTextOnCommit )

		/** The color of the background/border around the editable text (overrides Style) */
		SLATE_ATTRIBUTE( FSlateColor, BackgroundColor )

		/** Padding between the box/border and the text widget inside (overrides Style) */
		SLATE_ATTRIBUTE( FMargin, Padding )

		/** Provide a alternative mechanism for error reporting. */
		SLATE_ARGUMENT( TSharedPtr<class IErrorReportingWidget>, ErrorReporting )

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void ForceFocus(const FCharacterEvent& InCharacterEvent);
	void JumpToEnd();
protected:

};

#endif