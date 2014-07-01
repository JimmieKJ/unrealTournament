// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Slate.h"

class SUWMessageBox : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUWMessageBox)
	{}

	SLATE_ARGUMENT(TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner)
	SLATE_ARGUMENT(FText, MessageTitle)
	SLATE_ARGUMENT(FText, MessageText)
	SLATE_ARGUMENT(uint16, ButtonsMask)
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	virtual void OnDialogOpened();
	virtual void OnDialogClosed();

private:
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	TSharedPtr<class SWidget> GameViewportWidget;
	TSharedPtr<class SCanvas> Canvas;

	TSharedRef<class SWidget> BuildButtonBar(uint16 ButtonMask);
	void BuildButton(TSharedPtr<SHorizontalBox> Bar, FText ButtonText, uint16 ButtonID);
	virtual FReply OnButtonClick(uint16 ButtonID);	


	// HACKS needed to keep window focus
	virtual bool SupportsKeyboardFocus() const OVERRIDE;
	virtual FReply OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent ) OVERRIDE;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent ) OVERRIDE;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent ) OVERRIDE;

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;


};

