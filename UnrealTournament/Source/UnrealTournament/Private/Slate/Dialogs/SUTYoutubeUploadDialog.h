// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "../Base/SUTDialogBase.h"

#if !UE_SERVER
class UNREALTOURNAMENT_API SUTYoutubeUploadDialog : public SUTDialogBase
{
public:
	SLATE_BEGIN_ARGS(SUTYoutubeUploadDialog)
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
		SLATE_ARGUMENT(FString, VideoFilename)
		SLATE_ARGUMENT(FString, AccessToken)
		SLATE_ARGUMENT(FString, VideoTitle)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TOptional<float> GetProgressCompression() const;

	FReply OnButtonClick(uint16 ButtonID);

	FString VideoFilename;
	FString AccessToken;
	FString VideoTitle;
	TArray<uint8> YoutubeUploadBinaryData;
	FHttpRequestPtr YoutubeUploadRequest;

	int32 BytesUploaded;
	int64 FileSizeInBytes;
	FString UploadFailMessage;
	
	void YoutubeResumableSessionRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void YoutubeFileUploadRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void YoutubeFileUploadProgress(FHttpRequestPtr InHttpRequest, int32 NumBytesSent, int32 NumBytesRecv);
};
#endif