// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineIdentityInterface.h"
#include "OnlineSubsystemGooglePlayPackage.h"

#include "gpg/status.h"

class FOnlineIdentityGooglePlay :
	public IOnlineIdentity
{
private:
	
	bool bPrevLoggedIn;
	bool bLoggedIn;
	FString PlayerAlias;
	int32 CurrentLoginUserNum;
	class FOnlineSubsystemGooglePlay* MainSubsystem;

	bool bLoggingInUser;
	bool bRegisteringUser;

	/** UID for this identity */
	TSharedPtr< FUniqueNetIdString > UniqueNetId;

	struct FPendingConnection
	{
		FOnlineIdentityGooglePlay * ConnectionInterface;
		bool IsConnectionPending;
	};

	/** Instance of the connection context data */
	static FPendingConnection PendingConnectRequest;

PACKAGE_SCOPE:

	/**
	 * Default Constructor
	 */
	FOnlineIdentityGooglePlay(FOnlineSubsystemGooglePlay* InSubsystem);

	/** Allow individual interfaces to access the currently signed-in user's id */
	TSharedPtr<FUniqueNetId> GetCurrentUserId() const { return UniqueNetId; }

public:

	// Begin IOnlineIdentity interface
	virtual TSharedPtr<FUserOnlineAccount> GetUserAccount(const FUniqueNetId& UserId) const override;
	virtual TArray<TSharedPtr<FUserOnlineAccount> > GetAllUserAccounts() const override;
	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;
	virtual bool AutoLogin(int32 LocalUserNum) override;
	virtual TSharedPtr<FUniqueNetId> GetUniquePlayerId(int32 LocalUserNum) const override;
	virtual TSharedPtr<FUniqueNetId> CreateUniquePlayerId(uint8* Bytes, int32 Size) override;
	virtual TSharedPtr<FUniqueNetId> CreateUniquePlayerId(const FString& Str) override;
	virtual ELoginStatus::Type GetLoginStatus(int32 LocalUserNum) const override;
	virtual ELoginStatus::Type GetLoginStatus(const FUniqueNetId& UserId) const override;
	virtual FString GetPlayerNickname(int32 LocalUserNum) const override;
	virtual FString GetPlayerNickname(const FUniqueNetId& UserId) const override;
	virtual FString GetAuthToken(int32 LocalUserNum) const override;
	virtual void GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate) override;
	virtual FPlatformUserId GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) override;
	// End IOnlineIdentity interface

public:

	// Begin IOnlineIdentity interface
	void Tick(float DeltaTime);
	// End IOnlineIdentity interface
	
	//platform specific
	void OnLogin(const bool InLoggedIn,  const FString& InPlayerId, const FString& InPlayerAlias);
	void OnLogout(const bool InLoggedIn);

	void OnLoginCompleted(const int playerID, gpg::AuthStatus errorCode);

};


typedef TSharedPtr<FOnlineIdentityGooglePlay, ESPMode::ThreadSafe> FOnlineIdentityGooglePlayPtr;

