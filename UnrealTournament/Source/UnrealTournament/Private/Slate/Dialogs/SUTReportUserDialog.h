// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "../Base/SUTDialogBase.h"
#include "../Panels/SUTWebBrowserPanel.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTReportUserDialog : public SUTDialogBase
{
public:
	SLATE_BEGIN_ARGS(SUTReportUserDialog)
	: _DialogTitle(NSLOCTEXT("SUTReportUserDialog","Title","REPORT USER"))
	, _DialogSize(FVector2D(1000, 600))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	, _ButtonMask(UTDIALOG_BUTTON_CANCEL)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_ARGUMENT(FText, DialogTitle)											
	SLATE_ARGUMENT(FVector2D, DialogSize)										
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)									
	SLATE_ARGUMENT(FVector2D, DialogPosition)									
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)								
	SLATE_ARGUMENT(FVector2D, ContentPadding)									
	SLATE_ARGUMENT(uint16, ButtonMask)
	SLATE_ARGUMENT(TWeakObjectPtr<class AUTPlayerState>, Troll)
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)							
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	FReply OnButtonClick(uint16 ButtonID);
	TWeakObjectPtr<class AUTPlayerState> Troll;
	TSharedRef<SWidget> AddGridButton(FText Caption, int32 AbuseType);
	FReply OnReportClicked(int32 ReportType);
};

#endif