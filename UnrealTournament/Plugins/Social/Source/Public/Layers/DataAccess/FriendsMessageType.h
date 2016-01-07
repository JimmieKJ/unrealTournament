// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Enum for the chat message type.
 */
namespace EChatMessageType
{
	enum Type : uint16
	{
		Custom = 0,			// Empty
		Empty = 1 << 0,		// Person whisper Item
		Whisper = 1 << 1,	// Person whisper Item
		Global = 1 << 2,	// Global Chat Item
		Party = 1 << 3,		// Party Chat Item
		Game = 1 << 4,		// Game Chat Item
		Admin = 1 << 5,		// Admin messages
		Navigation = 1 << 6, // Selecting a valid chat channel
		LiveStreaming = 1 << 7, // Live streaming channel
		Team = 1 << 8,		// Team Chat
		Combined = 1 << 9,		// Combined Chat
		Invalid = 1 << 10,		// Invalid or max
	};

	/** @return a debug string matching the exact enum */
	inline FString ToDebugString(EChatMessageType::Type Type)
	{
		static FString CustomString = TEXT("custom");
		static FString EmptyString = TEXT("empty");
		static FString WhisperString = TEXT("whisper");
		static FString GlobalString = TEXT("global");
		static FString PartyString = TEXT("party");
		static FString GameString = TEXT("game");
		static FString AdminString = TEXT("admin");
		static FString LiveStreamString = TEXT("twitch");
		static FString TeamString = TEXT("Team");
		static FString InvalidString = TEXT("invalid");

		switch (Type)
		{
		case Custom: return CustomString;
		case Empty: return EmptyString;
		case Whisper: return WhisperString;
		case Global: return GlobalString;
		case Party: return PartyString;
		case Game: return GameString;
		case Admin: return AdminString;
		case LiveStreaming: return LiveStreamString;
		case Team: return TeamString;
		case Invalid: return InvalidString;

		default: return InvalidString;
		}
	}

	/** @return the FTextified version of the enum passed in */
	inline FText ToText(EChatMessageType::Type Type)
	{
		static FText WhisperText = NSLOCTEXT("FriendsList", "Whisper", "WHISPER");
		static FText GlobalText = NSLOCTEXT("FriendsList", "Global", "GLOBAL");
		static FText PartyText = NSLOCTEXT("FriendsList", "Party", "PARTY");
		static FText AdminText = NSLOCTEXT("FriendsList", "Admin", "ADMIN");
		static FText LiveStreamText = NSLOCTEXT("FriendsList", "LiveStream", "TWITCH");
		static FText TeamText = NSLOCTEXT("FriendsList", "Team", "TEAM");

		switch (Type)
		{
		case Whisper: return WhisperText;
		case Global: return GlobalText;
		case Party: return PartyText;
		case Game: return PartyText;
		case Admin: return AdminText;
		case LiveStreaming: return LiveStreamText;
		case Team: return TeamText;

		default: return FText::GetEmpty();
		}
	}

	inline FString ShortcutString(EChatMessageType::Type Type)
	{
		static FString WhisperShortcut = TEXT("/whisper");
		static FString GlobalShortcut = TEXT("/global");
		static FString PartyShortcut = TEXT("/party");
		static FString GameShortcut = TEXT("/game");
		static FString LiveStreamShortcut = TEXT("/twitch");
		static FString TeamShortcut = TEXT("/team");

		switch (Type)
		{
		case Whisper: return WhisperShortcut;
		case Global: return GlobalShortcut;
		case Party: return PartyShortcut;
		case Game: return GameShortcut;
		case LiveStreaming: return LiveStreamShortcut;
		case Team: return TeamShortcut;

		default: return FString();
		}
	}

	inline FText ShortcutParamString(EChatMessageType::Type Type)
	{
		static FText WhisperShortcut = NSLOCTEXT("FriendsList", "WhisperParam", "<player name> <message>");
		static FText GlobalShortcut = NSLOCTEXT("FriendsList", "GlobalParam", "<message>");
		static FText PartyShortcut = NSLOCTEXT("FriendsList", "PartyParam", "<message>");
		static FText GameShortcut = NSLOCTEXT("FriendsList", "GameParam", "<message>");
		static FText LiveStreamShortcut = NSLOCTEXT("FriendsList", "TwitterParam", "<message>");
		static FText TeamShortcut = NSLOCTEXT("FriendsList", "TeamParam", "<message>");

		switch (Type)
		{
		case Whisper: return WhisperShortcut;
		case Global: return GlobalShortcut;
		case Party: return PartyShortcut;
		case Game: return GameShortcut;
		case LiveStreaming: return LiveStreamShortcut;
		case Team: return TeamShortcut;

		default: return FText::GetEmpty();
		}
	}

	inline EChatMessageType::Type EnumFromString(FString ShortcutString)
	{
		static FString EmptyShortcut = TEXT("/");
		static FString WhisperShortcut = TEXT("/whisper");
		static FString GlobalShortcut = TEXT("/global");
		static FString PartyShortcut = TEXT("/party");
		static FString GameShortcut = TEXT("/game");
		static FString LiveStreamShortcut = TEXT("/twitch");
		static FString TeamShortcut = TEXT("/team");

		if (ShortcutString == WhisperShortcut)
		{
			return EChatMessageType::Whisper;
		}
		else if (ShortcutString == GlobalShortcut)
		{
			return EChatMessageType::Global;
		}
		else if (ShortcutString == PartyShortcut)
		{
			return EChatMessageType::Party;
		}
		else if (ShortcutString == GameShortcut)
		{
			return EChatMessageType::Game;
		}
		else if( ShortcutString == LiveStreamShortcut)
		{
			return EChatMessageType::LiveStreaming;
		}
		else if( TeamShortcut == EmptyShortcut)
		{
			return EChatMessageType::Team;
		}
		else if( ShortcutString == EmptyShortcut)
		{
			return EChatMessageType::Empty;
		}
		return EChatMessageType::Invalid;
	}
};

namespace EChatActionType
{
	static FText ReplyText = NSLOCTEXT("FriendsList", "ReplyText", "/reply");
	static FText NavigateToWhisperText = NSLOCTEXT("FriendsList", "NavigateToWhisperTabText", "Navigate To Whisper Tab");

	enum Type : uint8
	{
		Reply,					// Reply to last message
		NaviateToWhisper,		// Navigate to the whisper tab
		Invalid,				// Invalid action
	};

	inline FText ToText(EChatActionType::Type Type)
	{
		switch (Type)
		{
			case Reply: return ReplyText;
			case NaviateToWhisper: return NavigateToWhisperText;
			default: return FText::GetEmpty();
		}
	}

	inline FString ShortcutString(EChatActionType::Type Type)
	{
		switch (Type)
		{
			case Reply: return ReplyText.ToString();
			default: return FString();
		}
	}

	inline FText ShortcutParamString(EChatActionType::Type Type)
	{
		static FText ReplyParam = NSLOCTEXT("FriendsList", "ReplyParam", "- Reply to last whisper");

		switch (Type)
		{
		case Reply: return ReplyParam;

		default: return FText::GetEmpty();
		}
	}

	inline EChatActionType::Type EnumFromString(FString ShortcutString)
	{
		const FString ReplyShortcut = ReplyText.ToString();
		const FString NavigateToWhisperShortcut = NavigateToWhisperText.ToString();

		if (ShortcutString == ReplyShortcut)
		{
			return EChatActionType::Reply;
		}
		
		if (ShortcutString == NavigateToWhisperShortcut)
		{
			return EChatActionType::NaviateToWhisper;
		}

		return EChatActionType::Invalid;
	}
};
