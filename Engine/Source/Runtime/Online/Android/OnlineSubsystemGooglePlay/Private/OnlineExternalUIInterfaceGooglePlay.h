// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OnlineDelegateMacros.h"

#include "OnlineExternalUIInterface.h"
#include "OnlineSubsystemGooglePlayPackage.h"

class FOnlineSubsystemGooglePlay;

/** 
 * Interface definition for the online services external UIs
 * Any online service that provides extra UI overlays will implement the relevant functions
 */
class FOnlineExternalUIGooglePlay :
	public IOnlineExternalUI
{
private:
	FOnlineSubsystemGooglePlay* Subsystem;

public:
	FOnlineExternalUIGooglePlay(FOnlineSubsystemGooglePlay* InSubsystem);

	virtual bool ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, const FOnLoginUIClosedDelegate& Delegate) override;
	virtual bool ShowFriendsUI(int32 LocalUserNum) override;
	virtual bool ShowInviteUI(int32 LocalUserNum, FName SessionMame) override;
	virtual bool ShowAchievementsUI(int32 LocalUserNum) override;
	virtual bool ShowLeaderboardUI(const FString& LeaderboardName) override;
	virtual bool ShowWebURL(const FString& WebURL) override;
	virtual bool ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate) override;
	virtual bool ShowAccountUpgradeUI(const FUniqueNetId& UniqueId) override;
};

typedef TSharedPtr<FOnlineExternalUIGooglePlay, ESPMode::ThreadSafe> FOnlineExternalUIGooglePlayPtr;

