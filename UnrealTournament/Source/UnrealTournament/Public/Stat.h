// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stat.generated.h"

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

/** Enum of all possible recording periods */
UENUM(BlueprintType)
namespace EStatRecordingPeriod
{
	enum Type
	{
		Minute,
		Life,
		Map,
		Campaign,
		Persistent,
		Max,
	};
}

DECLARE_DELEGATE_RetVal_TwoParams( int32 , FStatModifier, int32, int32 )

/** Delegate prototype for the function to be called when the stat is modified */
DECLARE_MULTICAST_DELEGATE_FourParams( FOnStatModified, class AUTPlayerState*, FName, int32, EStatMod::Type);

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

// A single stat
UCLASS()
class UNREALTOURNAMENT_API UStat : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	/** The name of this stat */
	UPROPERTY()
	FName StatName;

	/** Highest period that is kept track of. */
	UPROPERTY()
	TEnumAsByte<EStatRecordingPeriod::Type> HighestPeriodToTrack;

	/** Stat data by period */
	UPROPERTY()
	TArray<int32> StatDataByPeriod;

	/** Never ever let this stat get over this max value regardless of what is in MaxValue. If it is 0, it is ignored.*/
	UPROPERTY()
	int32 AbsoluteMaxValue;

	/** Multicast delegate to be called whenever this stat is modified */
	FOnStatModified OnStatModifiedDelegate;

	/** Modifier function - Adds the Amount to the preexisting InitialValue of the stat
	 *	@param InitialValue The original value of this stat
	 *	@param Amount The amount to add to the InitialValue
	 */
	int32 DeltaModifier(int32 InitialValue, int32 Amount);

	/** Modifier function - Use the Amount, ignoring the InitialValue
	 *	@param InitialValue The original value of this stat
	 *	@param Amount The amount to set the stat to
	 */
	int32 SetModifier(int32 InitialValue, int32 Amount);

	/** Modifier function - Use the max of the existing amount and the new amount
	 *	@param InitialValue The original value of this stat
	 *	@param Amount The amount to set the stat to
	 */
	int32 MaxModifier(int32 InitialValue, int32 Amount);

	/** Called by the game code when something has caused this stat to change
	 *	@param Amount A value used to change the stat
	 *	@param ModType The way in which the Amount will be used to change the stat
	 */
	void ModifyStat(int32 Amount, EStatMod::Type ModType);
};
