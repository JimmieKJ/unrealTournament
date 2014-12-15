// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "../SUWPanel.h"
#include "UTLocalPlayer.h"

#if !UE_SERVER

#include "SWebBrowser.h"

class SUWStatsViewer : public SUWPanel
{
	virtual void ConstructPanel(FVector2D ViewportSize);

	FPlayerOnlineStatusChangedDelegate PlayerOnlineStatusChangedDelegate;
	virtual void OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID);

protected:
	FString StatsID;

	virtual void DownloadStats();

	TSharedPtr<SHorizontalBox> WebBrowserBox;
	TSharedPtr<SWebBrowser> StatsWebBrowser;
	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;
	IOnlineUserCloudPtr OnlineUserCloudInterface;
	FOnReadUserFileCompleteDelegate OnReadUserFileCompleteDelegate;
	virtual void OnReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName);
	virtual FString GetStatsFilename();
};

#endif