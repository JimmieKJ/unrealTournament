// a simple generic input box that has a single text field, OK/Cancel buttons, and supports a filtering delegate
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SUWDialog.h"

#if !UE_SERVER
/** function called to filter characters that are input; return true to keep it, false to remove it */
DECLARE_DELEGATE_RetVal_OneParam(bool, FInputBoxFilterDelegate, TCHAR);

class UNREALTOURNAMENT_API SUWInputBox : public SUWDialog
{
	SLATE_BEGIN_ARGS(SUWInputBox)
	: _DialogSize(FVector2D(700,300))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	, _MaxInputLength(255)
	, _ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
	, _IsPassword(false)
	, _IsScrollable(true)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_ARGUMENT(FText, DialogTitle)											
	SLATE_ARGUMENT(FVector2D, DialogSize)										
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)									
	SLATE_ARGUMENT(FVector2D, DialogPosition)									
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)								
	SLATE_ARGUMENT(FVector2D, ContentPadding)
	SLATE_ARGUMENT(uint32, MaxInputLength)
	SLATE_ARGUMENT(uint16, ButtonMask)
	/** Sets whether this text box is for storing a password */
	SLATE_ARGUMENT(bool, IsPassword)
	SLATE_ARGUMENT(bool, IsScrollable)
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)							

	SLATE_ARGUMENT(FText, MessageText)
	SLATE_ARGUMENT(FString, DefaultInput)
	SLATE_EVENT(FInputBoxFilterDelegate, TextFilter)
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	FString GetInputText();

protected:
	TSharedPtr<class SEditableTextBox> EditBox;
	FInputBoxFilterDelegate TextFilter;
	uint32 MaxInputLength;

	/** Sets whether this text box is for storing a password */
	TAttribute< bool > IsPassword;
	
	virtual void OnDialogOpened() override;
	void OnTextChanged(const FText& NewText);
	void OnTextCommited(const FText& NewText, ETextCommit::Type CommitType);
};

#endif