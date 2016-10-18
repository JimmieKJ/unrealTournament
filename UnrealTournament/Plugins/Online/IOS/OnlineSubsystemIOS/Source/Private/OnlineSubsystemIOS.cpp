// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemIOSPrivatePCH.h"
#import "OnlineStoreKitHelper.h"

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

IOnlinePartyPtr FOnlineSubsystemIOS::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemIOS::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemIOS::GetSharedCloudInterface() const
{
	return SharedCloudInterface;
}

IOnlineUserCloudPtr FOnlineSubsystemIOS::GetUserCloudInterface() const
{
	return UserCloudInterface;
}

IOnlineLeaderboardsPtr FOnlineSubsystemIOS::GetLeaderboardsInterface() const
{
	return LeaderboardsInterface;
}

IOnlineVoicePtr FOnlineSubsystemIOS::GetVoiceInterface() const
{
	return nullptr;
}

IOnlineExternalUIPtr FOnlineSubsystemIOS::GetExternalUIInterface() const
{
	return ExternalUIInterface;
}

IOnlineTimePtr FOnlineSubsystemIOS::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemIOS::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlineTitleFilePtr FOnlineSubsystemIOS::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemIOS::GetEntitlementsInterface() const
{
	return nullptr;
}

IOnlineStorePtr FOnlineSubsystemIOS::GetStoreInterface() const
{
	return StoreInterface;
}

IOnlineStoreV2Ptr FOnlineSubsystemIOS::GetStoreV2Interface() const
{
	return StoreV2Interface;
}

IOnlinePurchasePtr FOnlineSubsystemIOS::GetPurchaseInterface() const
{
	return PurchaseInterface;
}

IOnlineEventsPtr FOnlineSubsystemIOS::GetEventsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemIOS::GetAchievementsInterface() const
{
	return AchievementsInterface;
}

IOnlineSharingPtr FOnlineSubsystemIOS::GetSharingInterface() const
{
	return nullptr;
}

IOnlineUserPtr FOnlineSubsystemIOS::GetUserInterface() const
{
	return nullptr;
}

IOnlineMessagePtr FOnlineSubsystemIOS::GetMessageInterface() const
{
	return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemIOS::GetPresenceInterface() const
{
	return nullptr;
}

IOnlineChatPtr FOnlineSubsystemIOS::GetChatInterface() const
{
	return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemIOS::GetTurnBasedInterface() const
{
    return TurnBasedInterface;
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
		ExternalUIInterface = MakeShareable(new FOnlineExternalUIIOS(this));
        TurnBasedInterface = MakeShareable(new FOnlineTurnBasedIOS());
        UserCloudInterface = MakeShareable(new FOnlineUserCloudInterfaceIOS());
        SharedCloudInterface = MakeShareable(new FOnlineSharedCloudInterfaceIOS());
	}
	
	if(IsInAppPurchasingEnabled())
	{
		if (IsV2StoreEnabled())
		{
			StoreV2Interface = MakeShareable(new FOnlineStoreIOS(this));
			PurchaseInterface = MakeShareable(new FOnlinePurchaseIOS(this));
			InitStoreKitHelper();
		}
		else
		{
			StoreInterface = MakeShareable(new FOnlineStoreInterfaceIOS());
		}
	}

	return bSuccessfullyStartedUp;
}

void FOnlineSubsystemIOS::InitStoreKitHelper()
{
	StoreHelper = [[FStoreKitHelperV2 alloc] init];
	[StoreHelper retain];
	
	// Give each interface a chance to bind to the store kit helper
	StoreV2Interface->InitStoreKit(StoreHelper);
	PurchaseInterface->InitStoreKit(StoreHelper);
	
	// Bind the observer after the interfaces have bound their delegates
	[[SKPaymentQueue defaultQueue] addTransactionObserver:StoreHelper];
}

void FOnlineSubsystemIOS::CleanupStoreKitHelper()
{
	[StoreHelper release];
}

bool FOnlineSubsystemIOS::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

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

	bSuccessfullyShutdown = FOnlineSubsystemImpl::Shutdown();
	
#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
		ensure(Interface.IsUnique()); \
		Interface = nullptr; \
	}
	
	DESTRUCT_INTERFACE(SessionInterface);
	DESTRUCT_INTERFACE(IdentityInterface);
	DESTRUCT_INTERFACE(FriendsInterface);
	DESTRUCT_INTERFACE(LeaderboardsInterface);
	DESTRUCT_INTERFACE(AchievementsInterface);
	DESTRUCT_INTERFACE(ExternalUIInterface);
	DESTRUCT_INTERFACE(TurnBasedInterface);
	DESTRUCT_INTERFACE(UserCloudInterface);
	DESTRUCT_INTERFACE(SharedCloudInterface);
	DESTRUCT_INTERFACE(StoreInterface);
	DESTRUCT_INTERFACE(StoreV2Interface);
	DESTRUCT_INTERFACE(PurchaseInterface);
	
	// Cleanup after the interfaces are free
	CleanupStoreKitHelper();
	
	return bSuccessfullyShutdown;
}

FString FOnlineSubsystemIOS::GetAppId() const 
{
	return TEXT( "" );
}

bool FOnlineSubsystemIOS::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)  
{
	if (FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}
	return false;
}

bool FOnlineSubsystemIOS::IsEnabled()
{
	bool bEnableGameCenter = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bEnableGameCenterSupport"), bEnableGameCenter, GEngineIni);
    bool bEnableCloudKit = false;
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bEnableCloudKitSupport"), bEnableCloudKit, GEngineIni);
	return bEnableGameCenter || bEnableCloudKit || IsInAppPurchasingEnabled();
}

bool FOnlineSubsystemIOS::IsV2StoreEnabled()
{
	bool bUseStoreV2 = false;
	GConfig->GetBool(TEXT("OnlineSubsystemIOS.Store"), TEXT("bUseStoreV2"), bUseStoreV2, GEngineIni);
	return bUseStoreV2;
}

bool FOnlineSubsystemIOS::IsInAppPurchasingEnabled()
{
	bool bEnableIAP = false;
	GConfig->GetBool(TEXT("OnlineSubsystemIOS.Store"), TEXT("bSupportsInAppPurchasing"), bEnableIAP, GEngineIni);
	
	bool bEnableIAP1 = false;
	GConfig->GetBool(TEXT("OnlineSubsystemIOS.Store"), TEXT("bSupportInAppPurchasing"), bEnableIAP1, GEngineIni);
	return bEnableIAP || bEnableIAP1;
}
