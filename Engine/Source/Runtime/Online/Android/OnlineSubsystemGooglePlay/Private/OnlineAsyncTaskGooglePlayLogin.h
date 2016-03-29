// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"
#include "OnlineAsyncTaskGooglePlayAuthAction.h"
#include "OnlineSubsystemGooglePlayPackage.h"

#include "gpg/status.h"
#include "gpg/types.h"

class FOnlineSubsystemGooglePlay;

class FOnlineAsyncTaskGooglePlayLogin : public FOnlineAsyncTaskGooglePlayAuthAction
{
public:
	/** Delegate fired upon completion. */
	DECLARE_DELEGATE(FOnCompletedDelegate);

	/**
	 * Constructor.
	 *
	 * @param InSubsystem a pointer to the owning subsysetm
	 * @param InPlayerId index of the player who's logging in
	 */
	FOnlineAsyncTaskGooglePlayLogin(FOnlineSubsystemGooglePlay* InSubsystem, int InPlayerId, const FOnCompletedDelegate& InDelegate);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("Login"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	/** The subsystem is the only class that should be calling OnAuthActionFinished */
	friend class FOnlineSubsystemGooglePlay;

	// FOnlineAsyncTaskGooglePlayAuthAction
	virtual void OnAuthActionFinished(gpg::AuthOperation InOp, gpg::AuthStatus InStatus) override;
	virtual void Start_OnTaskThread() override;

	int PlayerId;
	gpg::AuthStatus Status;
	FOnCompletedDelegate Delegate;
};
