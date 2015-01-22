// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemFacebookPrivatePCH.h"
#include "IOSAppDelegate.h"

#include "OnlineSharingFacebook.h"
#include "OnlineUserFacebook.h"

FOnlineSubsystemFacebook::FOnlineSubsystemFacebook() 
	: FacebookIdentity(NULL)
	, FacebookFriends(NULL)
	, FacebookSharing(NULL)
{

}

FOnlineSubsystemFacebook::~FOnlineSubsystemFacebook()
{
	FacebookIdentity = NULL;
	FacebookFriends = NULL;
	FacebookSharing = NULL; 
}

IOnlineSessionPtr FOnlineSubsystemFacebook::GetSessionInterface() const
{
	return NULL;
}

IOnlineIdentityPtr FOnlineSubsystemFacebook::GetIdentityInterface() const
{
	return FacebookIdentity;
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
	return FacebookSharing;
}

IOnlineUserPtr FOnlineSubsystemFacebook::GetUserInterface() const
{
	return FacebookUser;
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

bool FOnlineSubsystemFacebook::Init() 
{
	bool bSuccessfullyStartedUp = true;

	FacebookIdentity = MakeShareable(new FOnlineIdentityFacebook());
	FacebookSharing = MakeShareable(new FOnlineSharingFacebook(this));
	FacebookFriends = MakeShareable(new FOnlineFriendsFacebook(this));
	FacebookUser = MakeShareable(new FOnlineUserFacebook(this));
	
	return bSuccessfullyStartedUp;
}

bool FOnlineSubsystemFacebook::Shutdown() 
{
	bool bSuccessfullyShutdown = true;


	return bSuccessfullyShutdown;
}

FString FOnlineSubsystemFacebook::GetAppId() const 
{
	return TEXT( "" );
}

bool FOnlineSubsystemFacebook::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)  
{
	return false;
}

bool FOnlineSubsystemFacebook::Tick(float DeltaTime)
{
	return true;
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