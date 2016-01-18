// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Implement the Friends data access
*/
class FOSSScheduler
	: public TSharedFromThis < FOSSScheduler >
{
public:

	/** Destructor. */
	virtual ~FOSSScheduler() {};

	virtual void Login(IOnlineSubsystem* InOnlineSub, bool bInIsGame, int32 LocalUserID) = 0;
	virtual void Logout() = 0;
	virtual bool IsLoggedIn() const = 0;
	virtual void SetOnline() = 0;
	virtual void SetAway() = 0;
	virtual bool CanChatOnline() = 0;
	virtual EOnlinePresenceState::Type GetOnlineStatus() = 0;
	virtual FString GetUserAppId() const = 0;
	virtual FString GetPlayerNickname(const FUniqueNetId& LocalUserId) const = 0;
	virtual FString GetPlatformAuthType() const = 0;
	virtual void SetUserIsOnline(EOnlinePresenceState::Type OnlineState) = 0;
	virtual void SetOverridePresence(const FString& PresenceID) = 0;
	virtual void ClearOverridePresence() = 0;
	virtual TSharedPtr<const FUniqueNetId> GetGameSessionId() const = 0;
	virtual bool IsFriendInSameSession(const TSharedPtr< const IFriendItem >& FriendItem) const = 0;
	virtual bool IsFriendInSameParty(const TSharedPtr< const IFriendItem >& FriendItem) const = 0;
	virtual TSharedPtr<IOnlinePartyJoinInfo> GetPartyJoinInfo(const TSharedPtr<const IFriendItem>& FriendItem) const = 0;
	

	virtual TSharedPtr<class FFriendsAndChatAnalytics> GetAnalytics() const = 0;
	virtual IOnlineFriendsPtr GetFriendsInterface() const = 0;
	virtual IOnlineUserPtr GetUserInterface() const = 0;
	virtual IOnlineIdentityPtr GetOnlineIdentity() const = 0;
	virtual IOnlinePartyPtr GetPartyInterface() const = 0;
	virtual IOnlineSessionPtr GetSessionInterface() const = 0;
	virtual IOnlinePresencePtr GetPresenceInterface() const = 0;
	virtual IOnlineChatPtr GetChatInterface() const = 0;

	DECLARE_DELEGATE(FOnRequestOSSAccepted);

	virtual void RequestOSS(FOnRequestOSSAccepted OnRequestOSSAcceptedDelegate) = 0;
	virtual void ReleaseOSS() = 0;
};

/**
* Creates the implementation of a chat manager.
*
* @return the newly created FriendViewModel implementation.
*/
FACTORY(TSharedRef< FOSSScheduler >, FOSSScheduler, TSharedPtr<class FFriendsAndChatAnalytics> Analytics);
