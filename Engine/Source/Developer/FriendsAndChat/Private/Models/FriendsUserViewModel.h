// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IUserInfo.h"

class FFriendsUserViewModel
	: public TSharedFromThis<FFriendsUserViewModel>
	, public IUserInfo
{
public:
	virtual ~FFriendsUserViewModel() {}
};

/**
* Creates the implementation for an FFriendsUserViewModel.
*
* @return the newly created FriendsUserViewModel implementation.
*/
FACTORY(TSharedRef< FFriendsUserViewModel >, FFriendsUserViewModel,
	const TSharedRef<class FFriendsAndChatManager>& FFriendsAndChatManager);