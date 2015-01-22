// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemFacebookPrivatePCH.h"
#include "OnlineIdentityFacebook.h"
#include "OnlineFriendsFacebook.h"

// FOnlineSubsystemFacebook


IOnlineSessionPtr FOnlineSubsystemFacebook::GetSessionInterface() const
{
	return NULL;
}


IOnlineFriendsPtr FOnlineSubsystemFacebook::GetFriendsInterface() const
{
	return FacebookFriends;
}


IOnlineSharedCloudPtr FOnlineSubsystemFacebook::GetSharedCloudInterface() const
{
	return NULL;
}


IOnlineUserCloudPtr FOnlineSubsystemFacebook::GetUserCloudInterface() const
{
	return NULL;
}


IOnlineLeaderboardsPtr FOnlineSubsystemFacebook::GetLeaderboardsInterface() const
{
	return NULL;
}

IOnlineVoicePtr FOnlineSubsystemFacebook::GetVoiceInterface() const
{
	return NULL;
}


IOnlineExternalUIPtr FOnlineSubsystemFacebook::GetExternalUIInterface() const	
{
	return NULL;
}


IOnlineTimePtr FOnlineSubsystemFacebook::GetTimeInterface() const
{
	return NULL;
}


IOnlineIdentityPtr FOnlineSubsystemFacebook::GetIdentityInterface() const
{
	return FacebookIdentity;
}


IOnlineTitleFilePtr FOnlineSubsystemFacebook::GetTitleFileInterface() const
{
	return NULL;
}


IOnlineEntitlementsPtr FOnlineSubsystemFacebook::GetEntitlementsInterface() const
{
	return NULL;
}


IOnlineStorePtr FOnlineSubsystemFacebook::GetStoreInterface() const
{
	return NULL;
}

IOnlineEventsPtr FOnlineSubsystemFacebook::GetEventsInterface() const
{
	return NULL;
}

IOnlineAchievementsPtr FOnlineSubsystemFacebook::GetAchievementsInterface() const
{
	return NULL;
}


IOnlineSharingPtr FOnlineSubsystemFacebook::GetSharingInterface() const
{
	return NULL;
}


IOnlineUserPtr FOnlineSubsystemFacebook::GetUserInterface() const
{
	return NULL;
}

IOnlineMessagePtr FOnlineSubsystemFacebook::GetMessageInterface() const
{
	return NULL;
}

IOnlinePresencePtr FOnlineSubsystemFacebook::GetPresenceInterface() const
{
	return NULL;
}

IOnlinePartyPtr FOnlineSubsystemFacebook::GetPartyInterface() const
{
	return NULL;
}

IOnlineChatPtr FOnlineSubsystemFacebook::GetChatInterface() const
{
	return NULL;
}

bool FOnlineSubsystemFacebook::Tick(float DeltaTime)
{
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
	
	FacebookIdentity = NULL;
	FacebookFriends = NULL;
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
	: FacebookIdentity(NULL)
{

}

FOnlineSubsystemFacebook::~FOnlineSubsystemFacebook()
{

}
