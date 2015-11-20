// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


enum class ECursorMoveMethod
{
	/** Move in one of the cardinal directions e.g. arrow left, right, up, down */
	Cardinal,
	/** Move the cursor to the correct character based on the given screen position. */
	ScreenPosition
};

enum class ECursorMoveGranularity
{
	/** Move one character at a time (e.g. arrow left, right, up, down). */
	Character,
	/** Move one word at a time (e.g. ctrl is held down and arrow left/right/up/down). */
	Word
};

enum class ECursorAction
{
	/** Just relocate the cursor. */
	MoveCursor,
	/** Move the cursor and select any text that it passes over. */
	SelectText
};


enum class ETextLocation
{
	/** Jump to the beginning of text. (e.g. Ctrl+Home) */
	BeginningOfDocument,
	/** Jump to the end of text. (e.g. Ctrl+End) */
	EndOfDocument,
	/** Jump to the beginning of the line or beginning of text within a line (e.g. Home) */
	BeginningOfLine,
	/** Jump to the end of line (e.g. End) */
	EndOfLine,
	/** Jump to the previous page in this document (e.g. PageUp) */
	PreviousPage,
	/** Jump to the next page in this document (e.g. PageDown) */
	NextPage,
};

/**
 * Argument to the ITextEditorWidget::Move(); it decouples performing
 * cursor movement and text highlighting actions from event handling.
 * FMoveCursor describes how the cursor can be moved via keyboard,
 * mouse, and other mechanisms in the future.
 */
class FMoveCursor
{
public:
	/**
	 * Creates a MoveCursor action that describes moving by a single character in any of the cardinal directions.
	 *
	 * @param Granularity    Move one character at a time, or on word boundaries (e.g. Ctrl is held down, or double-click mouse+drag)
	 * @param Direction      Axis-aligned unit vector along which to move. e.g. Move right: (0,1) or Move Up: (-1, 0)
	 * @param Action         Just move or also select text?
	 */
	static FMoveCursor Cardinal( ECursorMoveGranularity Granularity, FIntPoint Direction, ECursorAction Action );

	/**
	 * Creates a MoveCursor action that describes moving the text cursor by selecting an arbitrary coordinate on the screen.
	 * e.g. User user touches a touch device screen or uses the mouse to point at text.
	 *
	 * @param LocalPosition          Position in the text widget where the user wants to move the cursor by pointing
	 * @param GeometryScale          DPI Scale of the widget geometry.
	 * @param Action                 Just move or also select text?
	 */
	static FMoveCursor ViaScreenPointer( FVector2D LocalPosition, float GeometryScale, ECursorAction Action );

	/** @see ECursorMoveMethod */
	ECursorMoveMethod GetMoveMethod() const;

	/** Is the cursor moving up/down; Only valid for word and character movement methods. */
	bool IsVerticalMovement() const;

	/** Is the cursor moving left/right; Only valid for word and character movement methods. */
	bool IsHorizontalMovement() const;

	/** @see ECursorAction */
	ECursorAction GetAction() const;

	/** When using directional movement (i.e. Character or Word granularity; not screen position, which way to move.) */
	FIntPoint GetMoveDirection() const;

	/** Which position in the widget where the user touched when using ScreenPosition mode. */
	FVector2D GetLocalPosition() const;

	/** Move one character at a time, or one word? */
	ECursorMoveGranularity GetGranularity() const;

	/** Geometry Scale at the time of the event that caused this action */
	float GetGeometryScale() const;

private:
	FMoveCursor( ECursorMoveGranularity InGranularity, ECursorMoveMethod InMethod, FVector2D InDirectionOrPosition, float InGeometryScale, ECursorAction InAction );

	ECursorMoveGranularity Granularity;
	ECursorMoveMethod Method;
	FVector2D DirectionOrPosition;
	ECursorAction Action;
	float GeometryScale;
};

/** Manages the state for an active context menu */
class FActiveTextEditContextMenu
{
public:
	FActiveTextEditContextMenu()
		: bIsPendingSummon(false)
		, ActiveMenu()
	{
	}

	/** Check to see whether this context is valid (either pending or active) */
	bool IsValid() const
	{
		return bIsPendingSummon || ActiveMenu.IsValid();
	}

	/** Called to reset the active context menu state */
	void Reset()
	{
		bIsPendingSummon = false;
		ActiveMenu.Reset();
	}

	/** Called before you summon your context menu */
	void PrepareToSummon()
	{
		bIsPendingSummon = true;
		ActiveMenu.Reset();
	}

	DEPRECATED(4.9, "SummonSucceeded() taking an SWindow parameter is deprecated. Use the new version of SummonSucceeded() takes an IMenu.")
	void SummonSucceeded(const TSharedRef<SWindow>& InWindow)
	{
		// deprecated
		bIsPendingSummon = false;
		ActiveMenu.Reset();
	}

	/** Called when you've successfully summoned your context menu */
	void SummonSucceeded(const TSharedRef<IMenu>& InMenu)
	{
		bIsPendingSummon = false;
		ActiveMenu = InMenu;
	}

