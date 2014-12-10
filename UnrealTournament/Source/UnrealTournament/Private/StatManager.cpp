// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

DEFINE_LOG_CATEGORY(LogGameStats);

UStatManager::UStatManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StatPrefix = TEXT("STAT_");

	Stats.Add(MakeStat(FName(TEXT("SkillRating")), EStatRecordingPeriod::Persistent));

	Stats.Add(MakeStat(FName(TEXT("Kills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("Deaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("Suicides")), EStatRecordingPeriod::Persistent));

	Stats.Add(MakeStat(FName(TEXT("ImpactHammerKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("EnforcerKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("BioRifleKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("ShockBeamKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("ShockCoreKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("ShockComboKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("LinkKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("LinkBeamKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("MinigunKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("MinigunShardKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("FlakShardKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("FlakShellKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("RocketKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("SniperKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("SniperHeadshotKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("RedeemerKills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("InstagibKills")), EStatRecordingPeriod::Persistent));

	Stats.Add(MakeStat(FName(TEXT("ImpactHammerDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("EnforcerDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("BioRifleDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("ShockBeamDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("ShockCoreDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("ShockComboDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("LinkDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("LinkBeamDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("MinigunDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("MinigunShardDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("FlakShardDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("FlakShellDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("RocketDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("SniperDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("SniperHeadshotDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("RedeemerDeaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("InstagibDeaths")), EStatRecordingPeriod::Persistent));
}

UStat* UStatManager::MakeStat(FName StatName, EStatRecordingPeriod::Type HighestPeriod)
{
	FString NewName = FString::Printf(TEXT("%s%s"), *StatPrefix, *StatName.ToString());
	UStat* Stat = ConstructObject<UStat>(UStat::StaticClass(),this,FName(*NewName));
	Stat->StatName = StatName;
	Stat->HighestPeriodToTrack = HighestPeriod;
	return Stat;
}

UStat* UStatManager::AddOrGetStat(FName StatName, EStatRecordingPeriod::Type HighestPeriod)
{
	UStat* Stat = GetStatByName(StatName);

	if(Stat == NULL)
	{
		Stat = MakeStat(StatName, HighestPeriod);
		Stats.Add(Stat);
		StatLookup.Add(StatName, Stat);
	}

	return Stat;
}

/** Initialize the manager from the config variables
 *  @param FPC The PC that uses this stat manager
 */
void UStatManager::InitializeManager(AUTPlayerState *UTPS)
{
	this->UTPS = UTPS;
}

/** Clears all tracked stats back to 0
 */
void UStatManager::ClearManager()
{
	for(int32 StatIndex = 0; StatIndex < Stats.Num(); StatIndex++)
	{
		for(int32 PeriodIndex = 0; PeriodIndex < Stats[StatIndex]->StatDataByPeriod.Num(); PeriodIndex++)
		{
			Stats[StatIndex]->StatDataByPeriod[PeriodIndex] = 0;
		}
	}
}

/** Clears all tracked stats for this period
 *	@param Period The period to wipe
 */
void UStatManager::ClearPeriod(EStatRecordingPeriod::Type Period)
{
	for(int32 StatIndex = 0; StatIndex < Stats.Num(); StatIndex++)
	{
		Stats[StatIndex]->StatDataByPeriod[Period] = 0;
	}
}

/** Records a stat and determines if this will earn an award
 *  @param Stat The type of stat being recorded
 *	@param Amount The amount it has changed
 *	@param ModType The type of modification to be made on the stat
 *
 *  @return Returns true if the stat was successfully found
 */
bool UStatManager::ModifyStat(FName StatName, int32 Amount, EStatMod::Type ModType)
{
	UStat *Stat;

	Stat = GetStatByName(StatName);
	if (Stat != NULL)
	{
		Stat->ModifyStat(Amount, ModType);
		return true;
	}
	else
	{
		UE_LOG(LogGameStats, Warning, TEXT("Attempt to modify stat '%s' failed as it cannot be found!"), *StatName.ToString());
		return false;
	}
}

UStat* UStatManager::GetStatByName(FName StatName)
{
	return StatLookup.FindRef(StatName);
}

const UStat* UStatManager::GetStatByName(FName StatName) const
{
	return StatLookup.FindRef(StatName);
}

int32 UStatManager::GetStatValueByName(FName StatName, EStatRecordingPeriod::Type Period) const
{
	const UStat* Stat = GetStatByName(StatName);
	if (Stat != NULL)
	{
		return Stat->StatDataByPeriod[Period];
	}
	return 0;
}

int32 UStatManager::GetStatValue(const UStat *Stat, EStatRecordingPeriod::Type Period) const
{
	if (Stat != NULL)
	{
		return Stat->StatDataByPeriod[Period];
	}
	return 0;
}

/**
 * Debugging function to list all stats having certain substring
 */
void UStatManager::DumpStats( FString FilteringMask )
{
	UStat *Stat;
	FString StatOutputString;

	UE_LOG(LogGameStats, Log, TEXT("Showing stats that match substring '%s'"), *FilteringMask);
	for (int32 i =0; i < Stats.Num(); i++)
	{
		Stat = Stats[i];
		if ( FilteringMask == TEXT("") || Stat->StatName.ToString().Contains(FilteringMask) )
		{
			StatOutputString = FString::Printf(TEXT(" - (Campaign:%d), (Map:%d), (Persistent:%d), (Life:%d)"),
				Stat->StatDataByPeriod[EStatRecordingPeriod::Campaign],
				Stat->StatDataByPeriod[EStatRecordingPeriod::Map],
				Stat->StatDataByPeriod[EStatRecordingPeriod::Persistent],
				Stat->StatDataByPeriod[EStatRecordingPeriod::Life]);

			UE_LOG(LogGameStats, Log, TEXT("Stat '%s'  %s'"), *(Stat->StatName.ToString()), *StatOutputString);
		}
	}
}

void UStatManager::PostInitProperties()
{
	Super::PostInitProperties();

	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Populate the stat lookup table with any stats that were added in the constructor.
		PopulateStatLookup();
	}
}

void UStatManager::PostLoad()
{
	Super::PostLoad();

	// Populate the stat lookup table with any stats that were loaded from file.
	PopulateStatLookup();
}

void UStatManager::PopulateStatLookup()
{
	StatLookup.Empty(Stats.Num());

	for (auto* Stat : Stats)
	{
		if (Stat)
		{
			StatLookup.Add(Stat->StatName, Stat);
		}
	}
}