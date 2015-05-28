// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IUserInfo.h"

enum class EFriendActionLevel
{
	Action,			// A normal action
	Emphasis,		// An important action to be shown with emphasis
	Critical,		// A critical action
};

// Enum holding the state of the Friends manager
namespace EFriendActionType
{
	enum Type
	{
		AcceptFriendRequest,		// Accept an incoming Friend request
		IgnoreFriendRequest,		// Ignore an incoming Friend request
		RejectFriendRequest,		// Reject an incoming Friend request
		CancelFriendRequest,		// Cancel a friend request
		BlockFriend,				// Block a friend
		RemoveFriend,				// Remove a friend
		JoinGame,					// Join a Friends game
		RejectGame,					// Reject a game request
		InviteToGame,				// Invite to game
		Whisper,					// Open Whisper window
		SendFriendRequest,			// Send a friend request
		Updating,					// Updating;
		Chat,						// Chat
		MAX_None,
	};

	inline FText ToText(EFriendActionType::Type State)
	{
		switch (State)
		{
			case AcceptFriendRequest: return NSLOCTEXT("FriendsList","AcceptFriendRequest", "Accept");
			case IgnoreFriendRequest: return NSLOCTEXT("FriendsList","IgnoreFriendRequest", "Ignore");
			case RejectFriendRequest: return NSLOCTEXT("FriendsList","RejectFriendRequest", "Reject");
			case CancelFriendRequest: return NSLOCTEXT("FriendsList","CancelRequest", "Cancel");
			case BlockFriend: return NSLOCTEXT("FriendsList","BlockFriend", "Block");
			case RemoveFriend: return NSLOCTEXT("FriendsList","RemoveFriend", "Remove");
			case JoinGame: return NSLOCTEXT("FriendsList","JoingGame", "Join Game");
			case RejectGame: return NSLOCTEXT("FriendsList","RejectGame", "Reject");
			case InviteToGame: return NSLOCTEXT("FriendsList","Invite to game", "Invite to Game");
			case SendFriendRequest: return NSLOCTEXT("FriendsList","SendFriendRequst", "Send Friend Request");
			case Updating: return NSLOCTEXT("FriendsList","Updating", "Updating");
			case Chat: return NSLOCTEXT("FriendsList", "Chat", "Chat");
			case Whisper: return NSLOCTEXT("FriendsList", "Whisper", "Whisper");

			default: return FText::GetEmpty();
		}
	}

	inline EFriendActionLevel ToActionLevel(EFriendActionType::Type State)
	{
		switch (State)
		{
			case AcceptFriendRequest:
			case JoinGame:
			case SendFriendRequest:
				return EFriendActionLevel::Emphasis;
			case BlockFriend:
			case RemoveFriend:
				return EFriendActionLevel::Critical;
			default:
				return EFriendActionLevel::Action;
		}
	}
};

class FFriendViewModel
	: public TSharedFromThis<FFriendViewModel>
	, public IUserInfo
{
public:
	virtual ~FFriendViewModel() {}
	virtual void EnumerateActions(TArray<EFriendActionType::Type>& Actions, bool bFromChat = false) = 0;
	virtual const bool HasChatAction() const = 0;
	virtual void PerformAction(const EFriendActionType::Type ActionType) = 0;
	virtual bool CanPerformAction(const EFriendActionType::Type ActionType) = 0;
	virtual FText GetJoinGameDisallowReason() const = 0;
	virtual FText GetFriendLocation() const = 0;
	virtual bool IsOnline() const = 0;
	virtual bool IsInGameSession() const = 0;
};

/**
 * Creates the implementation for an FriendViewModel.
 *
 * @return the newly created FriendViewModel implementation.
 */
FACTORY(TSharedRef< FFriendViewModel >, FFriendViewModel,
	const TSharedRef<class IFriendItem>& IFriendItem);