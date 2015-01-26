// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendsStatusViewModel.h"

class FFriendsStatusViewModelImpl
	: public FFriendsStatusViewModel
{
public:
	virtual EOnlinePresenceState::Type GetOnlineStatus() const override
	{
		return FriendsAndChatManager.Pin()->GetUserIsOnline();
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
		EOnlinePresenceState::Type CurrentOnlineStatus = FriendsAndChatManager.Pin()->GetUserIsOnline();

		const FOnlineState* FoundStatePtr = OnlineStateArray.FindByPredicate([CurrentOnlineStatus](const FOnlineState& InOnlineState) -> bool
		{
			return InOnlineState.State == CurrentOnlineStatus;
		});

		if (FoundStatePtr != nullptr)
		{
			return FoundStatePtr->DisplayText;
		}
		return NSLOCTEXT("OnlineState", "OnlineState_Unknown", "Unknown");
	}

private:
	FFriendsStatusViewModelImpl(
		const TSharedRef<FFriendsAndChatManager>& FriendsAndChatManager
		)
		: FriendsAndChatManager(FriendsAndChatManager)
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