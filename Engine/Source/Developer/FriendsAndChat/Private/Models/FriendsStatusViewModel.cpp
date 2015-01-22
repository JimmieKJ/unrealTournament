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

	virtual TArray<TSharedRef<FText> > GetStatusList() const override
	{
		return OnlineStateArray;
	}

	virtual FText GetStatusText() const override
	{
		//@todo - use loc text
		EOnlinePresenceState::Type OnlineStatus = FriendsAndChatManager.Pin()->GetUserIsOnline();
		switch (OnlineStatus)
		{
			case EOnlinePresenceState::Away:
			case EOnlinePresenceState::ExtendedAway:
				return FText::FromString(TEXT("Away"));
			case EOnlinePresenceState::Chat:
			case EOnlinePresenceState::DoNotDisturb:
			case EOnlinePresenceState::Online:
				return FText::FromString(TEXT("Online"));
			case EOnlinePresenceState::Offline:
			default:
				return FText::FromString("Offline");
		};
	}

private:
	FFriendsStatusViewModelImpl(
		const TSharedRef<FFriendsAndChatManager>& FriendsAndChatManager
		)
		: FriendsAndChatManager(FriendsAndChatManager)
	{
		OnlineStateArray.Add(MakeShareable(new FText(NSLOCTEXT("OnlineState", "OnlineState_Online", "Online"))));
		OnlineStateArray.Add(MakeShareable(new FText(NSLOCTEXT("OnlineState", "OnlineState_Away", "Away"))));
	}

private:
	TWeakPtr<FFriendsAndChatManager> FriendsAndChatManager;
	TArray<TSharedRef<FText> > OnlineStateArray;

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