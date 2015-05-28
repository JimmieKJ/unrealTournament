// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFriendList.h"

/**
 * Class containing the friend information - used to build the list view.
 */
class FDefaultFriendList
	: public TSharedFromThis<FDefaultFriendList>
	, public IFriendList
{
};

/**
 * Creates the implementation for an FDefaultFriendList.
 *
 * @return the newly created FDefaultFriendList implementation.
 */
FACTORY(TSharedRef< FDefaultFriendList >, FDefaultFriendList,
	EFriendsDisplayLists::Type ListType);
