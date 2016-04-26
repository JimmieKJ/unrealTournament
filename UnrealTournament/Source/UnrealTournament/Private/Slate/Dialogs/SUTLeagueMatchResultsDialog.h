// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SUTDialogBase.h"

#if !UE_SERVER
class UNREALTOURNAMENT_API SUTLeagueMatchResultsDialog : public SUTDialogBase
{
public:
	SLATE_BEGIN_ARGS(SUTLeagueMatchResultsDialog)
		: _MessageTextStyleName(TEXT("UT.Common.NormalText"))
		, _DialogSize(FVector2D(0.5f, 0.25f))
		, _bDialogSizeIsRelative(true)
		, _DialogPosition(FVector2D(0.5f, 0.5f))
		, _DialogAnchorPoint(FVector2D(0.5f, 0.5f))
		, _ContentPadding(FVector2D(10.0f, 5.0f))
		, _ButtonMask(UTDIALOG_BUTTON_OK)
		, _Tier(0)
		, _Division(0)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)
		SLATE_ARGUMENT(FText, DialogTitle)
		SLATE_ARGUMENT(FText, MessageText)
		SLATE_ARGUMENT(FString, MessageTextStyleName)
		SLATE_ARGUMENT(FVector2D, DialogSize)
		SLATE_ARGUMENT(bool, bDialogSizeIsRelative)
		SLATE_ARGUMENT(FVector2D, DialogPosition)
		SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)
		SLATE_ARGUMENT(FVector2D, ContentPadding)
		SLATE_ARGUMENT(uint16, ButtonMask)
		SLATE_ARGUMENT(int32, Tier)
		SLATE_ARGUMENT(int32, Division)
		SLATE_EVENT(FDialogResultDelegate, OnDialogResult)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
		
	FText LeagueTierToText(int32 Tier);
	FString LeagueTierToBrushName(int32 Tier);
};
#endif