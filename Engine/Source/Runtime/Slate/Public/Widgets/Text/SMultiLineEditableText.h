// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_FANCY_TEXT

#include "ITextEditorWidget.h"
#include "IVirtualKeyboardEntry.h"
#include "ITextInputMethodSystem.h"
#include "ITextLayoutMarshaller.h"
#include "SScrollBar.h"

class FTextBlockLayout;

/** An editable text widget that supports multiple lines and soft word-wrapping. */
class SLATE_API SMultiLineEditableText : public SWidget, public ITextEditorWidget, public IVirtualKeyboardEntry
{
public:
	/** Called when the cursor is moved within the text area */
	DECLARE_DELEGATE_OneParam( FOnCursorMoved, const FTextLocation& );

	SLATE_BEGIN_ARGS( SMultiLineEditableText )
		: _Text()
		, _HintText()
		, _Marshaller()
		, _WrapTextAt( 0.0f )
		, _AutoWrapText(false)
		, _TextStyle( &FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>( "NormalText" ) )
		, _Font()
		, _Margin( FMargin() )
		, _LineHeightPercentage( 1.0f )
		, _Justification( ETextJustify::Left )
		, _IsReadOnly(false)
		, _OnTextChanged()
		, _OnTextCommitted()
		, _SelectAllTextWhenFocused(false)
		, _RevertTextOnEscape(false)
		, _ClearKeyboardFocusOnCommit(true)
		, _OnCursorMoved()
		, _ContextMenuExtender()
		, _ModiferKeyForNewLine(EModifierKey::None)
	{}
		/** The initial text that will appear in the widget. */
		SLATE_ATTRIBUTE(FText, Text)

		/** Hint text that appears when there is no text in the text box */
		SLATE_ATTRIBUTE(FText, HintText)

		/** The marshaller used to get/set the raw text to/from the text layout. */
		SLATE_ARGUMENT(TSharedPtr< ITextLayoutMarshaller >, Marshaller)

		/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
		SLATE_ATTRIBUTE(float, WrapTextAt)

		/** Whether to wrap text automatically based on the widget's computed horizontal space.  IMPORTANT: Using automatic wrapping can result
		    in visual artifacts, as the the wrapped size will computed be at least one frame late!  Consider using WrapTextAt instead.  The initial 
			desired size will not be clamped.  This works best in cases where the text block's size is not affecting other widget's layout. */
		SLATE_ATTRIBUTE(bool, AutoWrapText)

		/** Pointer to a style of the text block, which dictates the font, color, and shadow options. */
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)

		/** Font color and opacity (overrides Style) */
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)

		/** The amount of blank space left around the edges of text area. */
		SLATE_ATTRIBUTE(FMargin, Margin)

		/** The amount to scale each lines height by. */
		SLATE_ATTRIBUTE(float, LineHeightPercentage)

		/** How the text should be aligned with the margin. */
		SLATE_ATTRIBUTE(ETextJustify::Type, Justification)

		/** Sets whether this text box can actually be modified interactively by the user */
		SLATE_ATTRIBUTE(bool, IsReadOnly)

		/** The horizontal scroll bar widget */
		SLATE_ARGUMENT(TSharedPtr< SScrollBar >, HScrollBar)

		/** The vertical scroll bar widget */
		SLATE_ARGUMENT(TSharedPtr< SScrollBar >, VScrollBar)

		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT(FOnTextChanged, OnTextChanged)

		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT(FOnTextCommitted, OnTextCommitted)

		/** Whether to select all text when the user clicks to give focus on the widget */
		SLATE_ATTRIBUTE(bool, SelectAllTextWhenFocused)

		/** Whether to allow the user to back out of changes when they press the escape key */
		SLATE_ATTRIBUTE(bool, RevertTextOnEscape)

		/** Whether to clear keyboard focus when pressing enter to commit changes */
		SLATE_ATTRIBUTE(bool, ClearKeyboardFocusOnCommit)

		/** Called whenever the horizontal scrollbar is moved by the user */
		SLATE_EVENT(FOnUserScrolled, OnHScrollBarUserScrolled)

		/** Called whenever the vertical scrollbar is moved by the user */
		SLATE_EVENT(FOnUserScrolled, OnVScrollBarUserScrolled)

		/** Called when the cursor is moved within the text area */
		SLATE_EVENT(FOnCursorMoved, OnCursorMoved)

		/** Menu extender for the right-click context menu */
		SLATE_EVENT(FMenuExtensionDelegate, ContextMenuExtender)

		/** The optional modifier key necessary to create a newline when typing into the editor. */
		SLATE_ARGUMENT(EModifierKey::Type, ModiferKeyForNewLine)

	SLATE_END_ARGS()

	SMultiLineEditableText();
	virtual ~SMultiLineEditableText();

	void Construct( const FArguments& InArgs );

	/**
	 * Sets the text for this text block
	 */
	void SetText(const TAttribute< FText >& InText);

	/**
	 * Sets the text that appears when there is no text in the text box
	 */
	void SetHintText(const TAttribute< FText >& InHintText);

	virtual bool GetIsReadOnly() const override;
	virtual void ClearSelection() override;

	/** Get the currently selected text */
	FText GetSelectedText() const;

	/** Insert the given text at the current cursor position, correctly taking into account new line characters */
	void InsertTextAtCursor(const FText& InText);
	void InsertTextAtCursor(const FString& InString);

	/** Insert the given run at the current cursor position */
	void InsertRunAtCursor(TSharedRef<IRun> InRun);

	/** Move the cursor to the given location in the document (will also scroll to this point) */
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

	/** Restores the text to the original state */
	void RestoreOriginalText();

	/** Returns whether the current text varies from the original */
	bool HasTextChangedFromOriginal() const;

