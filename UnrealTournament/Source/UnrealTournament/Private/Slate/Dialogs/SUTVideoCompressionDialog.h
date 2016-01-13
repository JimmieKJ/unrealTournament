// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#if !UE_SERVER

#include "SlateBasics.h"
#include "../Base/SUTDialogBase.h"

class UNREALTOURNAMENT_API SUTVideoCompressionDialog : public SUTDialogBase
{
public:

	SLATE_BEGIN_ARGS(SUTVideoCompressionDialog)
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
		SLATE_ARGUMENT(class UTVideoRecordingFeature*, VideoRecorder)
		SLATE_ARGUMENT(FString, VideoFilename)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TOptional<float> GetProgressCompression() const;

	FReply OnButtonClick(uint16 ButtonID);

	void CompressingComplete(bool bSuccessful);

	class UTVideoRecordingFeature* VideoRecorder;
};

#endif