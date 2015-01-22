// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemIOSPrivatePCH.h"


FOnlineSubsystemIOS::FOnlineSubsystemIOS()
{

}


IOnlineSessionPtr FOnlineSubsystemIOS::GetSessionInterface() const
{
	return SessionInterface;
}


IOnlineFriendsPtr FOnlineSubsystemIOS::GetFriendsInterface() const
{
	return FriendsInterface;
}


IOnlineSharedCloudPtr FOnlineSubsystemIOS::GetSharedCloudInterface() const
{
	return NULL;
}


IOnlineUserCloudPtr FOnlineSubsystemIOS::GetUserCloudInterface() const
{
	return NULL;
}


IOnlineLeaderboardsPtr FOnlineSubsystemIOS::GetLeaderboardsInterface() const
{
	return LeaderboardsInterface;
}

IOnlineVoicePtr FOnlineSubsystemIOS::GetVoiceInterface() const
{
	return NULL;
}


IOnlineExternalUIPtr FOnlineSubsystemIOS::GetExternalUIInterface() const
{
	return ExternalUIInterface;
}


IOnlineTimePtr FOnlineSubsystemIOS::GetTimeInterface() const
{
	return NULL;
}


IOnlineIdentityPtr FOnlineSubsystemIOS::GetIdentityInterface() const
{
	return IdentityInterface;
}


IOnlinePartyPtr FOnlineSubsystemIOS::GetPartyInterface() const
{
	return NULL;
}


IOnlineTitleFilePtr FOnlineSubsystemIOS::GetTitleFileInterface() const
{
	return NULL;
}


IOnlineEntitlementsPtr FOnlineSubsystemIOS::GetEntitlementsInterface() const
{
	return NULL;
}


IOnlineStorePtr FOnlineSubsystemIOS::GetStoreInterface() const
{
	return StoreInterface;
}

IOnlineEventsPtr FOnlineSubsystemIOS::GetEventsInterface() const
{
	return NULL;
}

IOnlineAchievementsPtr FOnlineSubsystemIOS::GetAchievementsInterface() const
{
	return AchievementsInterface;
}


IOnlineSharingPtr FOnlineSubsystemIOS::GetSharingInterface() const
{
	return NULL;
}


IOnlineUserPtr FOnlineSubsystemIOS::GetUserInterface() const
{
	return NULL;
}

IOnlineMessagePtr FOnlineSubsystemIOS::GetMessageInterface() const
{
	return NULL;
}

IOnlinePresencePtr FOnlineSubsystemIOS::GetPresenceInterface() const
{
	return NULL;
}

IOnlineChatPtr FOnlineSubsystemIOS::GetChatInterface() const
{
	return NULL;
}

bool FOnlineSubsystemIOS::Init() 
{
	bool bSuccessfullyStartedUp = true;
	UE_LOG(LogOnline, Display, TEXT("FOnlineSubsystemIOS::Init()"));
	
	bool bIsGameCenterSupported = ([IOSAppDelegate GetDelegate].OSVersion >= 4.1f);
	if( !bIsGameCenterSupported )
	{
		UE_LOG(LogOnline, Warning, TEXT("GameCenter is not supported on systems running IOS 4.0 or earlier."));
		bSuccessfullyStartedUp = false;
	}
	else if( !IsEnabled() )
	{
		UE_LOG(LogOnline, Warning, TEXT("GameCenter has been disabled in the system settings"));
		bSuccessfullyStartedUp = false;
	}
	else
	{
		SessionInterface = MakeShareable(new FOnlineSessionIOS(this));
		IdentityInterface = MakeShareable(new FOnlineIdentityIOS());
		FriendsInterface = MakeShareable(new FOnlineFriendsIOS(this));
		LeaderboardsInterface = MakeShareable(new FOnlineLeaderboardsIOS(this));
		AchievementsInterface = MakeShareable(new FOnlineAchievementsIOS(this));
		ExternalUIInterface = MakeShareable(new FOnlineExternalUIIOS());
	}

	if( IsInAppPurchasingEnabled() )
	{
		StoreInterface = MakeShareable(new FOnlineStoreInterfaceIOS());
	}

	return bSuccessfullyStartedUp;
}

bool FOnlineSubsystemIOS::Tick(float DeltaTime)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->Tick(DeltaTime);
	}
	return true;
}

bool FOnlineSubsystemIOS::Shutdown() 
{
	bool bSuccessfullyShutdown = true;
	UE_LOG(LogOnline, Display, TEXT("FOnlineSubsystemIOS::Shutdown()"));

	return bSuccessfullyShutdown;
}

FString FOnlineSubsystemIOS::GetAppId() const 
{
	return TEXT( "" );
}

bool FOnlineSubsystemIOS::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)  
{
	return false;
}

bool FOnlineSubsystemIOS::IsEnabled()
{
	bool bEnableGameCenter = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bEnableGameCenterSupport"), bEnableGameCenter, GEngineIni);
	return bEnableGameCenter;
}

bool FOnlineSubsystemIOS::IsInAppPurchasingEnabled()
{
	bool bEnableIAP = false;
	GConfig->GetBool(TEXT("OnlineSubsystemIOS.Store"), TEXT("bSupportsInAppPurchasing"), bEnableIAP, GEngineIni);
	return bEnableIAP;
}