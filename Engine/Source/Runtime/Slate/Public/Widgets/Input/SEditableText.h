// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITextEditorWidget.h"
#include "ITextInputMethodSystem.h"
#include "IVirtualKeyboardEntry.h"
#include "SlateScrollHelper.h"

/**
 * Editable text widget
 */
class SLATE_API SEditableText : public SLeafWidget, public ITextEditorWidget, public IVirtualKeyboardEntry
{

public:

	SLATE_BEGIN_ARGS( SEditableText )
		: _Text()
		, _HintText()
		, _Style(&FCoreStyle::Get().GetWidgetStyle< FEditableTextStyle >("NormalEditableText"))
		, _Font()
		, _ColorAndOpacity()
		, _BackgroundImageSelected()
		, _BackgroundImageSelectionTarget()
		, _BackgroundImageComposing()
		, _CaretImage()
		, _IsReadOnly( false )
		, _IsPassword( false )
		, _IsCaretMovedWhenGainFocus( true )
		, _SelectAllTextWhenFocused( false )
		, _RevertTextOnEscape( false )
		, _ClearKeyboardFocusOnCommit(true)
		, _MinDesiredWidth(0.0f)
		, _SelectAllTextOnCommit( false )
		, _VirtualKeyboardType(EKeyboardType::Keyboard_Default)
		{}

		/** Sets the text content for this editable text widget */
		SLATE_ATTRIBUTE( FText, Text )

		/** The text that appears when there is nothing typed into the search box */
		SLATE_ATTRIBUTE( FText, HintText )

		/** The style of the text block, which dictates the font, color */
		SLATE_STYLE_ARGUMENT( FEditableTextStyle, Style )

		/** Sets the font used to draw the text (overrides Style) */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )

		/** Text color and opacity (overrides Style) */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/** Background image for the selected text (overrides Style) */
		SLATE_ATTRIBUTE( const FSlateBrush*, BackgroundImageSelected )

		/** Background image for the selection targeting effect (overrides Style) */
		SLATE_ATTRIBUTE( const FSlateBrush*, BackgroundImageSelectionTarget )

		/** Background image for the composing text (overrides Style) */
		SLATE_ATTRIBUTE( const FSlateBrush*, BackgroundImageComposing )

		/** Image brush used for the caret (overrides Style) */
		SLATE_ATTRIBUTE( const FSlateBrush*, CaretImage )

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

		/**
		 * This is NOT for validating input!
		 * 
		 * Called whenever a character is typed.
		 * Not called for copy, paste, or any other text changes!
		 */
		SLATE_EVENT( FOnIsTypedCharValid, OnIsTypedCharValid )

		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT( FOnTextChanged, OnTextChanged )

		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )

		/** Minimum width that a text block should be */
		SLATE_ATTRIBUTE( float, MinDesiredWidth )

		/** Whether to select all text when pressing enter to commit changes */
		SLATE_ATTRIBUTE( bool, SelectAllTextOnCommit )

		/** Menu extender for the right-click context menu */
		SLATE_EVENT( FMenuExtensionDelegate, ContextMenuExtender )

		/** The type of virtual keyboard to use on mobile devices */
		SLATE_ATTRIBUTE( EKeyboardType, VirtualKeyboardType)

	SLATE_END_ARGS()

	/** Constructor */
	SEditableText();
	virtual ~SEditableText();

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Sets the text currently being edited 
	 *
	 * @param  InNewText  The new text
	 */
	void SetText( const TAttribute< FText >& InNewText );
	
	/** See the HintText attribute */
	void SetHintText( const TAttribute< FText >& InHintText );
	
	/** See the IsReadOnly attribute */
	void SetIsReadOnly( TAttribute< bool > InIsReadOnly );
	
	/** See the IsPassword attribute */
	void SetIsPassword( TAttribute< bool > InIsPassword );
	
	/** See the ColorAndOpacity attribute */
	void SetColorAndOpacity(TAttribute<FSlateColor> Color);

	/**
	 * Restores the text to the original state
	 */
	void RestoreOriginalText();

	/**
	 *	Returns whether the current text varies from the original
	 */
	bool HasTextChangedFromOriginal() const;

	/**
	 * Sets the font used to draw the text
	 *
	 * @param  InNewFont	The new font to use
	 */
	void SetFont( const TAttribute< FSlateFontInfo >& InNewFont );

