// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * A widget that adds visuals to an SGestureEditor
 */
class SGestureEditBox
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SGestureEditBox ){}
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	void Construct( const FArguments& InArgs, TSharedPtr<struct FGestureTreeItem> InputCommand );

private:

	/** @return Border image for the text box based on the hovered and focused state */
	const FSlateBrush* GetBorderImage() const;

	FText GetNotificationMessage() const;

	/** Called when the gesture editor loses focus */
	void OnGestureEditorLostFocus();

	/** Called when editing starts in the gesture editor */
	void OnGestureEditingStarted();

	/** Called when editing stops in the gesture editor */
	void OnGestureEditingStopped();

	/** Called when the edited gesture changes */
	void OnGestureChanged();

	/** @return the visibility of the gesture edit button */
	EVisibility GetGestureRemoveButtonVisibility() const;

	/** Called when the gesture edit button is clicked */
	FReply OnGestureRemoveButtonClicked();

	/** Called when the accept button is clicked.  */
	FReply OnAcceptNewGestureButtonClicked();

	/** @return content to be shown in the key binding conflict pop-up */
	TSharedRef<SWidget> OnGetContentForConflictPopup();

	/** @return The visibility of the duplicate binding notification area */
	EVisibility GetNotificationVisibility() const; 
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& InMouseEvent );

private:

	/** The gesture editor for this box */
	TSharedPtr<class SGestureEditor> GestureEditor;
	
	/** Menu anchor where the conflict pop-up is shown */
	TSharedPtr<SMenuAnchor> ConflictPopup;
	
	/** The button for committing gesture */
	mutable TSharedPtr<SButton> GestureAcceptButton;
	
	/** Styling: border image to draw when not hovered or focused */
	const FSlateBrush* BorderImageNormal;
	
	/** Styling: border image to draw when hovered */
	const FSlateBrush* BorderImageHovered;
	
	/** Styling: border image to draw when focused */
	const FSlateBrush* BorderImageFocused;
};
