// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemTypes.h"
#include "OnlineChatInterface.h"

/**
 * Key value data that can be sent to party members
 */
typedef FOnlineKeyValuePairs<FString, FVariantData> FOnlinePartyAttrs;

/**
 * Party member user info returned by IOnlineParty interface
 */
class FOnlinePartyMember : public FOnlineUser
{
public:
	
	/**
	 * @return the current invite status of a friend wrt to user that queried
	 */
	virtual EInviteStatus::Type GetInviteStatus() const = 0;
	
	/**
	 * @return presence info for an online friend
	 */
	virtual const class FOnlineUserPresence& GetPresence() const = 0;

	/**
	 * @return true if this is the party owner/leader
	 */
	virtual bool IsLeader() const = 0;
};

/**
 * Data associated with the entire party
 */
class IOnlinePartyData
{
public:

	/**
	 * @return id of the party this data is associated with
	 */
	virtual TSharedRef<FOnlinePartyId> GetPartyId() const = 0;

	/**
	 * Get an attribute from the party data
	 *
	 * @param AttrName - key for the attribute
	 * @param OutAttrValue - [out] value for the attribute if found
	 *
	 * @return true if the attribute was vound
	 */
	virtual bool GetAttribute(const FString& AttrName, FVariantData& OutAttrValue) const = 0;

	/**
	 * Get all attributes from the party data
	 *
	 * @return key/val attributes
	 */
	virtual const FOnlinePartyAttrs& GetAttributes() const = 0;
};

/**
 * Data associated with a single party member
 */
class IOnlinePartyMemberData : public IOnlinePartyData
{
public:

	/**
	 * @return id of the party member this data is associated with
	 */
	virtual TSharedRef<FUniqueNetId> GetPartyMemberId() const = 0;
};

/**
 * Permissions for joining a party
 */
namespace EPartyPermissions
{
	enum Type
	{
		/** any user can discover/join the party */
		Public,
		/** only friends can discover/join the party */
		FriendsOnly,
		/** invite required to discover/join the party */
		Private
	};
}

/**
 * Options for configuring a new party or for updating an existing party
 */
struct FPartyConfiguration
{
	FPartyConfiguration()
		: JoinPerms(EPartyPermissions::FriendsOnly)
		, MaxMembers(0)
	{}

	/** Permission for configuring party */
	EPartyPermissions::Type JoinPerms;
	/** Maximum active members allowed */
	int32 MaxMembers;
	/** PartyId to use - largely for debug purposes*/
	FString PartyId;
	/** Password to use */
	FString Password;
};

namespace EPartyState
{
	enum Type
	{
		None,
		CreatePending,
		DestroyPending,
		JoinPending,
		LeavePending,
		Success,
		Failed
	};
}

/**
 * Current state associated with a party
 */
class FOnlinePartyInfo
{
public:
	FOnlinePartyInfo(const TSharedPtr<FOnlinePartyId>& InPartyId = TSharedPtr<FOnlinePartyId>())
		: State(EPartyState::None)
		, PartyId(InPartyId )
	{}

	virtual ~FOnlinePartyInfo() 
	{}

	EPartyState::Type State;
	/** Current state of configuration */
	FPartyConfiguration Config;
	/** unique id of the party */
	TSharedPtr<FOnlinePartyId> PartyId;
	/** leader of the party */
	TSharedPtr<FUniqueNetId> PartyLeader;
	/** password needed for private parties */
	FString Password;
	/** id of chat room associated with the party */
	FChatRoomId RoomId;
};

// Completion delegates

/**
 * Party creation async task completed callback
 *
 * @param LocalUserId - id of user that initiated the request
 * @param bWasSuccessful - true if successfully created new party
 * @param PartyId - id associated with the party
 * @param Error - string with error info if any
 */
