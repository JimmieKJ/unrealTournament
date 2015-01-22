// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SVerbChoiceDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SVerbChoiceDialog )	{}
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ATTRIBUTE(FText, Message)	
		SLATE_ATTRIBUTE(TArray<FText>, Buttons)
		SLATE_ATTRIBUTE(float, WrapMessageAt)
	SLATE_END_ARGS()

	/** Displays the modal dialog box */
	static int32 ShowModal(const FText& InTitle, const FText& InText, const TArray<FText>& InButtons);

	/** Constructs the dialog */
	void Construct( const FArguments& InArgs );

	/** Override behavior when a key is pressed */
	virtual	FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

	/** Override the base method to allow for keyboard focus */
	virtual bool SupportsKeyboardFocus() const override;

protected:
	/** Copies the message text to the clipboard. */
	void CopyMessageToClipboard( );

private:
	/** Handles clicking a message box button. */
	FReply HandleButtonClicked( int32 InResponse );

	/** Handles clicking the 'Copy Message' hyper link. */
	void HandleCopyMessageHyperlinkNavigate( );

	int32 Response;
	TSharedPtr<SWindow> ParentWindow;
	TAttribute<FText> Message;
	TAttribute< TArray<FText> > Buttons;
};
