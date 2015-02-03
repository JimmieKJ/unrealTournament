// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendsUserViewModel.h"

class FFriendsUserViewModelImpl
	: public FFriendsUserViewModel
{
public:

	virtual ~FFriendsUserViewModelImpl() override
	{
		Uninitialize();
	}

	virtual EOnlinePresenceState::Type GetOnlineStatus() const override
	{
		TSharedPtr<FFriendsAndChatManager> ManagerPinned = FriendsAndChatManager.Pin();
		if (ManagerPinned.IsValid())
		{
			return ManagerPinned->GetUserIsOnline();
		}
		return EOnlinePresenceState::Offline;
	}

	virtual FString GetClientId() const override
	{
		FString ClientId;
		TSharedPtr<FFriendsAndChatManager> ManagerPinned = FriendsAndChatManager.Pin();
		if (ManagerPinned.IsValid())
		{
			ClientId = ManagerPinned->GetUserClientId();
		}
		return ClientId;
	}

	virtual FString GetUserNickname() const override
	{
		FString Nickname;
		TSharedPtr<FFriendsAndChatManager> ManagerPinned = FriendsAndChatManager.Pin();
		if (ManagerPinned.IsValid())
		{
			Nickname = ManagerPinned->GetUserNickname();
		}
		return Nickname;
	}

private:

	void Uninitialize()
	{
		if (FriendsAndChatManager.IsValid())
		{
			FriendsAndChatManager.Reset();
		}
	}

	FFriendsUserViewModelImpl(
		const TSharedRef<FFriendsAndChatManager>& FriendsAndChatManager
		)
		: FriendsAndChatManager(FriendsAndChatManager)
	{
	}

private:
	TWeakPtr<FFriendsAndChatManager> FriendsAndChatManager;

private:
	friend FFriendsUserViewModelFactory;
};

TSharedRef< FFriendsUserViewModel > FFriendsUserViewModelFactory::Create(const TSharedRef<FFriendsAndChatManager>& FriendsAndChatManager)
{
	TSharedRef< FFriendsUserViewModelImpl > ViewModel(new FFriendsUserViewModelImpl(FriendsAndChatManager));
	return ViewModel;
}