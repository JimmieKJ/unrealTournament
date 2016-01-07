// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Interface for the message communication service
 */
class IChatCommunicationService
{
public:

	virtual bool SendRoomMessage(const FString& RoomName, const FString& MsgBody) = 0;
	virtual bool SendPrivateMessage(TSharedPtr<IFriendItem> Friend, const FText MessageText) = 0;
	virtual void SendGameMessage(const FString& MsgBody) = 0;
	virtual void InsertNetworkMessage(const FString& MsgBody) = 0;
	virtual void InsertAdminMessage(const FString& MsgBody) = 0;
	virtual void InsertLiveStreamMessage(const FString& MsgBody) = 0;
	virtual void EnableChatThrottle(int32 TicketCount, float TicketInterval) = 0;
	virtual void DisableChatThrottle() = 0;

	DECLARE_EVENT_OneParam(IChatCommunicationService, FOnChatMessageAddedEvent, const TSharedRef<struct FFriendChatMessage> /*The chat message*/)
	virtual FOnChatMessageAddedEvent& OnChatMessageAdded() = 0;

	DECLARE_EVENT_TwoParams(IChatCommunicationService, FOnChatMessageReceivedEvent, EChatMessageType::Type /*Type of message received*/, TSharedPtr<IFriendItem> /*Friend if chat type is whisper*/);
	virtual FOnChatMessageReceivedEvent& OnChatMessageRecieved() = 0;

	DECLARE_EVENT_OneParam(IChatCommunicationService, FOnFriendsSendNetworkMessageEvent, const FString& /* the message */)
	virtual FOnFriendsSendNetworkMessageEvent& OnNetworkMessageSentEvent() = 0;
};
