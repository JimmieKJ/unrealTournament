// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemSteamPrivatePCH.h"
#include "OnlineExternalUIInterfaceSteam.h"
#include "OnlineSessionInterfaceSteam.h"
#include "OnlineSubsystemSteam.h"

// Other external UI possibilities in Steam
// "Players" - recently played with players
// "Community" 
// "Settings"
// "OfficialGameGroup"
// "Stats"

FString FOnlineAsyncEventSteamExternalUITriggered::ToString() const
{
	return FString::Printf(TEXT("FOnlineAsyncEventSteamExternalUITriggered bIsActive: %d"), bIsActive);
}

void FOnlineAsyncEventSteamExternalUITriggered::TriggerDelegates()
{
	FOnlineAsyncEvent::TriggerDelegates();
	IOnlineExternalUIPtr ExternalUIInterface = Subsystem->GetExternalUIInterface();
	ExternalUIInterface->TriggerOnExternalUIChangeDelegates(bIsActive);

	// Calling this to mimic behavior as close as possible with other platforms where this delegate is passed in (such as PS4/Xbox).
	if (!bIsActive)
	{
		FOnlineExternalUISteamPtr ExternalUISteam = StaticCastSharedPtr<FOnlineExternalUISteam>(ExternalUIInterface);
		ExternalUISteam->ProfileUIClosedDelegate.ExecuteIfBound();
		ExternalUISteam->ProfileUIClosedDelegate.Unbind();
	}
}

bool FOnlineExternalUISteam::ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, const FOnLoginUIClosedDelegate& Delegate)
{
	return false;
}

bool FOnlineExternalUISteam::ShowFriendsUI(int32 LocalUserNum)
{
	SteamFriends()->ActivateGameOverlay("Friends");
	return true;
}

bool FOnlineExternalUISteam::ShowInviteUI(int32 LocalUserNum, FName SessionMame)
{
	IOnlineSessionPtr SessionInt = SteamSubsystem->GetSessionInterface();
	if (SessionInt.IsValid() && SessionInt->HasPresenceSession())
	{
		SteamFriends()->ActivateGameOverlay("LobbyInvite");
		return true;
	}

	return false;
}

bool FOnlineExternalUISteam::ShowAchievementsUI(int32 LocalUserNum)
{
	SteamFriends()->ActivateGameOverlay("Achievements");
	return true;
}

bool FOnlineExternalUISteam::ShowLeaderboardUI(const FString& LeaderboardName)
{
	return false;
}

bool FOnlineExternalUISteam::ShowWebURL(const FString& WebURL)
{
	if (!WebURL.StartsWith(TEXT("https://")))
	{
		SteamFriends()->ActivateGameOverlayToWebPage(TCHAR_TO_UTF8(*FString::Printf(TEXT("https://%s"), *WebURL)));
	}
	else
	{
		SteamFriends()->ActivateGameOverlayToWebPage(TCHAR_TO_UTF8(*WebURL));
	}
	
	return true;
}

bool FOnlineExternalUISteam::ShowProfileUI( const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate )
{
	SteamFriends()->ActivateGameOverlayToUser(TCHAR_TO_UTF8(TEXT("steamid")), (const FUniqueNetIdSteam&)Requestee);

	ProfileUIClosedDelegate = Delegate;
	return true;
}

bool FOnlineExternalUISteam::ShowAccountUpgradeUI(const FUniqueNetId& UniqueId)
{
	return false;
}

