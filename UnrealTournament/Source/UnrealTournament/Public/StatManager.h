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
	FString MapName;
};

/** XP awards for a player */
USTRUCT()
struct FXPBreakdown
{
	GENERATED_USTRUCT_BODY()

	FXPBreakdown() = default;
	FXPBreakdown(EForceInit)
		: ScoreXP(0), KillAwardXP(0), OffenseXP(0), DefenseXP(0), ChallengeXP(0)
	{}

	/** XP given for match score */
	UPROPERTY()
	float ScoreXP;
	/** XP given for kill awards (multikill, killing spree, etc) */
	UPROPERTY()
		float KillAwardXP;
	/** XP given for offensive objective accomplishments (flag cap, assist) */
	UPROPERTY()
		float OffenseXP;
	/** XP given for defensive objective accomplishments (flag carrier kill, return) */
	UPROPERTY()
		float DefenseXP;
	/** XP given for earnings stars on challenges */
	UPROPERTY()
		float ChallengeXP;

	/** returns sum of all XP awards */
	inline int32 Total() const
	{
		return ScoreXP + KillAwardXP + OffenseXP + DefenseXP + ChallengeXP;
	}

	void operator+= (const FXPBreakdown& B)
	{
		ScoreXP += B.ScoreXP;
		KillAwardXP += B.KillAwardXP;
		OffenseXP += B.OffenseXP;
		DefenseXP += B.DefenseXP;
		ChallengeXP += B.ChallengeXP;
	}
	FXPBreakdown operator+ (const FXPBreakdown& B) const
	{
		FXPBreakdown NewXP(*this);
		NewXP += B;
		return NewXP;
	}
	template<typename NumberType>
	void operator*= (const NumberType B)
	{
		ScoreXP *= B;
		KillAwardXP *= B;
		OffenseXP *= B;
		DefenseXP *= B;
		ChallengeXP *= B;
	}
	template<typename NumberType>
	FXPBreakdown operator* (const NumberType B) const
	{
		FXPBreakdown NewXP(*this);
		NewXP *= B;
		return NewXP;
	}
	template<typename NumberType>
	void operator/= (const NumberType B)
	{
		ScoreXP /= B;
		KillAwardXP /= B;
		OffenseXP /= B;
		DefenseXP /= B;
		ChallengeXP /= B;
	}
	template<typename NumberType>
	FXPBreakdown operator/ (const NumberType B) const
	{
		FXPBreakdown NewXP(*this);
		NewXP /= B;
		return NewXP;
	}
};

// convenient overrides for XPBreakdown to make it easier to grant specific types
struct FNewScoreXP : public FXPBreakdown
{
	FNewScoreXP(float Value)
	: FXPBreakdown(EForceInit::ForceInitToZero)
	{
		ScoreXP = Value;
	}
};
struct FNewKillAwardXP : public FXPBreakdown
{
	FNewKillAwardXP(float Value)
	: FXPBreakdown(EForceInit::ForceInitToZero)
	{
		KillAwardXP = Value;
	}
};
struct FNewOffenseXP : public FXPBreakdown
{
	FNewOffenseXP(float Value)
	: FXPBreakdown(EForceInit::ForceInitToZero)
	{
		OffenseXP = Value;
	}
};
struct FNewDefenseXP : public FXPBreakdown
{
	FNewDefenseXP(float Value)
	: FXPBreakdown(EForceInit::ForceInitToZero)
	{
		DefenseXP = Value;
	}
};
struct FNewChallengeXP : public FXPBreakdown
{
	FNewChallengeXP(float Value)
	: FXPBreakdown(EForceInit::ForceInitToZero)
	{
		ChallengeXP = Value;
	}
};

USTRUCT()
struct FStat
{
	GENERATED_USTRUCT_BODY()
public:
	FStat()
		: StatData(0), bBackendStat(false), WriteMultiplier(0.0f), XPPerPoint(FXPBreakdown(ForceInitToZero))
	{}

	explicit FStat(bool inbBackendStat, const FXPBreakdown& InXP = FXPBreakdown(ForceInitToZero))
		: StatData(0), bBackendStat(inbBackendStat), WriteMultiplier(0.0f), XPPerPoint(InXP)
	{}

	FStat(bool inbBackendStat, float inWriteMultiplier, const FXPBreakdown& InXP = FXPBreakdown(ForceInitToZero))
		: StatData(0), bBackendStat(inbBackendStat), WriteMultiplier(inWriteMultiplier), XPPerPoint(InXP)
	{}

	UPROPERTY()
	int32 StatData;

	UPROPERTY()
	bool bBackendStat;

	UPROPERTY()
	float WriteMultiplier;

	/** XP per point gained of this stat (only applied for player stats and only for Delta mod type) */
	UPROPERTY()
	FXPBreakdown XPPerPoint;

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

	virtual void AddMatchToStats(const FString& MapName, const FString& GameType, const TArray<class AUTTeamInfo*>* Teams, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates);

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