protected:

	friend class FTextInputMethodContext;
	class FTextInputMethodContext : public ITextInputMethodContext
	{
	public:
		FTextInputMethodContext(const TWeakPtr<SEditableText>& InOwningWidget)
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
		TWeakPtr<SEditableText> OwningWidget;

	public:
		FGeometry CachedGeometry;
		bool IsComposing;
		int32 CompositionBeginIndex;
		uint32 CompositionLength;
	};

	/**
	 * Range of selected characters
	 */
	class FTextSelection
	{

	public:

		/** Index of character we started selection on.  Note that this can be larger than LastIndex! */
		int32 StartIndex;

		/** Index of last character included in the selection range.  Note that this can be smaller than StartIndex! */
		int32 FinishIndex;


	public:

		/** Constructor */
		FTextSelection()
			: StartIndex( INDEX_NONE ),
			  FinishIndex( INDEX_NONE )
		{
		}

		/** Clears selection */
		void Clear()
		{
			StartIndex = FinishIndex = INDEX_NONE;
		}

		/**
		 * Returns the first character index in the selection range
		 *
		 * @return  First character index
		 */
		int32 GetMinIndex() const
		{
			return FMath::Min( StartIndex, FinishIndex );
		}

		/**
		 * Returns the last character index in the selection range
		 *
		 * @return  Last character index
		 */
		int32 GetMaxIndex() const
		{
			return FMath::Max( StartIndex, FinishIndex );
		}
	};



	/**
	 * Stores a single undo level for editable text
	 */
	class FUndoState
	{

	public:

		/** Text string */
		FText Text;

		/** Selection state */
		FTextSelection Selection;

		/** Caret position */
		int32 CaretPosition;

	public:

		FUndoState()
			: Text(),
			  Selection(),
			  CaretPosition( INDEX_NONE )
		{
		}
	};
	
public:
	// BEGIN ITextEditorWidget interface
	virtual void StartChangingText() override;
	virtual void FinishChangingText() override;
	virtual bool GetIsReadOnly() const override;
	virtual void BackspaceChar() override;
	virtual void DeleteChar() override;
	virtual bool CanTypeCharacter(const TCHAR CharInQuestion) const override;
	virtual void TypeChar( const int32 Character ) override;
	virtual FReply MoveCursor( FMoveCursor Args ) override;
	virtual void JumpTo(ETextLocation JumpLocation, ECursorAction Action) override;
	virtual void ClearSelection() override;
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
		return Text.Get();
	}

	virtual const FText GetHintText() const override
	{
		return HintText.Get();
	}

	virtual EKeyboardType GetVirtualKeyboardType() const override
	{
		return VirtualKeyboardType.Get();
	}

	virtual bool IsMultilineEntry() const override
	{
		return false;
	}
	// END IVirtualKeyboardEntry interface

protected:
	// BEGIN SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	virtual FReply OnKeyChar( const FGeometry& MyGeometry,  const FCharacterEvent& InCharacterEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	// END SWidget interface

