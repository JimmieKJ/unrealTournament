// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace EChatMessageType
{
	enum Type : uint16;
}

class FFriendsNavigationService
	: public TSharedFromThis<FFriendsNavigationService>
	, public IFriendsNavigationService
{
public:

	virtual ~FFriendsNavigationService() {}

	virtual void ChangeViewChannel(EChatMessageType::Type ChannelSelected) = 0;
	virtual void ChangeChatChannel(EChatMessageType::Type ChannelSelected) = 0;
	virtual void CycleChatChannel() = 0;
	virtual bool IsInGame() const = 0;
};

FACTORY(TSharedRef<FFriendsNavigationService>, FFriendsNavigationService, const TSharedRef<class FFriendsService>& FriendsService, bool InGame);
