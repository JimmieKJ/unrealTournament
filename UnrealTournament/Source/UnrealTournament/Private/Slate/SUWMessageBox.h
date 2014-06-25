// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Slate.h"

DECLARE_DELEGATE(FOnMessageBoxClosed);


class SUWMessageBox : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUWMessageBox)
	{}

	SLATE_ARGUMENT(TWeakObjectPtr<AUTPlayerController>, PlayerOwner)

	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	
	FOnMessageBoxClosed& OnMessageBoxClosed() { return MessageBoxClosed;}

protected:
	
	FOnMessageBoxClosed MessageBoxClosed;

private:
	TWeakObjectPtr<class AUTPlayerController> PlayerOwner;

};

