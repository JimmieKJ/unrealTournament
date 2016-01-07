// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FChatDisplayService
	: public IChatDisplayService
	, public TSharedFromThis<FChatDisplayService>
{
public:

	/*
	 * Notify chat entered - keep entry box visible
	 */
	virtual void ChatEntered() = 0;

	/*
	 * Notify when a message is committed so a consumer can perform some action - e.g. play a sound
	 */
	virtual void MessageCommitted() = 0;

	/*
	 * Get Visibility of the Entry Bar
	 */
	virtual EVisibility GetEntryBarVisibility() const = 0;

	/*
	 * Get Visibility of the Chat Header Bar (Tabs)
	 */
	virtual EVisibility GetChatHeaderVisibiliy() const = 0;

	/*
	 * Get the chat window visibility
	 */
	virtual EVisibility GetChatWindowVisibiliy() const = 0;

	/*
	 * Get the minimized chat window visibility
	 */
	virtual EVisibility GetChatMinimizedVisibility() const = 0;

	/*
	 * Get Visibility of the chat list
	 */
	virtual EVisibility GetChatListVisibility() const = 0;

	/*
	 * Get number of messages to display when minimized
	 */
	virtual int32 GetMinimizedChatMessageNum() const = 0;

	/*
	 * Sets if background fading is enabled
	 */
	virtual void SetFadeBackgroundEnabled(bool bEnabled) = 0;

	/*
	 * Returns True if background fading is enabled at all
	 */
	virtual bool IsFadeBackgroundEnabled() const = 0;

	/*
	 * Get Visibility of the chat list background
	 */
	virtual EVisibility GetBackgroundVisibility() const = 0;

	/*
	 * Should we hide chat chrome.
	 */
	virtual bool HideChatChrome() const = 0;

	/*
	 * Returns True if chat minimizing is enabled at all
	 */
	virtual bool IsChatMinimizeEnabled() = 0;

	/*
	 * Toggle chat minimized
	 */
	virtual void SetChatMinimized(bool bIsMinimized) = 0;

	/*
	 * Returns True if chat window is minimized currently
	 */
	virtual bool IsChatMinimized() const = 0;

	virtual bool IsAutoFaded() const = 0;

	virtual void SetTabVisibilityOverride(EVisibility TabVisibility) = 0;

	virtual void ClearTabVisibilityOverride() = 0;

	virtual void SetEntryVisibilityOverride(EVisibility EntryVisibility) = 0;

	virtual void ClearEntryVisibilityOverride() = 0;

	virtual void SetBackgroundVisibilityOverride(EVisibility BackgroundVisibility) = 0;

	virtual void ClearBackgroundVisibilityOverride() = 0;

	virtual void SetChatListVisibilityOverride(EVisibility ChatVisibility) = 0;

	virtual void ClearChatListVisibilityOverride() = 0;

	virtual void SetActiveTab(TWeakPtr<class FChatViewModel> ActiveTab) = 0;

	virtual void SendLiveStreamMessage(const FString& GameMessage) = 0;

	// Should auto release
	virtual bool ShouldAutoRelease() = 0;

	virtual ~FChatDisplayService() {}

	DECLARE_EVENT(FChatDisplayService, FChatListSetFocus)
	virtual FChatListSetFocus& OnChatListSetFocus() = 0;

	DECLARE_EVENT(FChatDisplayService, FChatListMinimized)
	virtual FChatListMinimized& OnChatMinimized() = 0;
};

/**
 * Creates the implementation for an FChatDisplayService.
 *
 * @return the newly created FChatDisplayService implementation.
 */
FACTORY(TSharedRef< FChatDisplayService >, FChatDisplayService, const TSharedRef<IChatCommunicationService>& ChatService);