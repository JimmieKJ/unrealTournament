// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"
#include "OnlineAsyncTaskGooglePlayAuthAction.h"
#include "OnlineSubsystemGooglePlayPackage.h"

#include "gpg/status.h"
#include "gpg/types.h"

class FOnlineSubsystemGooglePlay;

class FOnlineAsyncTaskGooglePlayLogout : public FOnlineAsyncTaskGooglePlayAuthAction
{
public:
	/**
	 * Constructor.
	 *
	 * @param InSubsystem a pointer to the owning subsysetm
	 * @param InPlayerId index of the player who's logging in
	 */
	FOnlineAsyncTaskGooglePlayLogout(FOnlineSubsystemGooglePlay* InSubsystem, int32 InPlayerId);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("Logout"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	/** The subsystem is the only class that should be calling OnAuthActionFinished */
	friend class FOnlineSubsystemGooglePlay;

	// FOnlineAsyncTaskGooglePlayAuthAction
	virtual void OnAuthActionFinished(gpg::AuthOperation InOp, gpg::AuthStatus InStatus) override;
	virtual void Start_OnTaskThread() override;

	int32 PlayerId;
	gpg::AuthStatus Status;
};
