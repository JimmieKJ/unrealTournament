// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendsUserViewModel
	: public TSharedFromThis<FFriendsUserViewModel>
{
public:
	virtual ~FFriendsUserViewModel() {}
	virtual EOnlinePresenceState::Type GetOnlineStatus() const = 0;
	virtual FString GetClientId() const = 0;
	virtual FString GetUserNickname() const = 0;
};

/**
* Creates the implementation for an FFriendsUserViewModel.
*
* @return the newly created FriendsUserViewModel implementation.
*/
FACTORY(TSharedRef< FFriendsUserViewModel >, FFriendsUserViewModel,
	const TSharedRef<class FFriendsAndChatManager>& FFriendsAndChatManager);