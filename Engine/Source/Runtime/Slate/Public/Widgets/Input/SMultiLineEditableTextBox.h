// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_FANCY_TEXT

/**
 * Editable text box widget
 */
class SLATE_API SMultiLineEditableTextBox : public SBorder
{

public:

	SLATE_BEGIN_ARGS( SMultiLineEditableTextBox )
		: _Style(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
		, _TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
		, _Marshaller()
		, _Text()
		, _HintText()
		, _Font()
		, _ForegroundColor()
		, _ReadOnlyForegroundColor()
		, _Justification(ETextJustify::Left)
		, _LineHeightPercentage(1.0f)
		, _IsReadOnly( false )
		, _IsPassword( false )
		, _IsCaretMovedWhenGainFocus ( true )
		, _SelectAllTextWhenFocused( false )
		, _RevertTextOnEscape( false )
		, _ClearKeyboardFocusOnCommit( true )
		, _AlwaysShowScrollbars( false )
		, _HScrollBar()
		, _VScrollBar()
		, _WrapTextAt(0.0f)
		, _AutoWrapText(false)
		, _SelectAllTextOnCommit( false )
		, _BackgroundColor()		
		, _Padding()
		, _Margin()
		, _ErrorReporting()
		, _ModiferKeyForNewLine(EModifierKey::None)
		{}

		/** The styling of the textbox */
		SLATE_STYLE_ARGUMENT( FEditableTextBoxStyle, Style )

		/** Pointer to a style of the text block, which dictates the font, color, and shadow options. */
		SLATE_STYLE_ARGUMENT( FTextBlockStyle, TextStyle )

		/** The marshaller used to get/set the raw text to/from the text layout. */
		SLATE_ARGUMENT(TSharedPtr< ITextLayoutMarshaller >, Marshaller)

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

		/** How the text should be aligned with the margin. */
		SLATE_ATTRIBUTE(ETextJustify::Type, Justification)

		/** The amount to scale each lines height by. */
		SLATE_ATTRIBUTE(float, LineHeightPercentage)

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

		/** Should we always show the scrollbars (only affects internally created scroll bars) */
		SLATE_ARGUMENT(bool, AlwaysShowScrollbars)

		/** The horizontal scroll bar widget, or null to create one internally */
		SLATE_ARGUMENT( TSharedPtr< SScrollBar >, HScrollBar )

		/** The vertical scroll bar widget, or null to create one internally */
		SLATE_ARGUMENT( TSharedPtr< SScrollBar >, VScrollBar )

		/** Padding around the horizontal scrollbar (overrides Style) */
		SLATE_ATTRIBUTE( FMargin, HScrollBarPadding )

		/** Padding around the vertical scrollbar (overrides Style) */
		SLATE_ATTRIBUTE( FMargin, VScrollBarPadding )

		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT( FOnTextChanged, OnTextChanged )

		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )

		/** Called whenever the horizontal scrollbar is moved by the user */
		SLATE_EVENT( FOnUserScrolled, OnHScrollBarUserScrolled )

		/** Called whenever the vertical scrollbar is moved by the user */
		SLATE_EVENT( FOnUserScrolled, OnVScrollBarUserScrolled )

		/** Called when the cursor is moved within the text area */
		SLATE_EVENT( SMultiLineEditableText::FOnCursorMoved, OnCursorMoved )

		/** Menu extender for the right-click context menu */
		SLATE_EVENT( FMenuExtensionDelegate, ContextMenuExtender )

		/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
		SLATE_ATTRIBUTE( float, WrapTextAt )

		/** Whether to wrap text automatically based on the widget's computed horizontal space.  IMPORTANT: Using automatic wrapping can result
			in visual artifacts, as the the wrapped size will computed be at least one frame late!  Consider using WrapTextAt instead.  The initial 
			desired size will not be clamped.  This works best in cases where the text block's size is not affecting other widget's layout. */
		SLATE_ATTRIBUTE( bool, AutoWrapText )

		/** Whether to select all text when pressing enter to commit changes */
		SLATE_ATTRIBUTE( bool, SelectAllTextOnCommit )

