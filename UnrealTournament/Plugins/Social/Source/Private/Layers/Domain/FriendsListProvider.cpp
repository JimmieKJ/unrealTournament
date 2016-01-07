// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "IFriendList.h"
#include "DefaultFriendList.h"
#include "RecentPlayerList.h"
#include "SuggestedFriendsList.h"
#include "GameInviteList.h"
#include "ClanMemberList.h"
#include "BlockedPlayerList.h"

class FFriendListFactoryFactory;

class FFriendListFactory
	: public IFriendListFactory
{
public:
	virtual TSharedRef<IFriendList> Create(EFriendsDisplayLists::Type ListType) override
	{
		switch(ListType)
		{
			case EFriendsDisplayLists::GameInviteDisplay :
			{
				return FGameInviteListFactory::Create(FriendViewModelFactory, GameAndPartyService.Pin().ToSharedRef());
			}
			break;
			case EFriendsDisplayLists::RecentPlayersDisplay :
			{
				return FRecentPlayerListFactory::Create(FriendViewModelFactory, FriendsService.Pin().ToSharedRef());
			}
			break;
			case EFriendsDisplayLists::SuggestedFriendsDisplay:
			{
				return FSuggestedFriendsListFactory::Create(FriendViewModelFactory, FriendsService.Pin().ToSharedRef());
			}
			break;
			case EFriendsDisplayLists::BlockedPlayersDisplay :
			{
				return FBlockedPlayerListFactory::Create(FriendViewModelFactory, FriendsService.Pin().ToSharedRef());
			}
			break;
		}
		return FDefaultFriendListFactory::Create(ListType, FriendViewModelFactory, FriendsService.Pin().ToSharedRef(), SettingsService.Pin().ToSharedRef());
	}

	virtual TSharedRef<IFriendList> Create(TSharedRef<IClanInfo> ClanInfo) override
	{
		return FClanMemberListFactory::Create(ClanInfo, FriendViewModelFactory);
	}

	virtual ~FFriendListFactory() {}

private:

	FFriendListFactory(
	const TSharedRef<IFriendViewModelFactory>& InFriendViewModelFactory,
	const TSharedRef<FFriendsService>& InFriendsService,
	const TSharedRef<FGameAndPartyService>& InGameAndPartyService,
	const TSharedRef<FChatSettingsService>& InSettingsService)
		: FriendViewModelFactory(InFriendViewModelFactory)
		, FriendsService(InFriendsService)
		, GameAndPartyService(InGameAndPartyService)
		, SettingsService(InSettingsService)
	{ }

private:

	const TSharedRef<IFriendViewModelFactory> FriendViewModelFactory;
	TWeakPtr<FFriendsService> FriendsService;
	TWeakPtr<FGameAndPartyService> GameAndPartyService;
	TWeakPtr<FChatSettingsService> SettingsService;
	friend FFriendListFactoryFactory;
};

TSharedRef<IFriendListFactory> FFriendListFactoryFactory::Create(
	const TSharedRef<IFriendViewModelFactory>& FriendViewModelFactory,
	const TSharedRef<FFriendsService>& FriendsService,
	const TSharedRef<FGameAndPartyService>& GameAndPartyService,
	const TSharedRef<FChatSettingsService>& SettingsService)
{
	TSharedRef<FFriendListFactory> Factory = MakeShareable(
		new FFriendListFactory(FriendViewModelFactory, FriendsService, GameAndPartyService, SettingsService));

	return Factory;
}