// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IChatViewModel
{
public:

	// IChatViewModel Interface
	virtual void SetFocus() = 0;
	virtual void SetEntryBarVisibility(EVisibility Visibility) = 0;
	virtual EVisibility GetEntryBarVisibility() const = 0;
	virtual void SetFontOverrideColor(FSlateColor OverrideColor) = 0;
	virtual void SetOverrideColorActive(bool bSet) = 0;
	virtual bool GetOverrideColorSet() = 0;
	virtual FSlateColor GetFontOverrideColor() const = 0;
	virtual void SetInGameUI(bool bInGame) = 0;
	virtual void EnableGlobalChat(bool bEnable) = 0;
	virtual float GetChatListFadeValue() const = 0;
	virtual bool IsGlobalChatEnabled() const = 0;

	DECLARE_EVENT(IChatViewModel, FOnFriendsChatMessageCommitted)
	virtual FOnFriendsChatMessageCommitted& OnChatMessageCommitted() = 0;

	DECLARE_EVENT_OneParam(IChatViewModel, FOnFriendsSendNetworkMessageEvent, /*struct*/ const FString& /* the message */)
	virtual FOnFriendsSendNetworkMessageEvent& OnNetworkMessageSentEvent() = 0;

	DECLARE_EVENT(IChatViewModel, FChatListUpdated)
	virtual FChatListUpdated& OnChatListUpdated() = 0;
};
