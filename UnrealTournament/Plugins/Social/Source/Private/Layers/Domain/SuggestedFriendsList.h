// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFriendList.h"

/**
 * Class containing the friend information - used to build the list view.
 */
class FSuggestedFriendsList
	: public TSharedFromThis<FSuggestedFriendsList>
	, public IFriendList
{
};

/**
 * Creates the implementation for an FSuggestedFriendsList.
 *
 * @return the newly created FSuggestedFriendsList implementation.
 */
FACTORY(TSharedRef< FSuggestedFriendsList >, FSuggestedFriendsList,
	const TSharedRef<class IFriendViewModelFactory>& FriendViewModelFactory,
	const TSharedRef<class FFriendsService>& FriendsService);
