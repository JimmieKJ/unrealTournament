// a simple generic input box that has a single text field, OK/Cancel buttons, and supports a filtering delegate
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "../Base/SUTDialogBase.h"

#if !UE_SERVER

/** returns the result of this quickpick menu */
DECLARE_DELEGATE_OneParam(FQuickPickResult, int32);

class UNREALTOURNAMENT_API SUTQuickPickDialog : public SUTDialogBase
{
	SLATE_BEGIN_ARGS(SUTQuickPickDialog)
	: _DialogTitle(NSLOCTEXT("SUTQuickPickDialog","QuickPickDefaultTitle","Choose..."))
	{}

	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_ARGUMENT(FText, DialogTitle)											
	SLATE_EVENT(FQuickPickResult, OnPickResult)							
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	void AddOption(TSharedPtr<SWidget> NewOption, bool bLast = false);

protected:
	int32 OptionIndex;
	TSharedPtr<SVerticalBox> ContentBox;
	FQuickPickResult ResultDelegate;

	FReply OnOptionClicked(int32 PickedIndex);
};

#endif