// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"
#include "OnlineExternalUIInterface.h"
#include "OnlineAsyncTaskGooglePlayAuthAction.h"
#include "OnlineSubsystemGooglePlayPackage.h"

#include "gpg/types.h"
#include "gpg/player_manager.h"


class FOnlineSubsystemGooglePlay;

class FOnlineAsyncTaskGooglePlayShowLoginUI : public FOnlineAsyncTaskGooglePlayAuthAction
{
public:
	/**
	 * Constructor.
	 *
	 * @param InSubsystem a pointer to the owning subsystem
	 * @param InPlayerId index of the player who's logging in
	 */
	FOnlineAsyncTaskGooglePlayShowLoginUI(FOnlineSubsystemGooglePlay* InSubsystem, int InPlayerId, const FOnLoginUIClosedDelegate& InDelegate);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("ShowLoginUI"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	/** The subsystem is the only class that should be calling OnAuthActionFinished */
	friend class FOnlineSubsystemGooglePlay;

	// FOnlineAsyncTaskGooglePlayAuthAction
	virtual void OnAuthActionFinished(gpg::AuthOperation InOp, gpg::AuthStatus InStatus) override;
	virtual void OnFetchSelfResponse(const gpg::PlayerManager::FetchSelfResponse& SelfResponse);
	virtual void Start_OnTaskThread() override;

	int PlayerId;
	FOnLoginUIClosedDelegate Delegate;
};