DECLARE_DELEGATE_FourParams(FOnCreatePartyComplete, const FUniqueNetId& /*LocalUserId*/, bool /*bWasSuccessful*/, const FOnlinePartyId& /*PartyId*/, const FString& /*Error*/);
/**
 * Party update async task completed callback
 *
 * @param LocalUserId - id of user that initiated the request
 * @param bWasSuccessful - true if successfully updated party
 * @param PartyId - id associated with the party
 * @param Error - string with error info if any
 */
DECLARE_DELEGATE_FourParams(FOnUpdatePartyComplete, const FUniqueNetId& /*LocalUserId*/, bool /*bWasSuccessful*/, const FOnlinePartyId& /*PartyId*/, const FString& /*Error*/);
/**
 * Party destroy async task completed callback
 *
 * @param LocalUserId - id of user that initiated the request
 * @param bWasSuccessful - true if successfully destroyed party
 * @param PartyId - id associated with the party
 * @param Error - string with error info if any
 */
DECLARE_DELEGATE_FourParams(FOnDestroyPartyComplete, const FUniqueNetId& /*LocalUserId*/, bool /*bWasSuccessful*/, const FOnlinePartyId& /*PartyId*/, const FString& /*Error*/);
/**
 * Party join async task completed callback
 *
 * @param LocalUserId - id of user that initiated the request
 * @param bWasSuccessful - true if successfully joined party
 * @param PartyId - id associated with the party
 * @param Error - string with error info if any
 */
DECLARE_DELEGATE_FourParams(FOnJoinPartyComplete, const FUniqueNetId& /*LocalUserId*/, bool /*bWasSuccessful*/, const FOnlinePartyId& /*PartyId*/, const FString& /*Error*/);
/**
 * Party leave async task completed callback
 *
 * @param LocalUserId - id of user that initiated the request
 * @param bWasSuccessful - true if successfully left party
 * @param PartyId - id associated with the party
 * @param Error - string with error info if any
 */
DECLARE_DELEGATE_FourParams(FOnLeavePartyComplete, const FUniqueNetId& /*LocalUserId*/, bool /*bWasSuccessful*/, const FOnlinePartyId& /*PartyId*/, const FString& /*Error*/);
/**
 * Sending an invite to a user to join party async task completed callback
 *
 * @param LocalUserId - id of user that initiated the request
 * @param bWasSuccessful - true if successfully sent invite
 * @param PartyId - id associated with the party
 * @param RecipientId - id of user invite is being sent to
 * @param Error - string with error info if any
 */
DECLARE_DELEGATE_FiveParams(FOnSendPartyInviteComplete, const FUniqueNetId& /*LocalUserId*/, bool /*bWasSuccessful*/, const FOnlinePartyId& /*PartyId*/, const FUniqueNetId& /*RecipientId*/, const FString& /*Error*/);
/**
 * Kicking a member of a party async task completed callback
 *
 * @param LocalUserId - id of user that initiated the request
 * @param bWasSuccessful - true if successfully kicked member
 * @param PartyId - id associated with the party
 * @param MemberId - id of member being kicked
 * @param Error - string with error info if any
 */
DECLARE_DELEGATE_FiveParams(FOnKickPartyMemberComplete, const FUniqueNetId& /*LocalUserId*/, bool /*bWasSuccessful*/, const FOnlinePartyId& /*PartyId*/, const FUniqueNetId& /*MemberId*/, const FString& /*Error*/);
/**
 * Promoting a member of a party async task completed callback
 *
 * @param LocalUserId - id of user that initiated the request
 * @param bWasSuccessful - true if successfully promoted member to admin
 * @param PartyId - id associated with the party
 * @param MemberId - id of member being promoted to admin
 * @param Error - string with error info if any
 */
DECLARE_DELEGATE_FiveParams(FOnPromotePartyMemberComplete, const FUniqueNetId& /*LocalUserId*/, bool /*bWasSuccessful*/, const FOnlinePartyId& /*PartyId*/, const FUniqueNetId& /*MemberId*/, const FString& /*Error*/);

