// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemGooglePlayPackage.h"

#include "gpg/achievement_manager.h"

class FOnlineSubsystemGooglePlay;

class FOnlineAsyncTaskGooglePlayQueryAchievements : public FOnlineAsyncTaskBasic<FOnlineSubsystemGooglePlay>
{
public:
	FOnlineAsyncTaskGooglePlayQueryAchievements(
		FOnlineSubsystemGooglePlay* InSubsystem,
		const FUniqueNetIdString& InUserId,
		const FOnQueryAchievementsCompleteDelegate& InDelegate);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("QueryAchievements"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

	// FOnlineAsyncTask
	virtual void Tick() override;

private:
	FUniqueNetIdString UserId;
	FOnQueryAchievementsCompleteDelegate Delegate;
	gpg::AchievementManager::FetchAllResponse Response;
};
