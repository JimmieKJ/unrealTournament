// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Enum for the chat message type.
 */
namespace EChatMessageType
{
	enum Type : uint8
	{
		// Person whisper Item
		Whisper,
		// Party Chat Item
		Party,
		// Global Chat Item
		Global,
	};

	/** @return the FTextified version of the enum passed in */
	inline FText ToText(EChatMessageType::Type Type)
	{
		static FText GlobalText = NSLOCTEXT("FriendsList", "Global", "Global");
		static FText WhisperText = NSLOCTEXT("FriendsList", "Whisper", "Whisper");
		static FText PartyText = NSLOCTEXT("FriendsList", "Party", "Party");

		switch (Type)
		{
			case Global: return GlobalText;
			case Whisper: return WhisperText;
			case Party: return PartyText;

			default: return FText::GetEmpty();
		}
	}
};

// Struct for holding chat message information. Will probably be replaced by OSS version
struct FFriendChatMessage
{
	EChatMessageType::Type MessageType;
	FText FromName;
	FText ToName;
	FText Message;
	FDateTime MessageTime;
	FDateTime ExpireTime;
	TSharedPtr<class FChatMessage> MessageRef;
	TSharedPtr<FUniqueNetId> SenderId;
	TSharedPtr<FUniqueNetId> RecipientId;
	bool bIsFromSelf;
};

/**
 * Implement the Friend and Chat manager
 */
class FFriendsMessageManager
	: public TSharedFromThis<FFriendsMessageManager>
{
public:

	/** Destructor. */
	virtual ~FFriendsMessageManager( ) {};

	virtual void LogIn() = 0;
	virtual void LogOut() = 0;
	virtual const TArray<TSharedRef<FFriendChatMessage> >& GetMessages() const = 0;
	virtual void JoinPublicRoom(const FString& RoomName) = 0;
	virtual bool SendRoomMessage(const FString& RoomName, const FString& MsgBody) = 0;
	virtual bool SendPrivateMessage(TSharedPtr<FUniqueNetId> UserID, const FText MessageText) = 0;
	virtual void InsertNetworkMessage(const FString& MsgBody) = 0;

	DECLARE_EVENT_OneParam(FFriendsMessageManager, FOnChatMessageReceivedEvent, const TSharedRef<FFriendChatMessage> /*The chat message*/)
	virtual FOnChatMessageReceivedEvent& OnChatMessageRecieved() = 0;

	DECLARE_EVENT_OneParam(FFriendsMessageManager, FOnChatPublicRoomJoinedEvent, const FString& /*RoomName*/)
	virtual FOnChatPublicRoomJoinedEvent& OnChatPublicRoomJoined() = 0;

	DECLARE_EVENT_OneParam(FFriendsMessageManager, FOnChatPublicRoomExitedEvent, const FString& /*RoomName*/)
	virtual FOnChatPublicRoomExitedEvent& OnChatPublicRoomExited() = 0;
};

/**
 * Creates the implementation of a chat manager.
 *
 * @return the newly created FriendViewModel implementation.
 */
FACTORY(TSharedRef< FFriendsMessageManager >, FFriendsMessageManager);