private:
	
	/** 
	 * Note: The IME interface for the multiline editable text uses the pre-flowed version of the string since the IME APIs are designed to work with flat strings
	 *		 This means we have to do a bit of juggling to convert between the two
	 */
	friend class FTextInputMethodContext;
	class FTextInputMethodContext : public ITextInputMethodContext
	{
	public:
		FTextInputMethodContext(const TWeakPtr<SMultiLineEditableText>& InOwningWidget)
			:	OwningWidget(InOwningWidget)
			,	IsComposing(false)
			,	CompositionBeginIndex(INDEX_NONE)
			,	CompositionLength(0)
		{}

		virtual ~FTextInputMethodContext() {}

	private:
		virtual bool IsReadOnly() override;
		virtual uint32 GetTextLength() override;
		virtual void GetSelectionRange(uint32& BeginIndex, uint32& Length, ECaretPosition& CaretPosition) override;
		virtual void SetSelectionRange(const uint32 BeginIndex, const uint32 Length, const ECaretPosition CaretPosition) override;
		virtual void GetTextInRange(const uint32 BeginIndex, const uint32 Length, FString& OutString) override;
		virtual void SetTextInRange(const uint32 BeginIndex, const uint32 Length, const FString& InString) override;
		virtual int32 GetCharacterIndexFromPoint(const FVector2D& Point) override;
		virtual bool GetTextBounds(const uint32 BeginIndex, const uint32 Length, FVector2D& Position, FVector2D& Size) override;
		virtual void GetScreenBounds(FVector2D& Position, FVector2D& Size) override;
		virtual TSharedPtr<FGenericWindow> GetWindow() override;
		virtual void BeginComposition() override;
		virtual void UpdateCompositionRange(const int32 InBeginIndex, const uint32 InLength) override;
		virtual void EndComposition() override;

	private:
		TWeakPtr<SMultiLineEditableText> OwningWidget;

	public:
		FGeometry CachedGeometry;
		bool IsComposing;
		int32 CompositionBeginIndex;
		uint32 CompositionLength;
	};

	enum class ECursorAlignment : uint8
	{
		/** Visually align the cursor to the left of the character its placed at, and insert text before the character */
		Left,

		/** Visually align the cursor to the right of the character its placed at, and insert text after the character */
		Right,
	};

	/** Store the information about the current cursor position */
	class FCursorInfo
	{
	public:
		FCursorInfo()
			: CursorPosition()
			, CursorAlignment(ECursorAlignment::Left)
			, LastCursorInteractionTime(0)
		{
		}

		/** Get the literal position of the cursor (note: this may not be the correct place to insert text to, use GetCursorInteractionLocation for that) */
		FORCEINLINE FTextLocation GetCursorLocation() const
		{
			return CursorPosition;
		}

		/** Get the alignment of the cursor */
		FORCEINLINE ECursorAlignment GetCursorAlignment() const
		{
			return CursorAlignment;
		}

		/** Get the interaction position of the cursor (where to insert, delete, etc, text from/to) */
		FORCEINLINE FTextLocation GetCursorInteractionLocation() const
		{
			// If the cursor is right aligned, we treat it as if it were one character further along for insert/delete purposes
			return FTextLocation(CursorPosition, (CursorAlignment == ECursorAlignment::Right) ? 1 : 0);
		}

		FORCEINLINE double GetLastCursorInteractionTime() const
		{
			return LastCursorInteractionTime;
		}

		/** Set the position of the cursor, and then work out the correct alignment based on the current text layout */
		void SetCursorLocationAndCalculateAlignment(const TSharedPtr<FTextLayout>& TextLayout, const FTextLocation& InCursorPosition);

		/** Set the literal position and alignment of the cursor */
		void SetCursorLocationAndAlignment(const FTextLocation& InCursorPosition, const ECursorAlignment InCursorAlignment);

		/** Create an undo for this cursor data */
		FCursorInfo CreateUndo() const;

		/** Restore this cursor data from an undo */
		void RestoreFromUndo(const FCursorInfo& UndoData);

	private:
		/** Current cursor position; there is always one. */
		FTextLocation CursorPosition;

		/** Cursor alignment (horizontal) within its position. This affects the rendering and behavior of the cursor when adding new characters */
		ECursorAlignment CursorAlignment;

		/** Last time the user did anything with the cursor.*/
		double LastCursorInteractionTime;
	};

	/**
	* Stores a single undo level for editable text
	*/
	class FUndoState
	{

	public:

		/** Text */
		FText Text;

		/** Selection state */
		TOptional<FTextLocation> SelectionStart;

		/** Cursor data */
		FCursorInfo CursorInfo;

	public:

		FUndoState()
			: Text()
			, SelectionStart()
			, CursorInfo()
		{
		}
	};

	/** Information needed to be able to scroll to a given point */
	struct FScrollInfo
	{
		FScrollInfo()
			: Position()
			, Alignment(ECursorAlignment::Left)
		{
		}

		FScrollInfo(const FTextLocation InPosition, const ECursorAlignment InAlignment)
			: Position(InPosition)
			, Alignment(InAlignment)
		{
		}

		/** The location in the document to scroll to (in line model space) */
		FTextLocation Position;

		/** The alignment at the given position. This may affect which line view the character maps to when converted from line model space */
		ECursorAlignment Alignment;
	};

	/** Run highlighter used to draw the cursor */
	class FCursorLineHighlighter : public ISlateLineHighlighter
	{
	public:

		static TSharedRef< FCursorLineHighlighter > Create(const FCursorInfo* InCursorInfo);

		virtual ~FCursorLineHighlighter() {}

		virtual int32 OnPaint( const FPaintArgs& Args, const FTextLayout::FLineView& Line, const float OffsetX, const float Width, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

	protected:

		FCursorLineHighlighter(const FCursorInfo* InCursorInfo);

		/** Cursor data that this highlighter is tracking */
		const FCursorInfo* CursorInfo;
	};

	/** Run highlighter used to draw the composition range */
	class FTextCompositionHighlighter : public ISlateLineHighlighter
	{
	public:

		static TSharedRef< FTextCompositionHighlighter > Create();

		virtual ~FTextCompositionHighlighter() {}

		virtual int32 OnPaint( const FPaintArgs& Args, const FTextLayout::FLineView& Line, const float OffsetX, const float Width, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

	protected:

		FTextCompositionHighlighter();
	};

	/** Run renderer used to draw selection ranges */
	class FTextSelectionRunRenderer : public ISlateRunRenderer
	{
	public:

		static TSharedRef< FTextSelectionRunRenderer > Create();

		virtual ~FTextSelectionRunRenderer() {}

		virtual int32 OnPaint( const FPaintArgs& Args, const FTextLayout::FLineView& Line, const TSharedRef< ISlateRun >& Run, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

		void SetHasKeyboardFocus(const bool bInHasKeyboardFocus)
		{
			bHasKeyboardFocus = bInHasKeyboardFocus;
		}

	protected:

		FTextSelectionRunRenderer();

		/** true if the parent widget has keyboard focus, false otherwise */
		bool bHasKeyboardFocus;
	};

private:

	// BEGIN ITextEditorWidget interface
	virtual void StartChangingText() override;
	virtual void FinishChangingText() override;
	virtual void BackspaceChar() override;
	virtual void DeleteChar() override;
	virtual bool CanTypeCharacter(const TCHAR CharInQuestion) const override;
	virtual void TypeChar( const int32 Character ) override;
	virtual FReply MoveCursor( FMoveCursor Args ) override;
	virtual void JumpTo(ETextLocation JumpLocation, ECursorAction Action) override;
	virtual void SelectAllText() override;
	virtual bool SelectAllTextWhenFocused() override;
	virtual void SelectWordAt( const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition ) override;
	virtual void BeginDragSelection() override;
	virtual bool IsDragSelecting() const override;
	virtual void EndDragSelection() override;
	virtual bool AnyTextSelected() const override;
	virtual bool IsTextSelectedAt( const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition ) const override;
	virtual void SetWasFocusedByLastMouseDown( bool Value ) override;
	virtual bool WasFocusedByLastMouseDown() const override;
	virtual void SetHasDragSelectedSinceFocused( bool Value ) override;
	virtual bool HasDragSelectedSinceFocused() const override;
	virtual FReply OnEscape() override;
	virtual void OnEnter() override;
	virtual bool CanExecuteCut() const override;
	virtual void CutSelectedTextToClipboard() override;
	virtual bool CanExecuteCopy() const override;
	virtual void CopySelectedTextToClipboard() override;
	virtual bool CanExecutePaste() const override;
	virtual void PasteTextFromClipboard() override;
	virtual bool CanExecuteUndo() const override;
	virtual void Undo() override;
	virtual void Redo() override;
	virtual TSharedRef< SWidget > GetWidget() override;
	virtual void SummonContextMenu(const FVector2D& InLocation, TSharedPtr<SWindow> ParentWindow = TSharedPtr<SWindow>()) override;
	virtual void LoadText() override;
	// END ITextEditorWidget interface

public:
	// BEGIN IVirtualKeyboardEntry interface
	virtual void SetTextFromVirtualKeyboard(const FText& InNewText) override;

	virtual const FText& GetText() const override
	{
		return BoundText.Get(FText::GetEmpty());
	}

	virtual const FText GetHintText() const override
	{
		return HintText.Get(FText::GetEmpty());
	}

	virtual EKeyboardType GetVirtualKeyboardType() const override
	{
		return EKeyboardType::Keyboard_Default;
	}

	virtual bool IsMultilineEntry() const override
	{
		return true;
	}
	// END IVirtualKeyboardEntry interface

protected:
	// BEGIN SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual void CacheDesiredSize(float) override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FChildren* GetChildren() override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyChar( const FGeometry& MyGeometry,const FCharacterEvent& InCharacterEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	// END SWidget interface

private:
	
	void OnWindowClosed(const TSharedRef<SWindow>&);

	/** Remember where the cursor was when we started selecting. */
	void BeginSelecting( const FTextLocation& Endpoint );

	void DeleteSelectedText();

	void ExecuteDeleteAction();
	bool CanExecuteDelete() const;

	bool CanExecuteSelectAll() const;

	bool DoesClipboardHaveAnyText() const;

	/** Insert the given text at the current cursor position, correctly taking into account new line characters */
	void InsertTextAtCursorImpl(const FString& InString);

	/** Insert a new-line at the current cursor position */
	void InsertNewLineAtCursorImpl();

	/**
	 * Given a location and a Direction to offset, return a new location.
	 *
	 * @param Location    Cursor location from which to offset
	 * @param Direction   Positive means right, negative means left.
	 */
	FTextLocation TranslatedLocation( const FTextLocation& CurrentLocation, int8 Direction ) const;

	/**
	 * Given a location and a Direction to offset, return a new location.
	 *
	 * @param Location              Cursor location from which to offset
	 * @param NumLinesToMove        Number of lines to move in a given direction. Positive means down, negative means up.
	 * @param GeometryScale         Geometry DPI scale at which the widget is being rendered
	 * @param OutCursorPosition     Fill with the updated cursor position.
	 * @param OutCursorAlignment    Optionally fill with a new cursor alignment (will be auto-calculated if not set).
	 */
	void TranslateLocationVertical( const FTextLocation& Location, int32 NumLinesToMove, float GeometryScale, FTextLocation& OutCursorPosition, TOptional<ECursorAlignment>& OutCursorAlignment ) const;

	/** Find the closest word boundary */
	FTextLocation ScanForWordBoundary( const FTextLocation& Location, int8 Direction ) const; 

	/** Get the character at Location */
	TCHAR GetCharacterAt( const FTextLocation& Location ) const;

	/** Are we at the beginning of all the text. */
	bool IsAtBeginningOfDocument( const FTextLocation& Location ) const;
	
	/** Are we at the end of all the text. */
	bool IsAtEndOfDocument( const FTextLocation& Location ) const;

	/** Is this location the beginning of a line */
	bool IsAtBeginningOfLine( const FTextLocation& Location ) const;

	/** Is this location the end of a line. */
	bool IsAtEndOfLine( const FTextLocation& Location ) const;

	/** Are we currently at the beginning of a word */
	bool IsAtWordStart( const FTextLocation& Location ) const;

	void UpdateCursorHighlight();

	void RemoveCursorHighlight();

	void UpdatePreferredCursorScreenOffsetInLine();

	void PushUndoState(const SMultiLineEditableText::FUndoState& InUndoState);

	void ClearUndoStates();

	void MakeUndoState(SMultiLineEditableText::FUndoState& OutUndoState);

	void SaveText(const FText& TextToSave);

	/**
	 * Sets the current editable text for this text block
	 * Note: Doesn't update the value of BoundText, nor does it call OnTextChanged
	 *
	 * @param TextToSet		The new text to set in the internal TextLayout
	 * @param bForce		True to force the update, even if the text currently matches what's in the TextLayout
	 *
	 * @return true if the text was updated, false if the text wasn't update (because it was already up-to-date)
	 */
	bool SetEditableText(const FText& TextToSet, const bool bForce = false);

	/**
	 * Gets the current editable text for this text block
	 * Note: We don't store text in this form (it's stored as lines in the text layout) so every call to this function has to reconstruct it
	 */
	FText GetEditableText() const;

	/**
	 * Force the text layout to be updated from the marshaller
	 */
	void ForceRefreshTextLayout(const FText& CurrentText);

	void OnHScrollBarMoved(const float InScrollOffsetFraction);
	void OnVScrollBarMoved(const float InScrollOffsetFraction);

	/** Return whether a RMB+Drag scroll operation is taking place */
	bool IsRightClickScrolling() const;

	/**
	 * Ensure that we will get a Tick() soon (either due to having active focus, or something having changed progmatically and requiring an update)
	 * Does nothing if the active tick timer is already enabled
	 */
	void EnsureActiveTick();

private:

	/** The text displayed in this text block */
	TAttribute<FText> BoundText;

	/** The state of BoundText last Tick() (used to allow updates when the text is changed) */
	FTextSnapshot BoundTextLastTick;

	/** The text that appears when there is no text in the text box */
	TAttribute<FText> HintText;

	/** The marshaller used to get/set the BoundText text to/from the text layout. */
	TSharedPtr< ITextLayoutMarshaller > Marshaller;

	/** In control of the layout and wrapping of the BoundText */
	TSharedPtr< FSlateTextLayout > TextLayout;

	/** In control of the layout and wrapping of the HintText */
	TSharedPtr< FTextBlockLayout > HintTextLayout;

	/** Default style used by the TextLayout */
	FTextBlockStyle TextStyle;
	
	/** Style used to draw the hint text (only valid when HintTextLayout is set) */
	TSharedPtr< FTextBlockStyle > HintTextStyle;

	/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
	TAttribute< float > WrapTextAt;

	/** True if we're wrapping text automatically based on the computed horizontal space for this widget */
	TAttribute< bool > AutoWrapText;

	/** The last known size of the control from the previous OnPaint, used to recalculate wrapping. */
	mutable FVector2D CachedSize;

	/** The scroll offset (in unscaled Slate units) for this text */
	FVector2D ScrollOffset;

	TAttribute< FMargin > Margin;
	TAttribute< ETextJustify::Type > Justification; 
	TAttribute< float > LineHeightPercentage;

	TSharedPtr< FCursorLineHighlighter > CursorLineHighlighter;
	TSharedPtr< FTextCompositionHighlighter > TextCompositionHighlighter;
	TSharedPtr< FTextSelectionRunRenderer > TextSelectionRunRenderer;

	/** That start of the selection when there is a selection. The end is implicitly wherever the cursor happens to be. */
	TOptional<FTextLocation> SelectionStart;

	/** Current cursor data */
	FCursorInfo CursorInfo;

	/** The user probably wants the cursor where they last explicitly positioned it horizontally. */
	float PreferredCursorScreenOffsetInLine;

	/** Whether to select all text when the user clicks to give focus on the widget */
	TAttribute< bool > bSelectAllTextWhenFocused;

	/** True if any changes should be reverted if we recieve an escape key */
	TAttribute< bool > bRevertTextOnEscape;

	/** True if we want the text control to lose focus on an text commit/revert events */
	TAttribute< bool > bClearKeyboardFocusOnCommit;

	/** True if we're currently selecting text by dragging the mouse cursor with the left button held down */
	bool bIsDragSelecting;

	/** True if the last mouse down caused us to receive keyboard focus */
	bool bWasFocusedByLastMouseDown;

	/** True if characters were selected by dragging since the last keyboard focus.  Used for text selection. */
	bool bHasDragSelectedSinceFocused;

	/** If set, the pending data containing a position that should be scrolled into view */
	TOptional< FScrollInfo > PositionToScrollIntoView;

	/** Undo states */
	TArray< FUndoState > UndoStates;

	/** Current undo state level that we've rolled back to, or INDEX_NONE if we haven't undone.  Used for 'Redo'. */
	int32 CurrentUndoLevel;

	/** Undo state that will be pushed if text is actually changed between calls to StartChangingText() and FinishChangingText() */
	FUndoState StateBeforeChangingText;

	/** Original text undo state */
	FUndoState OriginalText;

	/** True if we're currently (potentially) changing the text string */
	bool bIsChangingText;

	/** Sets whether this text box can actually be modified interactively by the user */
	TAttribute< bool > IsReadOnly;

	/** A list commands to execute if a user presses the corresponding keybinding in the text box */
	TSharedRef< FUICommandList > UICommandList;

	/** Called whenever the text is changed interactively by the user */
	FOnTextChanged OnTextChanged;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	FOnTextCommitted OnTextCommitted;

	/** Called when the cursor is moved within the text area */
	FOnCursorMoved OnCursorMoved;

	/** The horizontal scroll bar widget */
	TSharedPtr< SScrollBar > HScrollBar;

	/** The vertical scroll bar widget */
	TSharedPtr< SScrollBar > VScrollBar;

	/** Called whenever the horizontal scrollbar is moved by the user */
	FOnUserScrolled OnHScrollBarUserScrolled;

	/** Called whenever the vertical scrollbar is moved by the user */
	FOnUserScrolled OnVScrollBarUserScrolled;

	/** Menu extender for right-click context menu */
	TSharedPtr<FExtender> MenuExtender;

	/** Information about any active context menu widgets */
	FActiveTextEditContextMenu ActiveContextMenu;

	/** Implemented context object for text input method systems. */
	TSharedPtr<FTextInputMethodContext> TextInputMethodContext;

	/** Notification interface object for text input method systems. */
	TSharedPtr<ITextInputMethodChangeNotifier> TextInputMethodChangeNotifier;

	/** Whether the text has been changed by a virtual keyboard */
	bool bTextChangedByVirtualKeyboard;

	/** The optional modifier key necessary to create a newline when typing into the editor. */
	EModifierKey::Type ModiferKeyForNewLine;

	/** The timer that is actively driving this widget to Tick() even when Slate is idle */
	TWeakPtr<FActiveTimerHandle> ActiveTickTimer;

	/** How much we scrolled while RMB was being held */
	float AmountScrolledWhileRightMouseDown;

	/** Whether a software cursor is currently active */
	bool bIsSoftwareCursor;

	/**	The current position of the software cursor */
	FVector2D SoftwareCursorPosition;
};


#endif //WITH_FANCY_TEXT