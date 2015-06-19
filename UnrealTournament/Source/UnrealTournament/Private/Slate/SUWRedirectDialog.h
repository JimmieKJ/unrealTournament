// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SUWDialog.h"

#if !UE_SERVER
#define NUM_REDIRECT_SAMPLES 5

#include "Http.h"

class UNREALTOURNAMENT_API SUWRedirectDialog : public SUWDialog
{
public:

	SLATE_BEGIN_ARGS(SUWRedirectDialog)
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
		SLATE_ARGUMENT(FString, RedirectToURL)
		SLATE_ARGUMENT(TArray<FString>, RedirectURLs)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
protected:

	virtual FReply OnButtonClick(uint16 ButtonID);

	TOptional<float> GetProgressFile() const
	{
		if (AssetsTotalSize == 0)
		{
			return 0.0f;
		}

		return float(AssetsDownloadedAmount) / float(AssetsTotalSize);
	}

	FText GetFileName() const
	{
		return FText::FromString(RedirectToURL);
	}
	
	FText GetProgressFileText() const;

	bool DownloadFile(FString URL);
	void CancelDownload();

	bool bDownloadCanceled;

	void HttpRequestProgress(FHttpRequestPtr HttpRequest, int32 NumBytesSent, int32 NumBytesRecv);
	void HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	FHttpRequestPtr HttpRequest;

	FString RedirectToURL;
	TArray<FString> RedirectURLs;
	
	int32 AssetsTotalSize;
	int32 AssetsDownloadedAmount;

	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;

	FText BytesToText(int32 Bytes) const;
	FText SecondsToText(float Seconds) const;

	void UpdateETA();

	float LastETATime;
	int32 LastDownloadedAmount;
	float SecondsRemaining;

	//Sample stuff to smooth out the ETA
	int32 ByteSamples[NUM_REDIRECT_SAMPLES];
	int32 NumSamples;
	int32 CurrentSample;
	void AddSample(int32 Sample);
	float GetAverageBytes() const;

public:
	virtual void Tick(const FGeometry & AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
};

#endif