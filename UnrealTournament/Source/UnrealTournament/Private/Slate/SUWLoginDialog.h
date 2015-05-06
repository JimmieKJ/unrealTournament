// a simple generic input box that has a single text field, OK/Cancel buttons, and supports a filtering delegate
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SUWDialog.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUWLoginDialog : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUWLoginDialog)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)							

	SLATE_ARGUMENT(FText, ErrorText)
	SLATE_ARGUMENT(FString, UserIDText)
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	FString GetEpicID();
	FString GetPassword();
	void SetInitialFocus();
protected:

	// The Dialog Result delegate
	FDialogResultDelegate OnDialogResult;

	/** Holds a reference to the SCanvas widget that makes up the dialog */
	TSharedPtr<SCanvas> Canvas;

	/** Holds a reference to the SOverlay that defines the content for this dialog */
	TSharedPtr<SOverlay> WindowContent;

	TSharedPtr<class SEditableTextBox> UserEditBox;
	TSharedPtr<class SEditableTextBox> PassEditBox;

	FReply OnNewAccountClick();
	FReply OnForgotPasswordClick();
	FReply OnCloseClick();
	FReply OnSignInClick();

	void OnTextCommited(const FText& NewText, ETextCommit::Type CommitType);

	FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

private:
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent) override;

};

#endif