// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LeaderboardBlueprintLibrary.generated.h"

/**
 * A beacon host used for taking reservations for an existing game session
 */
UCLASS()
class ONLINESUBSYSTEMUTILS_API ULeaderboardBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Writes an integer value to the specified leaderboard */
	UFUNCTION(BlueprintCallable, Category = "Online|Leaderboard")
	static bool WriteLeaderboardInteger(APlayerController* PlayerController, FName StatName, int32 StatValue);

private:
	static bool WriteLeaderboardObject(APlayerController* PlayerController, class FOnlineLeaderboardWrite& WriteObject);
};
