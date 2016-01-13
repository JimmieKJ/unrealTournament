// a simple generic input box that has a single text field, OK/Cancel buttons, and supports a filtering delegate
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "../Base/SUTDialogBase.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTDownloadAllDialog : public SUTDialogBase
{
	SLATE_BEGIN_ARGS(SUTDownloadAllDialog)
	: _DialogTitle(NSLOCTEXT("SUTDownloadAllDialog", "Title", "DOWNLOADING ALL CONTENT...."))
	, _DialogSize(FVector2D(1000,300))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	, _ButtonMask(UTDIALOG_BUTTON_CLOSE)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_ARGUMENT(FText, DialogTitle)											
	SLATE_ARGUMENT(FVector2D, DialogSize)										
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)									
	SLATE_ARGUMENT(FVector2D, DialogPosition)									
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)								
	SLATE_ARGUMENT(FVector2D, ContentPadding)
	SLATE_ARGUMENT(uint16, ButtonMask)

	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	void OnDialogClosed() override;
	void Start();
	void Done();

protected:

	TSharedPtr<SVerticalBox> MessageBox;
	TOptional<float> GetProgress() const;
	FText GetNumFilesLeft() const;
	bool bActive;
	bool bDone;
	FText GetFilename() const;

};

#endif