// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemAmazonModule.h"
#include "OnlineSubsystemAmazonPackage.h"

/** Forward declarations of all interface classes */
typedef TSharedPtr<class FOnlineIdentityAmazon, ESPMode::ThreadSafe> FOnlineIdentityAmazonPtr;

/**
 * Amazon subsystem
 */
class ONLINESUBSYSTEMAMAZON_API FOnlineSubsystemAmazon :
	public FOnlineSubsystemImpl

{
	class FOnlineFactoryAmazon* AmazonFactory;

	/** Interface to the identity registration/auth services */
	FOnlineIdentityAmazonPtr IdentityInterface;

	/** Used to toggle between 1 and 0 */
	int TickToggle;

PACKAGE_SCOPE:

	/** Only the factory makes instances */
	FOnlineSubsystemAmazon();

public:
	// IOnlineSubsystem

	virtual IOnlineSessionPtr GetSessionInterface() const override
	{
		return NULL;
	}
	virtual IOnlineFriendsPtr GetFriendsInterface() const override
	{
		return NULL;
	}
	virtual IOnlinePartyPtr GetPartyInterface() const override
	{
		return NULL;
	}
	virtual IOnlineGroupsPtr GetGroupsInterface() const override
	{
		return nullptr;
	}
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override
	{
		return NULL;
	}
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const override
	{
		return NULL;
	}
	virtual IOnlineUserCloudPtr GetUserCloudInterface(const FString& Key) const override
	{
		return NULL;
	}
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override
	{
		return NULL;
	}
	virtual IOnlineVoicePtr GetVoiceInterface() const override
	{
		return NULL;
	}
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override	
	{
		return NULL;
	}
	virtual IOnlineTimePtr GetTimeInterface() const override
	{
		return NULL;
	}
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const override
	{
		return NULL;
	}

	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override
	{
		return NULL;
	}

	virtual IOnlineIdentityPtr GetIdentityInterface() const override;

	virtual IOnlineStorePtr GetStoreInterface() const override
	{
		return NULL;
	}

	virtual IOnlineEventsPtr GetEventsInterface() const override
	{
		return NULL;
	}

	virtual IOnlineAchievementsPtr GetAchievementsInterface() const override
	{
		return NULL;
	}

	virtual IOnlineSharingPtr GetSharingInterface() const override
	{
		return NULL;
	}

	virtual IOnlineUserPtr GetUserInterface() const override
	{
		return NULL;
	}

	virtual IOnlineMessagePtr GetMessageInterface() const override
	{
		return NULL;
	}

	virtual IOnlinePresencePtr GetPresenceInterface() const override
	{
		return NULL;
	}

	virtual IOnlineChatPtr GetChatInterface() const override
	{
		return NULL;
	}

    virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override
    {
        return NULL;
    }
	
	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	// FTickerBaseObject

	virtual bool Tick(float DeltaTime) override;

	// FOnlineSubsystemAmazon

	/**
	 * Destructor
	 */
	virtual ~FOnlineSubsystemAmazon();

	/**
	 * @return whether this subsystem is enabled or not
	 */
	bool IsEnabled();
};

typedef TSharedPtr<FOnlineSubsystemAmazon, ESPMode::ThreadSafe> FOnlineSubsystemAmazonPtr;