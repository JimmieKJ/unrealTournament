// a simple generic input box that has a single text field, OK/Cancel buttons, and supports a filtering delegate
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "SUWDialog.h"

#if !UE_SERVER

class SUWLoginDialog : public SUWDialog
{
	SLATE_BEGIN_ARGS(SUWLoginDialog)
	: _DialogTitle(NSLOCTEXT("MCPMessages", "LogonDialogTitle", "- Log in to Epic -"))
	, _DialogSize(FVector2D(400, 500))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	, _ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_ARGUMENT(FText, DialogTitle)											
	SLATE_ARGUMENT(FVector2D, DialogSize)										
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)									
	SLATE_ARGUMENT(FVector2D, DialogPosition)									
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)								
	SLATE_ARGUMENT(FVector2D, ContentPadding)									
	SLATE_ARGUMENT(uint16, ButtonMask)
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)							

	SLATE_ARGUMENT(FText, ErrorText)
	SLATE_ARGUMENT(FString, UserIDText)
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	FString GetEpicID();
	FString GetPassword();


protected:
	TSharedPtr<class SEditableTextBox> UserEditBox;
	TSharedPtr<class SEditableTextBox> PassEditBox;
	
	virtual void OnDialogOpened() override;
	virtual TSharedRef<class SWidget> BuildCustomButtonBar();
	FReply NewAccountClick();
	void OnTextCommited(const FText& NewText, ETextCommit::Type CommitType);

};

#endif