// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "OnlineIdentityInterface.h"

/**
 * Class used to test the identity interface
 */
 class FTestIdentityInterface
 {
	/** The subsystem that was requested to be tested or the default if empty */
	const FString SubsystemName;
	/** The online interface to use for testing */
	IOnlineIdentityPtr OnlineIdentity;
	/** Delegate to use for authenticating new user */
	FOnLoginCompleteDelegate OnLoginCompleteDelegate;
	/** Delegate to use for logging out a user */
	FOnLogoutCompleteDelegate OnLogoutCompleteDelegate;
	/** Handles to the above delegates */
	FDelegateHandle OnLoginCompleteDelegateHandle;
	FDelegateHandle OnLogoutCompleteDelegateHandle;
	/** true if authentication test should be run */
	bool bRunLoginTest;
	/** true if logout test should be run */
	bool bRunLogoutTest;
	/** Registered user info */
	TSharedPtr<FUserOnlineAccount> UserInfo;
	/** local user to run tests for */
	int32 LocalUserIdx;

	FOnlineAccountCredentials AccountCredentials;

	/** Hidden on purpose */
	FTestIdentityInterface()
		: SubsystemName()
		, LocalUserIdx(0)
	{
	}

	/**
	 * Step through the various tests that should be run and initiate the next one
	 */
	void StartNextTest();

	/**
	 * Finish/cleanup the tests
	 */
	void FinishTest();

	/**
	 * See OnlineIdentityInterface
	 */
	void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void OnLogoutComplete(int32 LocalUserNum, bool bWasSuccessful);

 public:
	/**
	 * Sets the subsystem name to test
	 *
	 * @param InSubsystemName the subsystem to test
	 */
	FTestIdentityInterface(const FString& InSubsystemName)
		: SubsystemName(InSubsystemName)
		, OnlineIdentity(NULL)
		, OnLoginCompleteDelegate(FOnLoginCompleteDelegate::CreateRaw(this, &FTestIdentityInterface::OnLoginComplete))
		, OnLogoutCompleteDelegate(FOnLogoutCompleteDelegate::CreateRaw(this, &FTestIdentityInterface::OnLogoutComplete))
		, bRunLoginTest(true)
		, bRunLogoutTest(false)
		, LocalUserIdx(0)
	{
		
	}

	/**
	 * Kicks off all of the testing process
	 */
	void Test(UWorld* InWorld, const FOnlineAccountCredentials& InAccountCredentials, bool bOnlyRunLogoutTest=false);
 };
