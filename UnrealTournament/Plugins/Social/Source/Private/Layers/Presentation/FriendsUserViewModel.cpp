// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendsUserViewModel.h"
#include "WebImageService.h"

#include "FriendsService.h"

class FFriendsUserViewModelImpl
	: public FFriendsUserViewModel
{
public:

	virtual ~FFriendsUserViewModelImpl() override
	{
		Uninitialize();
	}

	virtual const EOnlinePresenceState::Type GetOnlineStatus() const override
	{
		return FriendsService->GetOnlineStatus();
	}

	virtual const FString GetAppId() const override
	{
		FString AppId = FriendsService->GetUserAppId();
		return AppId;
	}

	virtual const FString GetName() const override
	{
		FString Nickname = FriendsService->GetUserNickname();
		return Nickname;
	}

	virtual const FSlateBrush* GetPresenceBrush() const override
	{
		return WebImageService->GetBrush(GetAppId());
	}

private:

	void Uninitialize()
	{
	}

	FFriendsUserViewModelImpl(const TSharedRef<FFriendsService>& InFriendsService, const TSharedRef<class FWebImageService>& InWebImageService)
	: FriendsService(InFriendsService)
	, WebImageService(InWebImageService)
	{
	}

private:
	TSharedRef<FFriendsService> FriendsService;
	TSharedRef<FWebImageService> WebImageService;

private:
	friend FFriendsUserViewModelFactory;
};

TSharedRef< FFriendsUserViewModel > FFriendsUserViewModelFactory::Create(const TSharedRef<FFriendsService>& FriendsService, const TSharedRef<class FWebImageService>& WebImageService)
{
	TSharedRef< FFriendsUserViewModelImpl > ViewModel(new FFriendsUserViewModelImpl(FriendsService, WebImageService));
	return ViewModel;
}