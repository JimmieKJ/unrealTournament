// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManager.h"

class FOnlineSubsystemGooglePlay;

class FOnlineAsyncTaskGooglePlayQueryInAppPurchases : public FOnlineAsyncTaskBasic<FOnlineSubsystemGooglePlay>
{
public:
	FOnlineAsyncTaskGooglePlayQueryInAppPurchases(
		FOnlineSubsystemGooglePlay* InSubsystem,
		const TArray<FString> InProductIds,
		const TArray<bool> InIsConsumableFlags);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("QueryInAppPurchases"); }
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

	// FOnlineAsyncTask
	virtual void Tick() override;

	// FOnlineAsyncTaskGooglePlayQueryInAppPurchases
	void ProcessQueryAvailablePurchasesResults(bool bInSuccessful);

private:

	// The product ids provided for this task
	const TArray<FString> ProductIds;

	// The consume flags provided for this task
	const TArray<bool> IsConsumableFlags;

	/** Flag indicating that the request has been sent */
	bool bWasRequestSent;
};