// Notification delegates

/**
 * Notification when a new invite is received
 *
 * @param RecipientId - id of user that this invite is for
 * @param SenderId - id of member that sent the invite
 * @param PartyId - id associated with the party
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPartyInviteReceived, const FUniqueNetId& /*RecipientId*/, const FUniqueNetId& /*SenderId*/, const FOnlinePartyId& /*PartyId*/);
typedef FOnPartyInviteReceived::FDelegate FOnPartyInviteReceivedDelegate;

/**
 * Notification when a party has been disbanded 
 *
 * @param PartyId - id associated with the party
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartyDisbanded, const FOnlinePartyId& /*PartyId*/);
typedef FOnPartyDisbanded::FDelegate FOnPartyDisbandedDelegate;

/**
 * Notification when a member joins the party
 *
 * @param PartyId - id associated with the party
 * @param MemberId - id of member that joined
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPartyMemberJoined, const FOnlinePartyId& /*PartyId*/, const FUniqueNetId& /*MemberId*/);
typedef FOnPartyMemberJoined::FDelegate FOnPartyMemberJoinedDelegate;

/**
 * Notification when a member leaves the party
 *
 * @param PartyId - id associated with the party
 * @param MemberId - id of member that left
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPartyMemberLeft, const FOnlinePartyId& /*PartyId*/, const FUniqueNetId& /*MemberId*/);
typedef FOnPartyMemberLeft::FDelegate FOnPartyMemberLeftDelegate;

/**
 * Notification when a member is promoted to admin in the party
 *
 * @param PartyId - id associated with the party
 * @param MemberId - id of member that was promoted to admin
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPartyMemberPromoted, const FOnlinePartyId& /*PartyId*/, const FUniqueNetId& /*MemberId*/);
typedef FOnPartyMemberPromoted::FDelegate FOnPartyMemberPromotedDelegate;

/**
 * Notification when party data is updated
 *
 * @param PartyId - id associated with the party
 * @param PartyData - party data that was updated
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPartyDataReceived, const FOnlinePartyId& /*PartyId*/, const TSharedRef<IOnlinePartyData>& /*PartyData*/);
typedef FOnPartyDataReceived::FDelegate FOnPartyDataReceivedDelegate;

/**
 * Notification when party member data is updated
 *
 * @param PartyId - id associated with the party
 * @param MemberId - id of member that had updated data
 * @param PartyMemberData - party member data that was updated
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPartyMemberDataReceived, const FOnlinePartyId& /*PartyId*/, const FUniqueNetId& /*MemberId*/, const TSharedRef<IOnlinePartyMemberData>& /*PartyMemberData*/);
typedef FOnPartyMemberDataReceived::FDelegate FOnPartyMemberDataReceivedDelegate;

/**
 * Interface definition for the online party services 
 * Allows for forming a party and communicating with party members
 */
class IOnlineParty
{
protected:
	IOnlineParty() {};

public:
	virtual ~IOnlineParty() {};

	/**
	 * Create a new party
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyConfig - configuration for the party (can be updated later)
	 * @param Delegate - called on completion
	 * @param UserRoomId - this forces the name of the room to be this value
	 *
	 * @return true if task was started
	 */
	virtual bool CreateParty(const FUniqueNetId& LocalUserId, const FPartyConfiguration& PartyConfig, const FOnCreatePartyComplete& Delegate = FOnCreatePartyComplete()) = 0;

	/**
	 * Update an existing party with new configuration
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyConfig - configuration for the party
	 * @param Delegate - called on completion
	 *
	 * @return true if task was started
	 */
	virtual bool UpdateParty(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FPartyConfiguration& PartyConfig, const FOnUpdatePartyComplete& Delegate = FOnUpdatePartyComplete()) = 0;