protected:
	/**
	 * Updates text selection after the caret moved
	 *
	 * @param  InOldCaretPosition  Previous location of the caret
	 */
	void SelectText( const int32 InOldCaretPosition );

	/**
	 * Deletes selected text
	 */
	void DeleteSelectedText();
	
	/**
	 * Determines which character was clicked on
	 *
	 * @param  InMyGeometry				Widget geometry
	 * @param  InScreenCursorPosition	Click position (in screen space)
	 *
	 * @return  The clicked character index
	 */
	int32 FindClickedCharacterIndex( const FGeometry& InMyGeometry, const FVector2D& InScreenCursorPosition ) const;

	/**
	 * Determines which character was clicked on
	 *
	 * @param  InLocalCursorPosition       Click position (in local space)
	 * @param  GeometryScale               The DPI scale with which this widget is being drawn.
	 *
	 * @return  The clicked character index
	 */
	int32 FindClickedCharacterIndex( const FVector2D& InLocalCursorPosition, float GeometryScale ) const;

	/** Find the closest word boundary */
	int32 ScanForWordBoundary( const int32 Location, int8 Direction ) const; 

	/** Are we currently at the beginning of a word */
	bool IsAtWordStart( const int32 Location ) const;

	/**
	 * Adds the specified undo state to the undo stack
	 *
	 * @param  InUndoState  The undo state to push.  Usually, this is created using MakeUndoState()
	 */
	void PushUndoState( const SEditableText::FUndoState& InUndoState );


	/**
	 * Clears the local undo buffer
	 */
	void ClearUndoStates();


	/**
	 * Creates an undo state structure for the control's current state and returns it
	 *
	 * @param  OutUndoState  Undo state for current state of control
	 */
	void MakeUndoState( SEditableText::FUndoState& OutUndoState );

	/**
	 * Saves edited text into our text attribute, if appropriate
	 */
	void SaveText();

	/**
	 * Executes action to delete selected text
	 */
	void ExecuteDeleteAction();

	/**
	 * Returns true if 'Select All' is currently a valid action
	 *
	 * @return	True to allow this action, otherwise false
	 */
	bool CanExecuteSelectAll() const;

	/**
	 * Returns true if 'Delete' is currently a valid action
	 *
	 * @return	True to allow this action, otherwise false
	 */
	bool CanExecuteDelete() const;

	/**
	 * Sets the caret position
	 * 
	 * @param Position The character index for the new position
	 */
	void SetCaretPosition( int32 Position );

	/**
	 * Called to restart selection target animation after most types of text selection changes
	 */
	void RestartSelectionTargetAnimation();

	/**
	 * Called when the content menu window is closed
	 */
	void OnWindowClosed(const TSharedRef<SWindow>&);

private:
	/** Animates the caret and highlight selection springs */
	EActiveTimerReturnType AnimateSpringsWhileFocused(double InCurrentTime, float InDeltaTime);
	
	/** @return Whether the editable text should appear focused */
	bool ShouldAppearFocused() const;

	/** 
	 * @return whether there is anything in the clipboard
	 */
	bool DoesClipboardHaveAnyText() const;

	/**
	 * @return  the string that needs to be rendered
	 */
	FString GetStringToRender() const;

