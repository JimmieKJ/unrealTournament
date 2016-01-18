// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
/*=============================================================================
	IChatDisplayService.h: Interface for Chat display services.
	The chat display service is used to control a group of chat windows display.
	e.g. Setting chat entry bar to be hidden, and register to listen for list refreshes
=============================================================================*/

#pragma once

namespace EChatOrientationType
{
	enum Type
	{
		COT_Horizontal,
		COT_Vertical,
	};
};

class IChatDisplayService
{
public:

	/*
	 * Are we able to minimize chat (shows minimize button)
	 */
	virtual void SetMinimizeEnabled(bool InMinimizeEnabled) = 0;

	/*
	 * Set time before chat list fades out
	 */
	virtual void SetListFadeTime(float InListFadeTime) = 0;

	/*
	 * Set number minimized messages to display
	 */
	virtual void SetMinimizedChatMessageNum(int32 NumberOfChatMessages) = 0;

	/*
	 * Set if we should hide the chat chrome - used for in-game etc.
	 */
	virtual void SetHideChrome(bool bInHideChrome) = 0;

	/*
	 * Set the chat orientation
	 */
	virtual void SetChatOrientation(EChatOrientationType::Type InChatOrientation) = 0;

	/*
	 * Get the chat orientation
	 */
	virtual EChatOrientationType::Type GetChatOrientation() = 0;

	/*
	 * Set the widget to have focus ready for chat input
	 */
	virtual void SetFocus() = 0;

	/*
	 * Set if this widget is active - visible and receiving input
	 */
	virtual void SetActive(bool bIsActive) = 0;

	/*
	 * Is this chat widget active
	 */
	virtual bool IsActive() const = 0;

	// Set if we should release focus after a message is sent
	virtual void SetAutoReleaseFocus(bool AutoRelease) = 0;

	DECLARE_EVENT(IChatDisplayService, FOnFriendsChatMessageCommitted)
	virtual FOnFriendsChatMessageCommitted& OnChatMessageCommitted() = 0;

	DECLARE_EVENT_OneParam(IChatDisplayService, FOnSendLiveSteamMessageEvent, /*struct*/ const FString& /* the message */)
	virtual FOnSendLiveSteamMessageEvent& OnLiveStreamMessageSentEvent() = 0;

	DECLARE_EVENT(IChatDisplayService, FOnFocusReleasedEvent)
	virtual FOnFocusReleasedEvent& OnFocuseReleasedEvent() = 0;

	DECLARE_EVENT(IChatDisplayService, FChatListUpdated)
	virtual FChatListUpdated& OnChatListUpdated() = 0;
};
