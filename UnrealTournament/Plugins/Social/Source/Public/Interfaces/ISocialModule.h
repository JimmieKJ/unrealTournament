// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "IFriendsAndChatManager.h"

DECLARE_DELEGATE_RetVal(bool, FFriendsSystemReady )


/** Log categories */
DECLARE_LOG_CATEGORY_EXTERN(LogSocial, Display, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSocialChat, Display, All);

/**
 * Interface for the Friends and chat manager.
 */
class ISocialModule
	: public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline ISocialModule& Get()
	{
		return FModuleManager::LoadModuleChecked<ISocialModule>("Social");
	}

	virtual TSharedRef<IFriendsAndChatManager> GetFriendsAndChatManager(FName OSSInstanceName = TEXT(""), bool InGame = false) = 0;

public:

	/** Virtual destructor. */
	virtual ~ISocialModule() { }
};
