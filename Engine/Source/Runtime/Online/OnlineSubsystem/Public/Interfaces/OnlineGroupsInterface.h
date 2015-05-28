// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "OnlineSubsystemTypes.h"

/**
 * Data about the group that is used for display
 */
struct FGroupDisplayInfo
{
	/** The human readable name of this guild */
	FText DisplayName;

	/** User entered guild description text */
	FText Description;

	/** URL for an icon file for */
	FString IconUri;
};

/**
 * This struct describes metadata about a group.
 */
struct FGroupInfo
{
	/** Get the group */
	TSharedPtr<FUniqueNetId> GroupId;

	/** Arbitrary namespace string used to filter group groups in some queries or client side. Usually this would be the game codename */
	FString Namespace;

	/** Used in combination with namespace to differentiate different types of groups within a single product */
	FString GroupType;

	/** All fields in this struct are group admin/owner entered */
	FGroupDisplayInfo DisplayInfo;

	/** GUID of the user account that holds the owner role for this group (will only be one) */
	TSharedPtr<FUniqueNetId> Owner;

	/** When was this group created */
	FDateTime CreatedAt;

	/** When was this group last updated (according to the server) */
	FDateTime LastUpdated;
};

/**
 * Enum for the various (mutually exclusive) roles a user can have within a group.
 */
enum class EGroupRole
{
	/* someone who is not a member of the group (usually these won't appear in our maps) */
	Unaffiliated,

	/* someone who has been sent an invitation but has yet to accept (Invitee + Petitioner => Member) */
	Invitee,

	/* someone who has petitioned but has not yet been accepted (Petitioner + Invitee => Member) */
	Petitioner,

	/* has been invited and accepted but does not have any special group status*/
	Member,

	/* has permission to modify the group (invite, promote, update meta info, etc) */
	Admin,

	/* all the privs of admin. Cannot be demoted or kicked by admins. Only this user has permission to disband the group or promote a new owner */
	Owner
};

/**
 * 
 */
struct FGroupEntry
{
	TSharedPtr<FUniqueNetId> Id;
	EGroupRole Role;
};

/**
 * A structure for caching a list of people in the group and their roles
 */
struct FGroupRoster
{
	TSharedPtr<FUniqueNetId> GroupId;
	TArray<FGroupEntry> Users;

	const FGroupEntry* GetUser(const FUniqueNetId& UserId) const { return Users.FindByPredicate([&](const FGroupEntry& Entry) { return *Entry.Id == UserId; }); }
	FGroupEntry* GetUser(const FUniqueNetId& UserId) { return Users.FindByPredicate([&](const FGroupEntry& Entry) { return *Entry.Id == UserId; }); }
};

/**
 * What groups does a particular user currently belong to and what roles do they fill
 */
struct FUserMembership
{
	TSharedPtr<FUniqueNetId> UserId;
	TArray<FGroupEntry> Groups;

	const FGroupEntry* GetGroup(const FUniqueNetId& GroupId) const { return Groups.FindByPredicate([&](const FGroupEntry& Entry) { return *Entry.Id == GroupId; }); }
	FGroupEntry* GetGroup(const FUniqueNetId& GroupId) { return Groups.FindByPredicate([&](const FGroupEntry& Entry) { return *Entry.Id == GroupId; }); }
};

/**
 * Structure to encapsulate the result of a call to the server. This only contains enough information to check for errors
 */
struct FGroupsResult
{
	TSharedPtr<FUniqueNetId> PrimaryId;
	int32 HttpStatus;

	inline bool DidSucceed() const { return (HttpStatus / 100) == 2; }

	FGroupsResult()
		: HttpStatus(0)
	{
	}
};

/**
 * Generic delegate used for when an asynchronous query completes
 *
 * @param Result A structure containing information about whether the query succeeded or failed. 
 *        Use the GetCached* related function once this delegate has been called to retrieve the result payload.
 */
DECLARE_DELEGATE_OneParam(FOnGroupsRequestCompleted, FGroupsResult);

/**
 * Specialized result for FindGroups 
 */
struct FFindGroupsResult
{
	TArray< TSharedPtr<const FGroupInfo> > MatchingGroups;
	int32 HttpStatus;

	inline bool DidSucceed() const { return (HttpStatus / 100) == 2; }

