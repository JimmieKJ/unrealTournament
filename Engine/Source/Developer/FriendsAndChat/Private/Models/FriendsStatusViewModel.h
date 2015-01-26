// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendsStatusViewModel
	: public TSharedFromThis<FFriendsStatusViewModel>
{
public:
	/** Struct that maps EOnlinePresenceStates to loc strings for display */
	struct FOnlineState
	{
		/** Flag that determines which EOnlinePresenceStates are shown in the UI for the user to select */
		bool bIsDisplayed;

		/** The localized diaply text for this EOnlinePresenceStates */
		FText DisplayText;

		/** The state the which this struct refers */
		EOnlinePresenceState::Type State;

		FOnlineState(bool InIsDisplayed, FText InDisplayText, EOnlinePresenceState::Type InState)
			: bIsDisplayed(InIsDisplayed)
			, DisplayText(InDisplayText)
			, State(InState)
		{}
	};

	virtual ~FFriendsStatusViewModel() {}
	virtual EOnlinePresenceState::Type GetOnlineStatus() const = 0;
	virtual void SetOnlineStatus(EOnlinePresenceState::Type OnlineState) = 0;
	virtual const TArray<FOnlineState>& GetStatusList() const = 0;
	virtual FText GetStatusText() const = 0;
};

/**
 * Creates the implementation for an FFriendsStatusViewModel.
 *
 * @return the newly created FFriendsStatusViewModel implementation.
 */
FACTORY(TSharedRef< FFriendsStatusViewModel >, FFriendsStatusViewModel,
	const TSharedRef<class FFriendsAndChatManager>& FFriendsAndChatManager );