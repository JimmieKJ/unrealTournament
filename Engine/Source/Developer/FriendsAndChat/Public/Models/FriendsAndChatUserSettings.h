// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Enum holding the state of the Friends manager
namespace EUserSettngsType
{
	enum Type : uint8
	{
		ShowNotifications,		// Show Notification
	};

	inline FText ToText(EUserSettngsType::Type State)
	{
		switch (State)
		{
			case ShowNotifications: return NSLOCTEXT("FriendsListOptions","ShowNotifications", "Show Notifications");
			default: return FText::GetEmpty();
		}
	}
};

// Class containing the friend message information
struct FFriendsAndChatSettings
{
	// Holds if we should show notifications.
	bool bShowNotifications;
};