private:

	/** A pointer to the editable text currently under the mouse cursor. nullptr when there isn't one. */
	static SEditableText* EditableTextUnderCursor;

	/** The text content for this editable text widget */
	TAttribute< FText > Text;

	/** The text content for watermark/hinting what text to type here */
	TAttribute< FText > HintText;

	/** The font used to draw the text */
	TAttribute< FSlateFontInfo > Font;

	/** Text color and opacity */
	TAttribute<FSlateColor> ColorAndOpacity;

	/** Background image for the selected text */
	TAttribute< const FSlateBrush* > BackgroundImageSelected;

	/** Background image for the selection targeting effect */
	TAttribute< const FSlateBrush* > BackgroundImageSelectionTarget;

	/** Background image for the composing text */
	TAttribute< const FSlateBrush* > BackgroundImageComposing;

	/** Image brush used for the caret */
	TAttribute< const FSlateBrush* > CaretImage;

	/** Sets whether this text box can actually be modified interactively by the user */
	TAttribute< bool > IsReadOnly;

	/** Sets whether this text box is for storing a password */
	TAttribute< bool > IsPassword;

	/** Workaround as we loose focus when the auto completion closes. */
	TAttribute< bool > IsCaretMovedWhenGainFocus;
	
	/** Whether to select all text when the user clicks to give focus on the widget */
	TAttribute< bool > bSelectAllTextWhenFocused;

	/** Whether to allow the user to back out of changes when they press the escape key */
	TAttribute< bool > RevertTextOnEscape;

	/** Whether to clear keyboard focus when pressing enter to commit changes */
	TAttribute< bool > ClearKeyboardFocusOnCommit;

	/** Whether to select all text when pressing enter to commit changes */
	TAttribute< bool > SelectAllTextOnCommit;

	/** Called when a character is typed and we want to know if the text field supports typing this character. */
	FOnIsTypedCharValid OnIsTypedCharValid;

	/** Called whenever the text is changed interactively by the user */
	FOnTextChanged OnTextChanged;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	FOnTextCommitted OnTextCommitted;

	/** Text string currently being edited */
	FText EditedText;

	/** Original text prior to user edits.  This is used to restore the text if the user uses the escape key. */
	FText OriginalText;

	/** Current scrolled position, in text-area space; use ConvertGeometryFromRenderSpaceToTextSpace to convert before using values with ScrollHelper */
	FScrollHelper ScrollHelper;

	/** Caret position as an index into the editable text string's character array */
	int32 CaretPosition;

	/** Horizontal visual position of caret along the scrolling canvas */
	FFloatSpring1D CaretVisualPositionSpring;

	/** Stores the last time that the caret was moved by the user.  Used to prevent the caret from blinking. */
	double LastCaretInteractionTime;

	/** Current selection state */
	FTextSelection Selection;

	/** Selection "targeting" visual effect left position */
	FFloatSpring1D SelectionTargetLeftSpring;

	/** Selection "targeting" visual effect right position */
	FFloatSpring1D SelectionTargetRightSpring;

	/** Last time that the user had a major interaction with the selection */
	double LastSelectionInteractionTime;

	/** True if we're currently selecting text by dragging the mouse cursor with the left button held down */
	bool bIsDragSelecting;

	/** True if the last mouse down caused us to receive keyboard focus */
	bool bWasFocusedByLastMouseDown;

	/** True if characters were selected by dragging since the last keyboard focus.  Used for text selection. */
	bool bHasDragSelectedSinceFocused;

	/** True if characters were selected by dragging since the last mouse down. */
	bool bHasDragSelectedSinceMouseDown;

	/** Undo states */
	TArray< FUndoState > UndoStates;

	/** Current undo state level that we've rolled back to, or INDEX_NONE if we haven't undone.  Used for 'Redo'. */
	int32 CurrentUndoLevel;

	/** True if we're currently (potentially) changing the text string */
	bool bIsChangingText;

	/** Undo state that will be pushed if text is actually changed between calls to StartChangingText() and FinishChangingText() */
	FUndoState StateBeforeChangingText;

	/** Information about any active context menu widgets */
	FActiveTextEditContextMenu ActiveContextMenu;

	/** Prevents the editabletext from being smaller than desired in certain cases (e.g. when it is empty) */
	TAttribute<float> MinDesiredWidth;

	/** A list commands to execute if a user presses the corresponding keybinding in the text box */
	TSharedRef< FUICommandList > UICommandList;

	/** Menu extender for right-click context menu */
	TSharedPtr<FExtender> MenuExtender;

	/** Implemented context object for text input method systems. */
	TSharedPtr<FTextInputMethodContext> TextInputMethodContext;

	/** Notification interface object for text input method systems. */
	TSharedPtr<ITextInputMethodChangeNotifier> TextInputMethodChangeNotifier;

	/** The type of virtual keyboard to use for editing this text on mobile */
	TAttribute<EKeyboardType> VirtualKeyboardType;

	/** True if the text has been changed by a virtual keyboard */
	bool bTextChangedByVirtualKeyboard;

	/** True if a spring animation is currently in progress */
	bool bIsSpringing;

};