	/**
	 * Destroy an existing party. Party members will be disbanded
	 * Only Owner/Admin can destroy the party
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 * @param Delegate - called on completion
	 *
	 * @return true if task was started
	 */
	virtual bool DestroyParty(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FOnDestroyPartyComplete& Delegate = FOnDestroyPartyComplete()) = 0;

	/**
	 * Join an existing party
	 * If it is a private party then an invite is required
	 * All existing party members notified of member joining (see FOnPartyMemberJoined)
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 * @param Delegate - called on completion
	 *
	 * @return true if task was started
	 */
	virtual bool JoinParty(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FOnJoinPartyComplete& Delegate = FOnJoinPartyComplete()) = 0;

	/**
	 * Leave an existing party
	 * All existing party members notified of member leaving (see FOnPartyMemberLeft)
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 * @param Delegate - called on completion
	 *
	 * @return true if task was started
	 */
	virtual bool LeaveParty(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FOnLeavePartyComplete& Delegate = FOnLeavePartyComplete()) = 0;

	/**
	 * Invite a user to an existing party
	 * Recipient will be notified of the new invite (see FOnPartyInviteReceived)
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 * @param RecipientId - id of the user being invited
	 * @param Delegate - called on completion
	 *
	 * @return true if task was started
	 */
	virtual bool SendInvite(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& RecipientId, const FOnSendPartyInviteComplete& Delegate = FOnSendPartyInviteComplete()) = 0;

	/**
	 * Kick a user from an existing party
	 * Only admin can kick a party member
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 * @param MemberId - id of the user being kicked
	 * @param Delegate - called on completion
	 *
	 * @return true if task was started
	 */
	virtual bool KickMember(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId, const FOnKickPartyMemberComplete& Delegate = FOnKickPartyMemberComplete()) = 0;

	/**
	 * Promote a user from an existing party to be admin
	 * All existing party members notified of promoted member (see FOnPartyMemberPromoted)
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 * @param MemberId - id of the user being promoted
	 * @param Delegate - called on completion
	 *
	 * @return true if task was started
	 */
	virtual bool PromoteMember(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId, const FOnPromotePartyMemberComplete& Delegate = FOnPromotePartyMemberComplete()) = 0;

	/**
	 * Set party data and broadcast to all members
	 * Only current data can be set and no history of past party data is preserved
	 * Party members notified of new data (see FOnPartyDataReceived)
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 * @param PartyData - data to send to all party members
	 * @param Delegate - called on completion
	 *
	 * @return true if task was started
	 */
	virtual bool UpdatePartyData(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FOnlinePartyAttrs& PartyData) = 0;

	/**
	 * Set party data for a single party member and broadcast to all members
	 * Only current data can be set and no history of past party member data is preserved
	 * Party members notified of new data (see FOnPartyMemberDataReceived)
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 * @param PartyMemberData - member data to send to all party members
	 * @param Delegate - called on completion
	 *
	 * @return true if task was started
	 */
	virtual bool UpdatePartyMemberData(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FOnlinePartyAttrs& PartyMemberData) = 0;

	/**
	 * Get a list of currently joined parties for the user
	 *
	 * @param LocalUserId - user making the request
	 * @param OutPartyIds - list of party ids joined by the current user
	 *
	 * @return true if entries found
	 */
	virtual bool GetJoinedParties(const FUniqueNetId& LocalUserId, TArray< TSharedRef<FOnlinePartyId> >& OutPartyIds) const = 0;

	/**
	 * Get a list of parties the user has been invited to
	 *
	 * @param LocalUserId - user making the request
	 * @param OutPartyIds - list of party ids joined by the current user
	 *
	 * @return true if entries found
	 */
	virtual bool GetPartyInvites(const FUniqueNetId& LocalUserId, TArray< TSharedRef<FOnlinePartyId> >& OutPartyIds) const = 0;

	/**
	 * Get info associated with a party
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 *
	 * @return party info or nullptr if not found
	 */
	virtual TSharedPtr<FOnlinePartyInfo> GetPartyInfo(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId) const = 0;

