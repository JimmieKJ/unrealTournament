// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StatManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGameStats, Log, All);

/** The different ways that a stat can be modified */
UENUM(BlueprintType)
namespace EStatMod
{
	enum Type
	{
		//Add the given value to the existing value already stored in the stat. The most common modification.
		Delta,
		//Set the stat to the given value, ignoring any previous value.
		Set,
		//Set the stat to the max of the given value and the previous value. Like Set, except it can't lower the stat.
		Maximum,
	};
}

USTRUCT()
struct FMatchStatsPlayer
{
	GENERATED_USTRUCT_BODY()
public:
	FString PlayerName;
	FString StatsID;
	int32 TeamIndex;
	int32 Score;
	int32 Kills;
	int32 Deaths;
	int32 FlagCaptures;
	int32 FlagReturns;
};

USTRUCT()
struct FMatchStats
{
	GENERATED_USTRUCT_BODY()
public:
	TArray<int32> Teams;
	TArray<FMatchStatsPlayer> Players;
	FString GameType;
};

USTRUCT()
struct FStat
{
	GENERATED_USTRUCT_BODY()
public:
	FStat()
	{}

	FStat(bool inbBackendStat)
		: bBackendStat(inbBackendStat), WriteMultiplier(0.0f)
	{}

	FStat(bool inbBackendStat, float inWriteMultiplier)
		: bBackendStat(inbBackendStat), WriteMultiplier(inWriteMultiplier)
	{}
	
	UPROPERTY()
	int32 StatData;

	UPROPERTY()
	bool bBackendStat;

	UPROPERTY()
	float WriteMultiplier;

	void ModifyStat(int32 Amount, EStatMod::Type ModType);
};

// Class that manages all of the stats needed by the game
UCLASS()
class UNREALTOURNAMENT_API UStatManager : public UObject
{
	GENERATED_UCLASS_BODY()

	~UStatManager();

	/** Initialize the manager from the config variables
	 *  @param UTPC The PC that uses this stat manager
	 */
	void InitializeManager(AUTPlayerState *UTPS);

	/** Records a stat and determines if this will earn an award
	*  @param Stat The type of stat being recorded
	*	@param Amount The amount it has changed
	*	@param ModType The type of modification to be made on the stat
	*
	*  @return Returns true if the stat was successfully found
	*/
	bool ModifyStat(FName StatName, int32 Amount, EStatMod::Type ModType);

	//Methods to get a stat or the value of a stat
	FStat* GetStatByName(FName StatName);
	const FStat* GetStatByName(FName StatName) const;
	int32 GetStatValueByName(FName StatName) const;
	int32 GetStatValue(const FStat* Stat) const;
	
	AUTPlayerState* GetPlayerState() { return UTPS; }

	virtual void PopulateJsonObjectForBackendStats(TSharedPtr<FJsonObject> JsonObject, AUTPlayerState* PS);
	virtual void PopulateJsonObjectForNonBackendStats(TSharedPtr<FJsonObject> JsonObject);
	virtual void InsertDataFromNonBackendJsonObject(const TSharedPtr<FJsonObject> JsonObject);

	virtual void AddMatchToStats(const FString& GameType, const TArray<class AUTTeamInfo*>* Teams, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates);

	int32 NumPreviousPlayerNamesToKeep;
	TArray<FString> PreviousPlayerNames;
private:
	int32 NumMatchesToKeep;
	int32 JSONVersionNumber;
	
	/** Map to look up stats by name quickly */
	TMap<FName, FStat*> Stats;
		
	/** Reference to the PS that controls this StatManager */
	UPROPERTY()
	AUTPlayerState *UTPS;

	TArray<FMatchStats> MatchStats;
};