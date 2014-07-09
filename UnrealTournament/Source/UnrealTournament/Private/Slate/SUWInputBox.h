// a simple generic input box that has a single text field, OK/Cancel buttons, and supports a filtering delegate
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "SUWDialog.h"

DECLARE_DELEGATE_TwoParams(FInputBoxResultDelegate, const FString&, bool);
/** function called to filter characters that are input; return true to keep it, false to remove it */
DECLARE_DELEGATE_RetVal_OneParam(bool, FInputBoxFilterDelegate, TCHAR);

class SUWInputBox : public SUWDialog
{
	SLATE_BEGIN_ARGS(SUWInputBox)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner)
	SLATE_ARGUMENT(FText, MessageTitle)
	SLATE_ARGUMENT(FText, MessageText)
	SLATE_ARGUMENT(FString, DefaultInput)
	SLATE_EVENT(FInputBoxResultDelegate, OnDialogResult)
	SLATE_EVENT(FInputBoxFilterDelegate, TextFilter)
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

protected:
	TSharedPtr<class SCanvas> Canvas;
	TSharedPtr<class SEditableTextBox> EditBox;
	FInputBoxResultDelegate OnDialogResult;
	FInputBoxFilterDelegate TextFilter;
	
	virtual void OnDialogOpened() OVERRIDE;

	TSharedRef<class SWidget> BuildButtonBar();
	void BuildButton(TSharedPtr<SUniformGridPanel> Bar, FText ButtonText, uint16 ButtonID, uint32& ButtonCount);
	void OnTextChanged(const FText& NewText);
	void OnTextCommited(const FText& NewText, ETextCommit::Type CommitType);
	virtual FReply OnButtonClick(uint16 ButtonID);
};

