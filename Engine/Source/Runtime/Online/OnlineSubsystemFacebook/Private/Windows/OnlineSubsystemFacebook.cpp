// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemFacebookPrivatePCH.h"
#include "OnlineIdentityFacebook.h"
#include "OnlineFriendsFacebook.h"

// FOnlineSubsystemFacebook

IOnlineSessionPtr FOnlineSubsystemFacebook::GetSessionInterface() const
{
	return nullptr;
}

IOnlineFriendsPtr FOnlineSubsystemFacebook::GetFriendsInterface() const
{
	return FacebookFriends;
}

IOnlinePartyPtr FOnlineSubsystemFacebook::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemFacebook::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemFacebook::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemFacebook::GetUserCloudInterface() const
{
	return nullptr;
}

IOnlineLeaderboardsPtr FOnlineSubsystemFacebook::GetLeaderboardsInterface() const
{
	return nullptr;
}

IOnlineVoicePtr FOnlineSubsystemFacebook::GetVoiceInterface() const
{
	return nullptr;
}

IOnlineExternalUIPtr FOnlineSubsystemFacebook::GetExternalUIInterface() const	
{
	return nullptr;
}

IOnlineTimePtr FOnlineSubsystemFacebook::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemFacebook::GetIdentityInterface() const
{
	return FacebookIdentity;
}

IOnlineTitleFilePtr FOnlineSubsystemFacebook::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemFacebook::GetEntitlementsInterface() const
{
	return nullptr;
}

IOnlineStorePtr FOnlineSubsystemFacebook::GetStoreInterface() const
{
	return nullptr;
}

IOnlineEventsPtr FOnlineSubsystemFacebook::GetEventsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemFacebook::GetAchievementsInterface() const
{
	return nullptr;
}

IOnlineSharingPtr FOnlineSubsystemFacebook::GetSharingInterface() const
{
	return nullptr;
}

IOnlineUserPtr FOnlineSubsystemFacebook::GetUserInterface() const
{
	return nullptr;
}

IOnlineMessagePtr FOnlineSubsystemFacebook::GetMessageInterface() const
{
	return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemFacebook::GetPresenceInterface() const
{
	return nullptr;
}

IOnlineChatPtr FOnlineSubsystemFacebook::GetChatInterface() const
{
	return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemFacebook::GetTurnBasedInterface() const
{
    return nullptr;
}

bool FOnlineSubsystemFacebook::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	if (FacebookIdentity.IsValid())
	{		
		FacebookIdentity->Tick(DeltaTime);
	}
	return true;
}

bool FOnlineSubsystemFacebook::Init()
{
	FacebookIdentity = MakeShareable(new FOnlineIdentityFacebook());
	FacebookFriends = MakeShareable(new FOnlineFriendsFacebook(this));
	return true;
}

bool FOnlineSubsystemFacebook::Shutdown()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineSubsystemFacebook::Shutdown()"));
	
	FOnlineSubsystemImpl::Shutdown();

	FacebookIdentity = nullptr;
	FacebookFriends = nullptr;
	return true;
}

FString FOnlineSubsystemFacebook::GetAppId() const
{
	return TEXT("Facebook");
}

bool FOnlineSubsystemFacebook::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) 
{
	return false;
}

bool FOnlineSubsystemFacebook::IsEnabled()
{
	// Check the ini for disabling Facebook
	bool bEnableFacebook = false;
	GConfig->GetBool(TEXT("OnlineSubsystemFacebook"), TEXT("bEnabled"), bEnableFacebook, GEngineIni);

#if !UE_BUILD_SHIPPING
	// Check the commandline for disabling Facebook
	bEnableFacebook = bEnableFacebook && !FParse::Param(FCommandLine::Get(),TEXT("NOFACEBOOK"));
#endif

	return bEnableFacebook;
}

FOnlineSubsystemFacebook::FOnlineSubsystemFacebook() 
	: FacebookIdentity(nullptr)
{

}

FOnlineSubsystemFacebook::~FOnlineSubsystemFacebook()
{

}
