// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFriendList.h"

/**
 * Class containing the friend information - used to build the list view.
 */
class FRecentPlayerList
	: public TSharedFromThis<FRecentPlayerList>
	, public IFriendList
{
};

/**
 * Creates the implementation for an FRecentPlayerList.
 *
 * @return the newly created FRecentPlayerList implementation.
 */
FACTORY(TSharedRef< FRecentPlayerList >, FRecentPlayerList);