	/** Called if your context menu summon fails */
	void SummonFailed()
	{
		bIsPendingSummon = false;
		ActiveMenu.Reset();
	}

private:
	/** True if we are pending the summon of a context menu, but don't yet have an active window pointer */
	bool bIsPendingSummon;

	/** Handle to the active context menu (if any) */
	TWeakPtr<IMenu> ActiveMenu;
};

class SLATE_API ITextEditorWidget 
{
	public:

	/**
	 * Call this before the text is changed interactively by the user (handles Undo, etc.)
	 */
	virtual void StartChangingText() = 0;

	/**
	 * Called when text is finished being changed interactively by the user
	 */
	virtual void FinishChangingText() = 0;
	
	/**
	 * Can the user edit the text?
	 */
	virtual bool GetIsReadOnly() const = 0;

	/**
	 * Deletes the character to the left of the caret
	 */
	virtual void BackspaceChar() = 0;

	/**
	 * Deletes the character to the right of the caret
	 */
	virtual void DeleteChar() = 0;
	
	/**
	 * The user pressed this character key. Should we actually type it?
	 * NOT RELIABLE VALIDATION.
	 */
	virtual bool CanTypeCharacter(const TCHAR CharInQuestion) const = 0;

	/**
	 * Adds a character at the caret position
	 *
	 * @param  InChar  Character to add
	 */
	virtual void TypeChar( const int32 Character ) = 0;
	

	/**
	 * Move the cursor left, right, up down, one character or word at a time.
	 * Optionally select text while moving.
	 */
	virtual FReply MoveCursor( FMoveCursor Args ) = 0;
	
	/**
	 * Jump the cursor to a special location: e.g. end of line, beginning of document, etc..
	 */
	virtual void JumpTo(ETextLocation JumpLocation, ECursorAction Action) = 0;

	/**
	 * Clears text selection
	 */
	virtual void ClearSelection() = 0;
	
	/**
	 * Selects all text
	 */
	virtual void SelectAllText() = 0;

	virtual bool SelectAllTextWhenFocused() = 0;

	virtual void SelectWordAt( const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition ) = 0;

	virtual void BeginDragSelection() = 0;

	virtual bool IsDragSelecting() const = 0;

	virtual void EndDragSelection() = 0;

	virtual bool AnyTextSelected() const = 0;

	virtual bool IsTextSelectedAt( const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition ) const = 0;

	virtual void SetWasFocusedByLastMouseDown( bool Value ) = 0;
	virtual bool WasFocusedByLastMouseDown() const = 0;

	virtual void SetHasDragSelectedSinceFocused( bool Value ) = 0;
	virtual bool HasDragSelectedSinceFocused() const = 0;

	/**
	 * Executed when the user presses ESC
	 */
	virtual FReply OnEscape() = 0;

	/**
	 * Executed when the user presses ENTER
	 */
	virtual void OnEnter() = 0;

	/**
	 * Returns true if 'Cut' is currently a valid action
	 *
	 * @return	True to allow this action, otherwise false
	 */
	virtual bool CanExecuteCut() const = 0;
	
	/**
	 * Cuts selected text to the clipboard
	 */
	virtual void CutSelectedTextToClipboard() = 0;

	/**
	 * Returns true if 'Copy' is currently a valid action
	 *
	 * @return	True to allow this action, otherwise false
	 */
	virtual bool CanExecuteCopy() const = 0;

	/**
	 * Copies selected text to the clipboard
	 */
	virtual void CopySelectedTextToClipboard() = 0;

	/**
	 * Returns true if 'Paste' is currently a valid action
	 *
	 * @return	True to allow this action, otherwise false
	 */
	virtual bool CanExecutePaste() const = 0;

	/**
	 * Pastes text from the clipboard
	 */
	virtual void PasteTextFromClipboard() = 0;
	
	/**
	 * Returns true if 'Undo' is currently a valid action
	 *
	 * @return	True to allow this action, otherwise false
	 */
	virtual bool CanExecuteUndo() const = 0;

	/**
	 * Rolls back state to the most recent undo history's state
	 */
	virtual void Undo() = 0;

	/**
	 * Restores cached undo state that was previously rolled back
	 */
	virtual void Redo() = 0;

	virtual TSharedRef< SWidget > GetWidget() = 0;

	DEPRECATED(4.9, "SummonContextMenu() without a FWidgetPath parameter is deprecated. Use the new version of SummonContextMenu().")
	virtual void SummonContextMenu(const FVector2D& InLocation, TSharedPtr<SWindow> ParentWindow = TSharedPtr<SWindow>())
	{
		SummonContextMenu(InLocation, ParentWindow, FWidgetPath());
	}

	virtual void SummonContextMenu(const FVector2D& InLocation, TSharedPtr<SWindow> ParentWindow, const FWidgetPath& EventPath) = 0;

	virtual void LoadText() = 0;
};