	FFindGroupsResult()
		: HttpStatus(0)
	{
	}
};
DECLARE_DELEGATE_OneParam(FOnFindGroupsCompleted, FFindGroupsResult);

/**
 * Interface definition for the online teams (groups) service
 * Groups are collections of users with an owner and a set of administrators
 */
class IOnlineGroups
{
public: // delegates

	/**
	 * Delegate fired when a group is updated. We only receive notifications for groups for which one 
	 * of the logged-in players is a member.
	 * 
	 * @param GroupId The GUID of the group.
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGroupUpdated, const FUniqueNetId&);
	FOnGroupUpdated OnGroupUpdated;

public: // can be called by any account

	/**
	 * Find all groups matching the specified search string within the namespace specified.
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param SearchString This string is matched against group name.
	 * @param Namespace The namespace to restrict the search to (i.e. "fortnite")
	 * @param OnCompleted Callback delegate which will receive the results of the search. Any GroupInfo from the search will 
	 *        also be available from GetCachedGroupInfo.
	 */
	virtual void FindGroups(const FUniqueNetId& ContextUserId, const FText& SearchString, const FString& Namespace, const FOnFindGroupsCompleted& OnCompleted) = 0;

	/**
	 * Create a new group using the specified GroupInfo. The "GroupId", "Owner", "CreatedAt", and "LastUpdated" fields of
	 * GroupInfo are ignored. If owner is not specified, it will default to the context user. FGroupsResult::GroupId
	 * can be used to identify the newly created group once the callback executes (FGroupsResult being a param to 
	 * the callback).
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupInfo The display info to use for the new group. Name, description, etc
	 * @param Namespace What namespace to create this group in. This must match a namespace the server recognizes. Some 
	 *        configuration options (like how many groups a user can own or be a member of) are inferred from this.
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void CreateGroup(const FUniqueNetId& ContextUserId, const FGroupDisplayInfo& GroupInfo, const FString& Namespace, const FString& GroupType, const FOnGroupsRequestCompleted& OnCompleted) = 0;

	/**
	 * Submit a request to join the specified group OR accept a pending invitation. This is done on behalf of the context 
	 * user (provided when you request this IOnlineGroups interface) and also updates the their cached membership
	 * info. You can call GetCachedUserMembership afterwards to see if they are a full member or petitioner.
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void JoinGroup(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId, const FOnGroupsRequestCompleted& OnCompleted) = 0;

	/**
	 * Tells the server to remove the context user from the specified group OR declines an invitation OR rescinds a petition.
	 * Should always result in the user's role for that group becoming Unaffiliated. Owners cannot leave their group, use
	 * TransferGroup (followed by LeaveGroup) or DeleteGroup instead.
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void LeaveGroup(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId, const FOnGroupsRequestCompleted& OnCompleted) = 0;

	/** 
	 * Get the group info for a group that has been previously queried. The shared pointer will be 
	 * empty if the group has not been successfully queried (or if it was purged from the cache).
	 * From time to time, the cache may be cleaned up. Keep a shared pointer to groups you care about
	 * or be willing to re-query.
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID
	 * @return Shared pointer to the cached group info structure if one exists.
	 */
	virtual TSharedPtr<const FGroupInfo> GetCachedGroupInfo(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId) = 0;

	/** 
	 * Ask the server for GroupInfo corresponding to the provided group ID. If this completes 
	 * successfully, you can call GetCachedGroupInfo to get the full group information. Even if the group info
	 * is currently cached, this function will contact the server (ostensibly to refresh the cache).
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID to query
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void QueryGroupInfo(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId, const FOnGroupsRequestCompleted& OnCompleted) = 0;

	/**
	 * Bulk version of QueryGroupInfo
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupIds An array of group IDs to query
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void QueryGroupInfo(const FUniqueNetId& ContextUserId, const TArray< TSharedRef<FUniqueNetId> >& GroupIds, const FOnGroupsRequestCompleted& OnCompleted) = 0;

	/**
	 * Get the cached Roster (membership) information for a group. If the information is not cached locally, call 
	 * QueryGroupRoster to request it from the server. If the context user is a member of this group, the cached roster
	 * information will include petitioners and invitees, otherwise it will not.
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID
	 * @return Shared pointer to the cached roster structure if one exists
	 */
	virtual TSharedPtr<const FGroupRoster> GetCachedGroupRoster(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId) = 0;
	
