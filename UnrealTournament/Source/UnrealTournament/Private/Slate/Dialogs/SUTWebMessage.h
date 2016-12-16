// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "../Base/SUTDialogBase.h"
#include "../Panels/SUTWebBrowserPanel.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTWebMessage : public SUTDialogBase
{
public:
	SLATE_BEGIN_ARGS(SUTWebMessage)
	: _DialogTitle(FText::GetEmpty())
	, _DialogSize(FVector2D(1200, 800))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	, _ButtonMask(UTDIALOG_BUTTON_OK)
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
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Browse(FText Caption, FString Url);

protected:
	
	TSharedPtr<SUTWebBrowserPanel> WebBrowser;
	FReply OnButtonClick(uint16 ButtonID);

};

#endif