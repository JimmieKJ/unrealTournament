// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendsStatusViewModel.h"

class FFriendsStatusViewModelImpl
	: public FFriendsStatusViewModel
{
public:
	virtual EOnlinePresenceState::Type GetOnlineStatus() const override
	{
		return FriendsAndChatManager.Pin()->GetOnlineStatus();
	}

	virtual void SetOnlineStatus(EOnlinePresenceState::Type OnlineState) override
	{
		FriendsAndChatManager.Pin()->SetUserIsOnline(OnlineState);
	}

	virtual const TArray<FOnlineState>& GetStatusList() const override
	{
		return OnlineStateArray;
	}

	virtual FText GetStatusText() const override
	{
		FString Nickname;
		TSharedPtr<FFriendsAndChatManager> ManagerPinned = FriendsAndChatManager.Pin();
		if (ManagerPinned.IsValid())
		{
			Nickname = ManagerPinned->GetUserNickname();
		}
		return FText::FromString(Nickname);
	}

private:
	FFriendsStatusViewModelImpl(
		const TSharedRef<FFriendsAndChatManager>& InFriendsAndChatManager
		)
		: FriendsAndChatManager(InFriendsAndChatManager)
	{
		OnlineStateArray.Add(FOnlineState(true, NSLOCTEXT("OnlineState", "OnlineState_Online", "Online"), EOnlinePresenceState::Online));
		OnlineStateArray.Add(FOnlineState(false, NSLOCTEXT("OnlineState", "OnlineState_Offline", "Offline"), EOnlinePresenceState::Offline));
		OnlineStateArray.Add(FOnlineState(true, NSLOCTEXT("OnlineState", "OnlineState_Away", "Away"), EOnlinePresenceState::Away));
		OnlineStateArray.Add(FOnlineState(false, NSLOCTEXT("OnlineState", "OnlineState_Away", "Away"), EOnlinePresenceState::ExtendedAway));
		OnlineStateArray.Add(FOnlineState(false, NSLOCTEXT("OnlineState", "OnlineState_Online", "Online"), EOnlinePresenceState::DoNotDisturb));
		OnlineStateArray.Add(FOnlineState(false, NSLOCTEXT("OnlineState", "OnlineState_Online", "Online"), EOnlinePresenceState::Chat));
	}

private:
	TWeakPtr<FFriendsAndChatManager> FriendsAndChatManager;
	TArray<FOnlineState> OnlineStateArray;

private:
	friend FFriendsStatusViewModelFactory;
};

TSharedRef< FFriendsStatusViewModel > FFriendsStatusViewModelFactory::Create(
	const TSharedRef<FFriendsAndChatManager>& FriendsAndChatManager
	)
{
	TSharedRef< FFriendsStatusViewModelImpl > ViewModel(new FFriendsStatusViewModelImpl(FriendsAndChatManager));
	return ViewModel;
}