	/**
	 * Get list of current party members
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 * @param OutPartyMembers - list of party members currently in the party
	 *
	 * @return true if entries found
	 */
	virtual bool GetPartyMembers(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, TArray< TSharedRef<FOnlinePartyMember> >& OutPartyMembers) const = 0;

	/**
	 * Get list of party members invited to the party but not yet joined
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 * @param OutPendingPartyMembers - list of pending party members
	 *
	 * @return true if entries found
	 */
	virtual bool GetPendingPartyMembers(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, TArray< TSharedRef<FOnlinePartyMember> >& OutPendingPartyMembers) const = 0;

	/**
	 * Get a party member by id
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 * @param MemberId - id of member to find
	 *
	 * @return party member info or nullptr if not found
	 */
	virtual TSharedPtr<FOnlinePartyMember> GetPartyMember(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId) const = 0;	

	/**
	 * Get current cached data associated with a party
	 * FOnPartyDataReceived notification called whenever this data changes
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 *
	 * @return party data or nullptr if not found
	 */
	virtual TSharedPtr<IOnlinePartyData> GetPartyData(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId) const = 0;

	/**
	 * Get current cached data associated with a party member
	 * FOnPartyMemberDataReceived notification called whenever this data changes
	 *
	 * @param LocalUserId - user making the request
	 * @param PartyId - id of an existing party
	 * @param MemberId - id of member to find data for
	 *
	 * @return party member data or nullptr if not found
	 */
	virtual TSharedPtr<IOnlinePartyMemberData> GetPartyMemberData(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId) const = 0;

	/**
	 * Notification when a new invite is received
	 *
	 * @param RecipientId - id of user that this invite is for
	 * @param SenderId - id of member that sent the invite
	 * @param PartyId - id associated with the party
	 */
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnPartyInviteReceived, const FUniqueNetId& /*RecipientId*/, const FUniqueNetId& /*SenderId*/, const FOnlinePartyId& /*PartyId*/);

	/**
	 * Notification when a party has been disbanded 
	 *
	 * @param PartyId - id associated with the party
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnPartyDisbanded, const FOnlinePartyId& /*PartyId*/);

	/**
	 * Notification when a member joins the party
	 *
	 * @param PartyId - id associated with the party
	 * @param MemberId - id of member that joined
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnPartyMemberJoined, const FOnlinePartyId& /*PartyId*/, const FUniqueNetId& /*MemberId*/);

	/**
	 * Notification when a member leaves the party
	 *
	 * @param PartyId - id associated with the party
	 * @param MemberId - id of member that left
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnPartyMemberLeft, const FOnlinePartyId& /*PartyId*/, const FUniqueNetId& /*MemberId*/);

	/**
	 * Notification when a member is promoted to admin in the party
	 *
	 * @param PartyId - id associated with the party
	 * @param MemberId - id of member that was promoted to admin
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnPartyMemberPromoted, const FOnlinePartyId& /*PartyId*/, const FUniqueNetId& /*MemberId*/);

	/**
	 * Notification when party data is updated
	 *
	 * @param PartyId - id associated with the party
	 * @param PartyData - party data that was updated
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnPartyDataReceived, const FOnlinePartyId& /*PartyId*/, const TSharedRef<IOnlinePartyData>& /*PartyData*/);

	/**
	 * Notification when party member data is updated
	 *
	 * @param PartyId - id associated with the party
	 * @param MemberId - id of member that had updated data
	 * @param PartyMemberData - party member data that was updated
	 */
	DEFINE_ONLINE_DELEGATE_THREE_PARAM(OnPartyMemberDataReceived, const FOnlinePartyId& /*PartyId*/, const FUniqueNetId& /*MemberId*/, const TSharedRef<IOnlinePartyMemberData>& /*PartyMemberData*/);
};

typedef TSharedPtr<IOnlineParty, ESPMode::ThreadSafe> IOnlinePartyPtr;
