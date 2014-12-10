// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stat.h"

#include "StatManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGameStats, Log, All);

// Class that manages all of the stats needed by the game
UCLASS()
class UNREALTOURNAMENT_API UStatManager : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Array of stats tracked */
	UPROPERTY()
	TArray<UStat*> Stats;
	
	// Prefix prepended to all stat names when constructing stat objects
	UPROPERTY()
	FString StatPrefix;

	/** Reference to the PS that controls this StatManager */
	UPROPERTY()
	AUTPlayerState *UTPS;

	/** Helper function that makes a stat object */
	UStat* MakeStat(FName StatName, EStatRecordingPeriod::Type HighestPeriod);

	/** Helper function to add a stat while ensuring that it does not create a duplicate */
	UStat* AddOrGetStat(FName StatName, EStatRecordingPeriod::Type HighestPeriod = EStatRecordingPeriod::Campaign);

	/** Initialize the manager from the config variables
	 *  @param UTPC The PC that uses this stat manager
	 */
	void InitializeManager(AUTPlayerState *UTPS);

	/** Clears all tracked stats back to 0
	 */
	void ClearManager();

	/** Clears all tracked stats for this period
	*	@param Period The period to wipe
	*/
	void ClearPeriod(EStatRecordingPeriod::Type Period);

	/** Records a stat and determines if this will earn an award
	*  @param Stat The type of stat being recorded
	*	@param Amount The amount it has changed
	*	@param ModType The type of modification to be made on the stat
	*
	*  @return Returns true if the stat was successfully found
	*/
	bool ModifyStat(FName StatName, int32 Amount, EStatMod::Type ModType);

	//Methods to get a stat or the value of a stat
	UStat* GetStatByName(FName StatName);
	const UStat* GetStatByName(FName StatName) const;
	int32 GetStatValueByName(FName StatName, EStatRecordingPeriod::Type Period) const;
	int32 GetStatValue(const UStat *Stat, EStatRecordingPeriod::Type Period) const;

	/**
	 * Debugging function to list all stats having certain substring
	 */
	void DumpStats( FString FilteringMask );

	// Begin UObject interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	// End UObject interface

private:
	/**
	 * Called during load to populate the StatLookup map. This is used to look up stat objects quickly.
	 * This is called during PostLoad to populate the map with stats that were previously saved.
	 * The StatLookup map will then be updates as stats are created at runtime, so this function should only be called once.
	 */
	void PopulateStatLookup();

	/** Map to look up stats by name quickly */
	TMap<FName, UStat*> StatLookup;
};