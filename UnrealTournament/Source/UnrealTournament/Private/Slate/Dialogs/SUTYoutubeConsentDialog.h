// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "../Base/SUTDialogBase.h"
#include "SWebBrowser.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTYoutubeConsentDialog : public SUTDialogBase
{
public:
	SLATE_BEGIN_ARGS(SUTYoutubeConsentDialog)
		: _DialogSize(FVector2D(0.5f, 0.25f))
		, _bDialogSizeIsRelative(true)
		, _DialogPosition(FVector2D(0.5f, 0.5f))
		, _DialogAnchorPoint(FVector2D(0.5f, 0.5f))
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
		SLATE_EVENT(FDialogResultDelegate, OnDialogResult)
		SLATE_ARGUMENT(FString, RequestURL)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	FString UniqueCode;

private:
	virtual FReply OnButtonClick(uint16 ButtonID);

	void OnTitleChanged(const FText& NewText);

	TSharedPtr<SWebBrowser> ConsentBrowser;

public:
	virtual void Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
};

#endif