	/**
	 * Get the list of members for a group and their role info. If the context user for this interface is a group member, 
	 * you will also receive users with the roles of Invitee and Petitioner otherwise those are omitted. Use AddUser and 
	 * RemoveUser to accept or decline petitions respectively. 
	 * When the OnCompleted callback fires, if it succeeded you can use GetCachedGroupRoster to retrieve the membership 
	 * information.
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void QueryGroupRoster(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId, const FOnGroupsRequestCompleted& OnCompleted) = 0;

	/**
	 * Get cached user membership information (if it exists). This retrieves which groups they are members of.
	 * If UserId is the ID of the context user, then this list will include groups to which the user has been invited 
	 * (use JoinGroup to accept, LeaveGroup to decline) or groups they have petitioned but haven't received a 
	 * response for yet.
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param UserId The ID of the user to query.
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual TSharedPtr<const FUserMembership> GetCachedUserMembership(const FUniqueNetId& ContextUserId, const FUniqueNetId& UserId) = 0;

	/**
	 * Queries the server for updated membership information for a particular user. This retrieves which groups they are members of.
	 * If the callback reports success, use GetCachedUserMembership to retrieve details.
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param UserId The user to query for membership information.
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void QueryUserMembership(const FUniqueNetId& ContextUserId, const FUniqueNetId& UserId, const FOnGroupsRequestCompleted& OnCompleted) = 0;

public: // can be called by group admins

	/**
	 * Update the user specified fields of the group such as name, description, etc. This call may fail if the name changes and 
	 * the new name is already in use within this namespace.
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID
	 * @param GroupInfo The new group display info structure.
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void UpdateGroupInfo(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId, const FGroupDisplayInfo& GroupInfo, const FOnGroupsRequestCompleted& OnCompleted) = 0;
	
	/**
	 * Invite a user to join the specified group OR accept a pending petition. 
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID
	 * @param UserId The user to add to the group.
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void AddUser(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId, const FUniqueNetId& UserId, const FOnGroupsRequestCompleted& OnCompleted) = 0;

	/**
	 * Kick a user from the group OR decline a petition OR rescind an invitation (success always results in the user's role becoming Unaffiliated).
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID
	 * @param UserId The user to remove from the group.
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void RemoveUser(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId, const FUniqueNetId& UserId, const FOnGroupsRequestCompleted& OnCompleted) = 0;

	/**
	 * Promote a Member to an Admin within the specified group. Fails if the user's role is not Member.
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID
	 * @param UserId The member to promote.
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void PromoteUser(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId, const FUniqueNetId& UserId, const FOnGroupsRequestCompleted& OnCompleted) = 0;
	
	/**
	 * Demote an Admin to a Member within the specified group. Fails if the user's role is not Admin (Owner != Admin).
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID
	 * @param UserId The user to demote to regular member.
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void DemoteUser(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId, const FUniqueNetId& UserId, const FOnGroupsRequestCompleted& OnCompleted) = 0;

public: // can be called by group owner only

	/**
	 * Promote an existing Admin to be the new Owner and simultaneously demotes the context user to Admin. Fails if the new owner
	 * is not currently an Admin of the group. Also fails if the new owner cannot own the group (due to owning too many other groups, as
	 * determined by the namespace config, perhaps).
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID
	 * @param UserId The user who should become the new group owner. This user must already be an Admin or the call fails.
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void TransferGroup(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId, const FUniqueNetId& NewOwnerId, const FOnGroupsRequestCompleted& OnCompleted) = 0;

	/**
	 * Delete a group entirely.
	 *
	 * @param ContextUserId The ID of the user whose credentials are being used to make this call
	 * @param GroupId The group's globally unique ID
	 * @param OnCompleted This callback is invoked after contacting the server. It is guaranteed to occur
	 *        (regardless of success/fail) and will not be called before this function returns.
	 */
	virtual void DeleteGroup(const FUniqueNetId& ContextUserId, const FUniqueNetId& GroupId, const FOnGroupsRequestCompleted& OnCompleted) = 0;

protected:
	IOnlineGroups() {}
public:
	virtual ~IOnlineGroups() {};
};

typedef TSharedPtr<IOnlineGroups, ESPMode::ThreadSafe> IOnlineGroupsPtr;
