// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemGooglePlayPackage.h"

#include "gpg/status.h"
#include "gpg/types.h"

class FOnlineSubsystemGooglePlay;

class FOnlineAsyncTaskGooglePlayLogin : public FOnlineAsyncTaskBasic<FOnlineSubsystemGooglePlay>
{
public:
	/**
	 * Constructor.
	 *
	 * @param InSubsystem a pointer to the owning subsysetm
	 * @param InPlayerId index of the player who's logging in
	 */
	FOnlineAsyncTaskGooglePlayLogin(FOnlineSubsystemGooglePlay* InSubsystem, int InPlayerId);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("Login"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

PACKAGE_SCOPE:
	/**
	 * The OnAuthActionFinished callback is handled globally in FOnlineSubsystemGooglePlay.
	 * The subsystem is responsible for forwarding the call to any pending login task.
	 *
	 * @param InOp Forwarded from Google's callback, indicates whether this was a sign-in or a sign-out
	 * @param InStatus Forwarded from Google's callback, indicates whether the operation succeeded
	 */
	void OnAuthActionFinished(gpg::AuthOperation InOp, gpg::AuthStatus InStatus);

private:
	int PlayerId;
	gpg::AuthStatus Status;
	int RetryCount;
};
