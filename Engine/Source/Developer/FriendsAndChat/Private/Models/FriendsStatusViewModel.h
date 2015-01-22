// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendsStatusViewModel
	: public TSharedFromThis<FFriendsStatusViewModel>
{
public:
	virtual ~FFriendsStatusViewModel() {}
	virtual EOnlinePresenceState::Type GetOnlineStatus() const = 0;
	virtual void SetOnlineStatus(EOnlinePresenceState::Type OnlineState) = 0;
	virtual TArray<TSharedRef<FText> > GetStatusList() const = 0;
	virtual FText GetStatusText() const = 0;
};

/**
 * Creates the implementation for an FFriendsStatusViewModel.
 *
 * @return the newly created FFriendsStatusViewModel implementation.
 */
FACTORY(TSharedRef< FFriendsStatusViewModel >, FFriendsStatusViewModel,
	const TSharedRef<class FFriendsAndChatManager>& FFriendsAndChatManager );