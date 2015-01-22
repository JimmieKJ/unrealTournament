// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSessionInterface.h"
#include "OnlineSubsystemIOSTypes.h"

#include <GameKit/GKSession.h>
#include <GameKit/GKLocalPlayer.h>


@interface FGameCenterSessionDelegate : UIViewController<GKSessionDelegate>
{
};

@property (nonatomic, strong) GKSession *Session;

-(void) initSessionWithName:(NSString*) sessionName;
-(void) shutdownSession;

@end


/**
 * Interface definition for the online services session services 
 * Session services are defined as anything related managing a session 
 * and its state within a platform service
 */
class FOnlineSessionIOS : public IOnlineSession
{
private:

	/** Reference to the main GameCenter subsystem */
	class FOnlineSubsystemIOS* IOSSubsystem;

	/** Hidden on purpose */
	FOnlineSessionIOS();

PACKAGE_SCOPE:

	/** Critical sections for thread safe operation of session lists */
	mutable FCriticalSection SessionLock;

	/** Current session settings */
	TArray<FNamedOnlineSession> Sessions;

	TMap< FName, FGameCenterSessionDelegate* > GKSessions;

	/** Current search object */
	TSharedPtr<FOnlineSessionSearch> CurrentSessionSearch;


PACKAGE_SCOPE:

	/** Constructor */
	FOnlineSessionIOS(class FOnlineSubsystemIOS* InSubsystem);

	/**
	 * Session tick for various background tasks
	 */
	void Tick(float DeltaTime);


	// Begin IOnlineSession interface
	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings) override;

	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSession& Session) override;

public:

	virtual ~FOnlineSessionIOS();

	FNamedOnlineSession* GetNamedSession(FName SessionName) override;

	virtual void RemoveNamedSession(FName SessionName) override;

	virtual EOnlineSessionState::Type GetSessionState(FName SessionName) const override;

	virtual bool HasPresenceSession() override;

	virtual bool CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;

	virtual bool CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;

	virtual bool StartSession(FName SessionName) override;

	virtual bool UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData = false) override;

	virtual bool EndSession(FName SessionName) override;

	virtual bool DestroySession(FName SessionName) override;

	virtual bool IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId) override;

	virtual bool StartMatchmaking(int32 SearchingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings) override;

	virtual bool StartMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings) override;

	virtual bool CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName) override;

	virtual bool CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName) override;

	virtual bool FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;

	virtual bool FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;

	virtual bool CancelFindSessions() override;

	virtual bool PingSearchResults(const FOnlineSessionSearchResult& SearchResult) override;

	virtual bool JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;

	virtual bool JoinSession(const FUniqueNetId& PlayerId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;

	virtual bool FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend) override;

	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend) override;

	virtual bool SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend) override;

	virtual bool SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend) override;

	virtual bool SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<FUniqueNetId> >& Friends) override;

	virtual bool SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<FUniqueNetId> >& Friends) override;

	virtual bool GetResolvedConnectString(FName SessionName, FString& ConnectInfo) override;

	virtual bool GetResolvedConnectString(const class FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo) override;

	virtual FOnlineSessionSettings* GetSessionSettings(FName SessionName) override;

	virtual bool RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited) override;

	virtual bool RegisterPlayers(FName SessionName, const TArray< TSharedRef<FUniqueNetId> >& Players, bool bWasInvited = false) override;

	virtual bool UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId) override;

	virtual bool UnregisterPlayers(FName SessionName, const TArray< TSharedRef<FUniqueNetId> >& Players) override;

	virtual void RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate) override;

	virtual void UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate) override;

	virtual int32 GetNumSessions() override;

	virtual void DumpSessionState() override;
	// End IOnlineSession interface
};

typedef TSharedPtr<FOnlineSessionIOS, ESPMode::ThreadSafe> FOnlineSessionIOSPtr;