		/** The color of the background/border around the editable text (overrides Style) */
		SLATE_ATTRIBUTE( FSlateColor, BackgroundColor )

		/** Padding between the box/border and the text widget inside (overrides Style) */
		SLATE_ATTRIBUTE( FMargin, Padding )

		/** The amount of blank space left around the edges of text area. 
			This is different to Padding because this area is still considered part of the text area, and as such, can still be interacted with */
		SLATE_ATTRIBUTE( FMargin, Margin )

		/** Provide a alternative mechanism for error reporting. */
		SLATE_ARGUMENT( TSharedPtr<class IErrorReportingWidget>, ErrorReporting )

		/** The optional modifier key necessary to create a newline when typing into the editor. */
		SLATE_ARGUMENT( EModifierKey::Type, ModiferKeyForNewLine)

	SLATE_END_ARGS()
	
	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );
	
	/**
	 * Returns the text string
	 *
	 * @return  Text string
	 */
	const FText& GetText() const
	{
		return EditableText->GetText();
	}

	/**
	 * Sets the text string currently being edited 
	 *
	 * @param  InNewText  The new text string
	 */
	void SetText( const TAttribute< FText >& InNewText );

	/**
	 * Sets the text that appears when there is no text in the text box
	 *
	 * @param  InHintText The hint text string
	 */
	void SetHintText( const TAttribute< FText >& InHintText );

	/**
	 * If InError is a non-empty string the TextBox will the ErrorReporting provided during construction
	 * If no error reporting was provided, the TextBox will create a default error reporter.
	 */
	void SetError( const FText& InError );
	void SetError( const FString& InError );

	// SWidget overrides
	virtual bool SupportsKeyboardFocus() const override;
	virtual bool HasKeyboardFocus() const override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;

	/** Get the currently selected text */
	FText GetSelectedText() const;

	/** Insert the given text at the current cursor position, correctly taking into account new line characters */
	void InsertTextAtCursor(const FText& InText);
	void InsertTextAtCursor(const FString& InString);

	/** Insert the given run at the current cursor position */
	void InsertRunAtCursor(TSharedRef<IRun> InRun);

	/** Move the cursor to the given location in the document */
	void GoTo(const FTextLocation& NewLocation);

	/** Scroll to the given location in the document (without moving the cursor) */
	void ScrollTo(const FTextLocation& NewLocation);

	/** Apply the given style to the currently selected text (or insert a new run at the current cursor position if no text is selected) */
	void ApplyToSelection(const FRunInfo& InRunInfo, const FTextBlockStyle& InStyle);

	/** Get the run currently under the cursor, or null if there is no run currently under the cursor */
	TSharedPtr<const IRun> GetRunUnderCursor() const;

	/** Get the runs currently that are current selected, some of which may be only partially selected */
	const TArray<TSharedRef<const IRun>> GetSelectedRuns() const;

	/** Get the horizontal scroll bar widget */
	TSharedPtr<const SScrollBar> GetHScrollBar() const;

	/** Get the vertical scroll bar widget */
	TSharedPtr<const SScrollBar> GetVScrollBar() const;

	/** Refresh this text box immediately, rather than wait for the usual caching mechanisms to take affect on the text Tick */
	void Refresh();

protected:

	/** Editable text widget */
	TSharedPtr< SMultiLineEditableText > EditableText;

	/** Read-only foreground color */
	TAttribute<FSlateColor> ReadOnlyForegroundColor;

	/** Allows for inserting additional widgets that extend the functionality of the text box */
	TSharedPtr<SHorizontalBox> Box;

	/** SomeWidget reporting */
	TSharedPtr<class IErrorReportingWidget> ErrorReporting;

private:

	/** Styling: border image to draw when not hovered or focused */
	const FSlateBrush* BorderImageNormal;
	/** Styling: border image to draw when hovered */
	const FSlateBrush* BorderImageHovered;
	/** Styling: border image to draw when focused */
	const FSlateBrush* BorderImageFocused;
	/** Styling: border image to draw when read only */
	const FSlateBrush* BorderImageReadOnly;

	/** @return Border image for the text box based on the hovered and focused state */
	const FSlateBrush* GetBorderImage() const;

};


#endif //WITH_FANCY_TEXT