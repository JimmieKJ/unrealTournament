// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "../SUWPanel.h"
#include "UTLocalPlayer.h"

#if !UE_SERVER

#include "SWebBrowser.h"

class UNREALTOURNAMENT_API SUWStatsViewer : public SUWPanel
{

	virtual void ConstructPanel(FVector2D ViewportSize);

	FDelegateHandle PlayerOnlineStatusChangedDelegate;
	virtual void OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID);

	virtual void OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow) override;
protected:
	FString StatsID;

	double LastStatsDownloadTime;

	virtual void DownloadStats();

	TSharedPtr<SHorizontalBox> WebBrowserBox;
	TSharedPtr<SWebBrowser> StatsWebBrowser;
	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;
	IOnlineUserCloudPtr OnlineUserCloudInterface;
	FOnReadUserFileCompleteDelegate OnReadUserFileCompleteDelegate;
	FDelegateHandle OnReadUserFileCompleteDelegateHandle;
	virtual void OnReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName);
	virtual FString GetStatsFilename();

	void ReadBackendStats();
	void ReadBackendStatsComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void ShowErrorPage();
public:
	virtual ~SUWStatsViewer();
};

#endif