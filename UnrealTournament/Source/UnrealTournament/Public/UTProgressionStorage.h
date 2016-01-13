// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
// These are settings that are stored in a remote MCP managed profile.  A copy of them are also stored in the user folder on the local machine
// in case of MCP failure or downtime.

#pragma once

#include "GameFramework/InputSettings.h"
#include "UTProgressionStorage.generated.h"

static const uint32 VALID_VERSION = 6;

class UUTLocalPlayer;
class UUTProfileSettings;

UCLASS()
class UNREALTOURNAMENT_API UUTProgressionStorage : public UObject
{
	GENERATED_UCLASS_BODY()

	bool HasTokenBeenPickedUpBefore(FName TokenUniqueID);
	void TokenPickedUp(FName TokenUniqueID);
	void TokenRevoke(FName TokenUniqueID);
	void TokensCommit();
	void TokensReset();

	bool GetBestTime(FName TimingName, float& OutBestTime);
	void SetBestTime(FName TimingName, float InBestTime);
	
	// debug only
	void TokensClear();

	void VersionFixup();

	/**
	 *	Versioning
	 **/
	UPROPERTY()
	uint32 RevisionNum;

	UPROPERTY()
	TArray<FUTChallengeResult> ChallengeResults;

	UPROPERTY()
	TArray<FUTDailyChallengeUnlock> UnlockedDailyChallenges;

	UPROPERTY()
	int32 TotalChallengeStars;

	virtual void SetTotalChallengeStars(int32 NewValue)
	{
		bNeedsUpdate = true;
		TotalChallengeStars = NewValue;
	}


	UPROPERTY()
	int32 SkullCount;

	virtual void SetSkullCount(int32 NewValue)
	{
		bNeedsUpdate = true;
		SkullCount = NewValue;
	}

	UPROPERTY()
	TArray<FName> Achievements;

	virtual void AddAchievement(FName NewAchievement)
	{
		bNeedsUpdate = true;
		Achievements.Add(NewAchievement);
	}

	virtual void RemoveAchievement(FName AchievementToRemove)
	{
		bNeedsUpdate = true;
		Achievements.Remove(AchievementToRemove);
	}

	virtual void LoadFromProfile(UUTProfileSettings* ProfileSettings);

	virtual bool NeedsUpdate()
	{
		return bNeedsUpdate;
	}

	virtual void Updated()
	{
		bNeedsUpdate = false;
	}

protected:

	// If true then the progress will be written on level change
	bool bNeedsUpdate;

	// Linear list of token unique ids for serialization
	UPROPERTY()
	TArray<FName> FoundTokenUniqueIDs;
	
	TArray<FName> TempFoundTokenUniqueIDs;

	UPROPERTY()
	TMap<FName, float> BestTimes;

};