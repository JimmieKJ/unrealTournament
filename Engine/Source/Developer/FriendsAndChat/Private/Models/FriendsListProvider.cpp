// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "IFriendList.h"
#include "DefaultFriendList.h"
#include "RecentPlayerList.h"
#include "FriendInviteList.h"

class FFriendListFactoryFactory;

class FFriendListFactory
	: public IFriendListFactory
{
public:
	TSharedRef<IFriendList> Create(EFriendsDisplayLists::Type ListType)
	{
		switch(ListType)
		{
			case EFriendsDisplayLists::GameInviteDisplay :
			{
				return FFriendInviteListFactory::Create();
			}
			break;
			case EFriendsDisplayLists::RecentPlayersDisplay :
			{
				return FRecentPlayerListFactory::Create();
			}
			break;
		}
		return FDefaultFriendListFactory::Create(ListType);
	}

	virtual ~FFriendListFactory() {}

private:

	FFriendListFactory()
	{ }

private:

	friend FFriendListFactoryFactory;
};


TSharedRef<IFriendListFactory> FFriendListFactoryFactory::Create()
{
	TSharedRef<FFriendListFactory> Factory = MakeShareable(
		new FFriendListFactory());

	return Factory;
}