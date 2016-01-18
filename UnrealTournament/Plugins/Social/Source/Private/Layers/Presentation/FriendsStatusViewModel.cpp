// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendsStatusViewModel.h"

#include "FriendsService.h"

class FFriendsStatusViewModelImpl
	: public FFriendsStatusViewModel
{
public:
	virtual EOnlinePresenceState::Type GetOnlineStatus() const override
	{
		return FriendsService->GetOnlineStatus();
	}

	virtual void SetOnlineStatus(EOnlinePresenceState::Type OnlineState) override
	{
		FriendsService->SetUserIsOnline(OnlineState);
	}

	virtual const TArray<FOnlineState>& GetStatusList() const override
	{
		return OnlineStateArray;
	}

	virtual FText GetStatusText() const override
	{
		FString Nickname = FriendsService->GetUserNickname();
		return FText::FromString(Nickname);
	}

private:
	FFriendsStatusViewModelImpl(const TSharedRef<FFriendsService>& InFriendsService)
	: FriendsService(InFriendsService)
	{
		OnlineStateArray.Add(FOnlineState(true, NSLOCTEXT("OnlineState", "OnlineState_Online", "Online"), EOnlinePresenceState::Online));
		OnlineStateArray.Add(FOnlineState(false, NSLOCTEXT("OnlineState", "OnlineState_Offline", "Offline"), EOnlinePresenceState::Offline));
		OnlineStateArray.Add(FOnlineState(true, NSLOCTEXT("OnlineState", "OnlineState_Away", "Away"), EOnlinePresenceState::Away));
		OnlineStateArray.Add(FOnlineState(false, NSLOCTEXT("OnlineState", "OnlineState_Away", "Away"), EOnlinePresenceState::ExtendedAway));
		OnlineStateArray.Add(FOnlineState(false, NSLOCTEXT("OnlineState", "OnlineState_Online", "Online"), EOnlinePresenceState::DoNotDisturb));
		OnlineStateArray.Add(FOnlineState(false, NSLOCTEXT("OnlineState", "OnlineState_Online", "Online"), EOnlinePresenceState::Chat));
	}

private:
	TSharedRef<FFriendsService> FriendsService;
	TArray<FOnlineState> OnlineStateArray;

private:
	friend FFriendsStatusViewModelFactory;
};

TSharedRef< FFriendsStatusViewModel > FFriendsStatusViewModelFactory::Create(const TSharedRef<FFriendsService>& FriendsService)
{
	TSharedRef< FFriendsStatusViewModelImpl > ViewModel(new FFriendsStatusViewModelImpl(FriendsService));
	return ViewModel;
}