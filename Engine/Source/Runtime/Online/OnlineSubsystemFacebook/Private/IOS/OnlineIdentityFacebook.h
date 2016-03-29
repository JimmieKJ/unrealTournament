// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
 
// Module includes
#include "OnlineIdentityInterface.h"
#include "OnlineSubsystemFacebookPackage.h"

/**
 * Facebook implementation of the online account information we may want
 */
class FUserOnlineAccountFacebook : public FUserOnlineAccount
{

public:

	// FOnlineUser
	
	virtual TSharedRef<const FUniqueNetId> GetUserId() const override;
	virtual FString GetRealName() const override;
	virtual FString GetDisplayName(const FString& Platform = FString()) const override;
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;
	virtual bool SetUserAttribute(const FString& AttrName, const FString& AttrValue) override;

	// FUserOnlineAccount

	virtual FString GetAccessToken() const override;
	virtual bool GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const override;

public:

	/**
	 * Constructor
	 *
	 * @param	InUserId		The user Id which should be assigned for this account.
	 * @param	InAuthTicket	The token which this account is using.
	 */
	FUserOnlineAccountFacebook(const FString& InUserId=TEXT(""), const FString& InAuthTicket=TEXT("")) ;

	/**
	 * Destructor
	 */
	virtual ~FUserOnlineAccountFacebook(){}


private:

	/** The token obtained for this session */
	FString AuthTicket;

	/** UID of the logged in user */
	TSharedRef<const FUniqueNetId> UserId;

	/** Allow the fb identity to fill in our private members from it's callbacks */
	friend class FOnlineIdentityFacebook;
};


/**
 * Facebook service implementation of the online identity interface
 */
class FOnlineIdentityFacebook : public IOnlineIdentity
{

public:

	//~ Begin IOnlineIdentity Interface	
	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;
	virtual bool AutoLogin(int32 LocalUserNum) override;
	virtual TSharedPtr<FUserOnlineAccount> GetUserAccount(const FUniqueNetId& UserId) const override;
	virtual TArray<TSharedPtr<FUserOnlineAccount> > GetAllUserAccounts() const override;
	virtual TSharedPtr<const FUniqueNetId> GetUniquePlayerId(int32 LocalUserNum) const override;
	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(uint8* Bytes, int32 Size) override;
	virtual TSharedPtr<const FUniqueNetId> CreateUniquePlayerId(const FString& Str) override;
	virtual ELoginStatus::Type GetLoginStatus(int32 LocalUserNum) const override;
	virtual ELoginStatus::Type GetLoginStatus(const FUniqueNetId& UserId) const override;
	virtual FString GetPlayerNickname(int32 LocalUserNum) const override;
	virtual FString GetPlayerNickname(const FUniqueNetId& UserId) const override;
	virtual FString GetAuthToken(int32 LocalUserNum) const override;
	virtual void GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate) override;
	virtual FPlatformUserId GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) override;
	virtual FString GetAuthType() const override;
	//~ End IOnlineIdentity Interface


public:

	/**
	 * Default constructor
	 */
	FOnlineIdentityFacebook();


	/**
	 * Destructor
	 */
	virtual ~FOnlineIdentityFacebook()
	{
	}


private:

	/** The user account details associated with this identity */
	TSharedRef< FUserOnlineAccountFacebook > UserAccount;

	/** The current state of our login */
	ELoginStatus::Type LoginStatus;

	/** Username of the user assuming this identity */
	FString Name;
};

typedef TSharedPtr<FOnlineIdentityFacebook, ESPMode::ThreadSafe> FOnlineIdentityFacebookPtr;