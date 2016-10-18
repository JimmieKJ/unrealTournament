// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"

#include "gpg/leaderboard_manager.h"

class FOnlineSubsystemGooglePlay;

class FOnlineAsyncTaskGooglePlayReadLeaderboard : public FOnlineAsyncTaskBasic<FOnlineSubsystemGooglePlay>
{
public:
	FOnlineAsyncTaskGooglePlayReadLeaderboard(
		FOnlineSubsystemGooglePlay* InSubsystem,
		FOnlineLeaderboardReadRef& InReadObject,
		const FString& InLeaderboardId);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("ReadLeaderboard"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

	// FOnlineAsyncTask
	virtual void Tick() override;

private:
	/** Leaderboard read data */
	FOnlineLeaderboardReadRef ReadObject;

	/** Google Play leaderboard id */
	FString LeaderboardId;

	/** API query result */
	gpg::LeaderboardManager::FetchScoreSummaryResponse Response;
};
