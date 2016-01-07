// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "OnlineSubsystemNames.h"
/**
 * Log categories for social module
 */

DEFINE_LOG_CATEGORY(LogSocial);
DEFINE_LOG_CATEGORY(LogSocialChat);

/**
 * Implements the FriendsAndChat module.
 */
class FSocialModule
	: public ISocialModule
{
public:

	// ISocialModule interface

	virtual TSharedRef<IFriendsAndChatManager> GetFriendsAndChatManager(FName MCPInstanceName,  bool InGame) override
	{
		FFriendsAndChatOptions InitOptions;
		InitOptions.Game = InGame;
		// TODO:  Handle other platforms?
		InitOptions.ExternalOnlineSub = IOnlineSubsystem::GetByPlatform();
		if(MCPInstanceName == TEXT(""))
		{
			if (!DefaultManager.IsValid())
			{
				DefaultManager = MakeShareable(new FFriendsAndChatManager());
				DefaultManager->Initialize(InitOptions);
			}
			return DefaultManager.ToSharedRef();
		}
		else
		{
			TSharedRef<FFriendsAndChatManager>* FoundManager = ManagerMap.Find(MCPInstanceName);
			if(FoundManager != nullptr)
			{
				return *FoundManager;
			}
		}

		TSharedRef<FFriendsAndChatManager> NewManager = MakeShareable(new FFriendsAndChatManager());
		NewManager->Initialize(InitOptions);
		ManagerMap.Add(MCPInstanceName, NewManager);
		return NewManager;
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
		if(DefaultManager.IsValid())
		{
			DefaultManager.Reset();
		}
		ManagerMap.Reset();
	}

private:

	TSharedPtr<FFriendsAndChatManager> DefaultManager;
	TMap<FName, TSharedRef<FFriendsAndChatManager>> ManagerMap;
};


IMPLEMENT_MODULE(FSocialModule, Social );
