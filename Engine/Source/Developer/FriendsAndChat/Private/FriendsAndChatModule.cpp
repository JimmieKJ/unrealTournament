// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"

/**
 * Implements the FriendsAndChat module.
 */
class FFriendsAndChatModule
	: public IFriendsAndChatModule
{
public:

	// IFriendsAndChatModule interface

	virtual TSharedRef<IFriendsAndChatManager> GetFriendsAndChatManager() override
	{
		return FFriendsAndChatManager::Get();
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
 		FFriendsAndChatManager::Get();
	}

	virtual void ShutdownModule() override
	{
		FFriendsAndChatManager::Shutdown();
	}

private:

};


IMPLEMENT_MODULE( FFriendsAndChatModule, FriendsAndChat );
