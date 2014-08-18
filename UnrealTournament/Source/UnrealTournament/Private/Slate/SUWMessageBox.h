// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "SUWDialog.h"

class SUWMessageBox : public SUWDialog
{
	SLATE_BEGIN_ARGS(SUWMessageBox)
	: _MaxWidthPct(0.35f)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner)
	SLATE_ARGUMENT(FText, MessageTitle)
	SLATE_ARGUMENT(FText, MessageText)
	SLATE_ARGUMENT(float, MaxWidthPct) // 0 to 1 fraction of viewport size for maximum box width
	SLATE_ARGUMENT(uint16, ButtonsMask)
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

protected:
	FDialogResultDelegate OnDialogResult;
	
	TSharedRef<class SWidget> BuildButtonBar(uint16 ButtonMask);
	void BuildButton(TSharedPtr<SUniformGridPanel> Bar, FText ButtonText, uint16 ButtonID, uint32 &ButtonCount);
	virtual FReply OnButtonClick(uint16 ButtonID);	
};

