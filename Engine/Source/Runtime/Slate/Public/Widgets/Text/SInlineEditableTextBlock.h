// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnVerifyTextChanged, const FText&, FText&)
DECLARE_DELEGATE_OneParam(FOnBeginTextEdit, const FText&)

/**
 * Slate's InlineEditableTextBlock's are double selectable to go from a STextBlock to become SEditableTextBox.
 */
class SLATE_API SInlineEditableTextBlock: public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SInlineEditableTextBlock )
		: _Text()
		, _Style( &FCoreStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("InlineEditableTextBlockStyle") )
		, _Font()
		, _ColorAndOpacity()
		, _ShadowOffset()
		, _ShadowColorAndOpacity()
		, _HighlightText()
		, _WrapTextAt(0.0f)
		, _Justification(ETextJustify::Left)
		, _LineBreakPolicy()
		, _IsReadOnly(false)
	{}

		/** The text displayed in this text block */
		SLATE_ATTRIBUTE( FText, Text )

		/** Pointer to a style of the inline editable text block, which dictates the font, color, and shadow options. */
		SLATE_STYLE_ARGUMENT( FInlineEditableTextBlockStyle, Style )

		/** Sets the font used to draw the text (overrides style) */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )

		/** Text color and opacity (overrides style) */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/** Drop shadow offset in pixels (overrides style) */
		SLATE_ATTRIBUTE( FVector2D, ShadowOffset )

		/** Shadow color and opacity (overrides style) */
		SLATE_ATTRIBUTE( FLinearColor, ShadowColorAndOpacity )

		/** Highlight this text in the text block */
		SLATE_ATTRIBUTE( FText, HighlightText )

		/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
		SLATE_ATTRIBUTE( float, WrapTextAt )

		/** How the text should be aligned with the margin. */
		SLATE_ATTRIBUTE( ETextJustify::Type, Justification )

		/** The iterator to use to detect appropriate soft-wrapping points for lines (or null to use the default) */
		SLATE_ARGUMENT( TSharedPtr<IBreakIterator>, LineBreakPolicy )

		/** True if the editable text block is read-only. It will not be able to enter edit mode if read-only */
		SLATE_ATTRIBUTE( bool, IsReadOnly )

		/** Callback when the text starts to be edited */
		SLATE_EVENT( FOnBeginTextEdit, OnBeginTextEdit )

		/** Callback when the text is committed. */
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )

		/** Callback to check if the widget is selected, should only be hooked up if parent widget is handling selection or focus. */
		SLATE_EVENT( FIsSelected, IsSelected )

		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT( FOnVerifyTextChanged, OnVerifyTextChanged )
	SLATE_END_ARGS()

	~SInlineEditableTextBlock();

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	InViewModel			The UI logic not specific to slate
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * See SWidget::SupportsKeyboardFocus().
	 *
	 * @return  This widget can gain focus if IsSelected is not bound.
	 */
	virtual bool SupportsKeyboardFocus() const override;

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

	//virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	 
	/** Switches the widget to editing mode */
	void EnterEditingMode();

	/** Switches the widget to label mode. */
	void ExitEditingMode();

	/** Checks if the widget is in edit mode */
	bool IsInEditMode() const;

	void SetText( const TAttribute< FText >& InText );
	void SetText( const FString& InText );

	/** Sets the wrap text at attribute.  See WrapTextAt attribute */
	void SetWrapTextAt(const TAttribute<float>& InWrapTextAt);

protected:
	/** Callback for the text box's OnTextChanged event */
	void OnTextChanged(const FText& InText);

	/** Callback when the text box is committed, switches back to label mode. */ 
	void OnTextBoxCommitted(const FText& InText, ETextCommit::Type InCommitType);

	/** Cancels the edit mode and switches back to label mode */
	void CancelEditMode();

protected:
	/** The widget used when in label mode */
	TSharedPtr< STextBlock > TextBlock;

	/** The widget used when in editing mode */ 
	TSharedPtr< SEditableTextBox > TextBox;

	/** Delegate to execute when the text starts to be edited */
	FOnBeginTextEdit OnBeginTextEditDelegate;

	/** Delegate to execute when editing mode text is committed. */
	FOnTextCommitted OnTextCommittedDelegate;

	/** Delegate to execute to check the status of if the widget is selected or not
		Only needs to be hooked up if an external widget is managing selection, such
		as a SListView or STreeView. */
	FIsSelected IsSelected;

	/** When selection of widget is managed by another widget, this delays the "double select" from 
		occurring immediately, offering a chance for double clicking to take action. */
	float DoubleSelectDelay;

	/** Main horizontal box, used to dynamically add and remove the editable slot. */
	TSharedPtr< SHorizontalBox > HorizontalBox;

	/** Callback to verify text when changed. Will return an error message to denote problems. */
	FOnVerifyTextChanged OnVerifyTextChanged;

	/** Attribute for the text to use for the widget */
	TAttribute<FText> Text;

	/** Attribute to look up if the widget is read-only */
	TAttribute< bool > bIsReadOnly;
	
	/** Widget to focus when we finish editing */
	TWeakPtr<SWidget> WidgetToFocus;

private:
	/** Active timer to trigger entry into edit mode after a delay */
	EActiveTimerReturnType TriggerEditMode(double InCurrentTime, float InDeltaTime);

	/** The handle to the active timer */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;
};
