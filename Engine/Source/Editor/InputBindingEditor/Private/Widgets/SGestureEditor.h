// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * A specialized text edit box that visualizes a new gesture being entered           .
 */
class SGestureEditor
	: public SEditableText
{
public:

	SLATE_BEGIN_ARGS( SGestureEditor ){}
		SLATE_EVENT( FSimpleDelegate, OnEditBoxLostFocus )
		SLATE_EVENT( FSimpleDelegate, OnGestureChanged )
		SLATE_EVENT( FSimpleDelegate, OnEditingStopped )
		SLATE_EVENT( FSimpleDelegate, OnEditingStarted )
	SLATE_END_ARGS()

public:

	/** Default constructor. */
	SGestureEditor()
		: bIsEditing( false )
		, bIsTyping( false )
	{ }

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InputCommand
	 */
	void Construct( const FArguments& InArgs, TSharedPtr<FGestureTreeItem> InputCommand );
	
	virtual bool SupportsKeyboardFocus() const { return true; }

	/** Starts editing the gesture. */
	void StartEditing();
	
	/** Stops editing the gesture. */
	void StopEditing();

	/** Commits the new gesture to the commands active gesture. */
	void CommitNewGesture();

	/** Removes the active gesture from the command. */
	void RemoveActiveGesture();

	/** @return Whether or not we are in editing mode. */
	bool IsEditing() const { return bIsEditing; }

	/** @return True if the user is physically typing a key. */
	bool IsTyping() const { return bIsTyping; }

	/** @return Whether or not the gesture being edited is valid. */
	bool IsEditedGestureValid() const { return EditingInputGesture.IsValidGesture(); }

	/** @return Whether or not the command has a valid gesture. */
	bool IsActiveGestureValid() const { return CommandInfo->GetActiveGesture()->IsValidGesture(); }

	/** @return the Notification message being displayed if any. */
	const FText& GetNotificationText() const { return NotificationMessage; };

	/** @return true if the edited gesture has a conflict with an existing gesture. */
	bool HasConflict() const { return !NotificationMessage.IsEmpty(); }

private:

	/** Gesture that is currently being edited. */
	static TWeakPtr<SGestureEditor> GestureBeingEdited;

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	virtual FReply OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent ) override;

	/** 
	 * Called when the gesture changes.
	 *
	 * @param NewGesture The new gesture.
	 */
	void OnGestureTyped( const FInputGesture& NewGesture );

	/** 
	 * Called when the gesture changes.
	 *
	 * @param NewGesture The gesture to commit.
	 */
	void OnGestureCommitted( const FInputGesture& NewGesture );

	/** @return The gesture input text (If editing, the edited gesture, otherwise the active gesture) */
	FText OnGetGestureInputText() const;
	
	/** @return The hint text to display in the text box if it is empty */
	FText OnGetGestureInputHintText() const;

private:

	/** The command we are editing a gesture for. */
	TSharedPtr<FUICommandInfo> CommandInfo;

	/** Delegate to execute when the edit box loses focus. */
	FSimpleDelegate OnEditBoxLostFocus;

	/** Delegate to execute when the gesture changes. */
	FSimpleDelegate OnGestureChanged;

	/** Delegate to execute when we stop editing. */
	FSimpleDelegate OnEditingStopped;

	/** Delegate to execute when we start editing. */
	FSimpleDelegate OnEditingStarted;

	/** The notification message (duplicate bindings) being displayed. */
	FText NotificationMessage;

	/** Temp gesture being edited. */
	FInputGesture EditingInputGesture;

	/** Whether or not we are in edit mode. */
	bool bIsEditing;

	/** Whether or not the user is physically typing a new key. */
	bool bIsTyping;
};
