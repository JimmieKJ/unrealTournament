// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

DEFINE_LOG_CATEGORY(LogGameStats);

UStatManager::UStatManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StatPrefix = TEXT("STAT_");

	Stats.Add(MakeStat(FName(TEXT("SkillRating")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("TDMSkillRating")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("DMSkillRating")), EStatRecordingPeriod::Persistent));

	Stats.Add(MakeStat(FName(TEXT("SkillRatingSamples")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("TDMSkillRatingSamples")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("DMSkillRatingSamples")), EStatRecordingPeriod::Persistent));

	Stats.Add(MakeStat(FName(TEXT("MatchesPlayed")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("TimePlayed")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("Wins")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("Losses")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("Kills")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("Deaths")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("Suicides")), EStatRecordingPeriod::Persistent));
	
	Stats.Add(MakeStat(FName(TEXT("MultiKillLevel0")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("MultiKillLevel1")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("MultiKillLevel2")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("MultiKillLevel3")), EStatRecordingPeriod::Persistent));

	Stats.Add(MakeStat(FName(TEXT("SpreeKillLevel0")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("SpreeKillLevel1")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("SpreeKillLevel2")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("SpreeKillLevel3")), EStatRecordingPeriod::Persistent));
	Stats.Add(MakeStat(FName(TEXT("SpreeKillLevel4")), EStatRecordingPeriod::Persistent));

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

	Stats.Add(MakeStat(FName(TEXT("PlayerXP")), EStatRecordingPeriod::Persistent));

	NumMatchesToKeep = 5;
	NumPreviousPlayerNamesToKeep = 5;

	JSONVersionNumber = 0;
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

void UStatManager::PopulateJsonObject(TSharedPtr<FJsonObject> JsonObject)
{
	JsonObject->SetNumberField(TEXT("Version"), JSONVersionNumber);

	for (auto* Stat : Stats)
	{
		if (Stat)
		{
			JsonObject->SetNumberField(Stat->StatName.ToString(), GetStatValue(Stat, EStatRecordingPeriod::Persistent));
		}
	}

	if (PreviousPlayerNames.Num() > 0)
	{
		TArray< TSharedPtr<FJsonValue> > PreviousPlayerNamesJson;
		PreviousPlayerNamesJson.AddZeroed(PreviousPlayerNames.Num());
		for (int32 PlayerNameIdx = 0; PlayerNameIdx < PreviousPlayerNames.Num(); PlayerNameIdx++)
		{
			TSharedPtr<FJsonObject> PlayerNameJson = MakeShareable(new FJsonObject);
			PlayerNameJson->SetStringField(TEXT("Name"), PreviousPlayerNames[PlayerNameIdx]);

			PreviousPlayerNamesJson[PlayerNameIdx] = MakeShareable(new FJsonValueObject(PlayerNameJson));
		}

		JsonObject->SetArrayField(TEXT("Aliases"), PreviousPlayerNamesJson);
	}

	if (MatchStats.Num() > 0)
	{
		TArray< TSharedPtr<FJsonValue> > MatchStatsJson;
		MatchStatsJson.AddZeroed(MatchStats.Num());

		for (int32 MatchIdx = 0; MatchIdx < MatchStats.Num(); MatchIdx++)
		{
			TSharedPtr<FJsonObject> MatchStatJson = MakeShareable(new FJsonObject);

			MatchStatJson->SetStringField(TEXT("GameType"), MatchStats[MatchIdx].GameType);

			if (MatchStats[MatchIdx].Teams.Num() > 0)
			{
				TArray< TSharedPtr<FJsonValue> > TeamStatsJson;
				TeamStatsJson.AddZeroed(MatchStats[MatchIdx].Teams.Num());

				for (int32 TeamIdx = 0; TeamIdx < MatchStats[MatchIdx].Teams.Num(); TeamIdx++)
				{
					TSharedPtr<FJsonObject> TeamJson = MakeShareable(new FJsonObject);

					TeamJson->SetNumberField(TEXT("Score"), MatchStats[MatchIdx].Teams[TeamIdx]);

					TeamStatsJson[TeamIdx] = MakeShareable(new FJsonValueObject(TeamJson));
				}

				MatchStatJson->SetArrayField(TEXT("Teams"), TeamStatsJson);
			}

			if (MatchStats[MatchIdx].Players.Num() > 0)
			{
				TArray< TSharedPtr<FJsonValue> > PlayerStatsJson;
				PlayerStatsJson.AddZeroed(MatchStats[MatchIdx].Players.Num());

				for (int32 PlayerIdx = 0; PlayerIdx < MatchStats[MatchIdx].Players.Num(); PlayerIdx++)
				{
					TSharedPtr<FJsonObject> PlayerJson = MakeShareable(new FJsonObject);
					const FMatchStatsPlayer& Player = MatchStats[MatchIdx].Players[PlayerIdx];

					PlayerJson->SetStringField(TEXT("PlayerName"), Player.PlayerName);
					PlayerJson->SetStringField(TEXT("StatsID"), Player.StatsID);
					PlayerJson->SetNumberField(TEXT("TeamIndex"), Player.TeamIndex);
					PlayerJson->SetNumberField(TEXT("Score"), Player.Score);
					PlayerJson->SetNumberField(TEXT("Kills"), Player.Kills);
					PlayerJson->SetNumberField(TEXT("Deaths"), Player.Deaths);
					PlayerJson->SetNumberField(TEXT("FlagCaptures"), Player.FlagCaptures);
					PlayerJson->SetNumberField(TEXT("FlagReturns"), Player.FlagReturns);

					PlayerStatsJson[PlayerIdx] = MakeShareable(new FJsonValueObject(PlayerJson));
				}

				MatchStatJson->SetArrayField(TEXT("Players"), PlayerStatsJson);
			}

			MatchStatsJson[MatchIdx] = MakeShareable(new FJsonValueObject(MatchStatJson));
		}
		JsonObject->SetArrayField(TEXT("Matches"), MatchStatsJson);
	}
}

void UStatManager::InsertDataFromJsonObject(TSharedPtr<FJsonObject> JsonObject)
{
	for (auto* Stat : Stats)
	{
		if (Stat)
		{
			int32 StatInput = 0;
			if (JsonObject->TryGetNumberField(Stat->StatName.ToString(), StatInput))
			{
				Stat->ModifyStat(StatInput, EStatMod::Set);
			}
		}
	}
	
	const TArray<TSharedPtr<FJsonValue>>* Aliases;
	if (JsonObject->TryGetArrayField(TEXT("Aliases"), Aliases))
	{
		PreviousPlayerNames.Empty();
		PreviousPlayerNames.AddZeroed(Aliases->Num());

		for (int PlayerIdx = 0; PlayerIdx < Aliases->Num(); PlayerIdx++)
		{
			const TSharedPtr<FJsonValue>& AliasJsonValue = (*Aliases)[PlayerIdx];
			PreviousPlayerNames[PlayerIdx] = AliasJsonValue->AsObject()->GetStringField(TEXT("Name"));
		}
	}

	// It would most likely be a performance win to leave matches in JsonValue form so we have less parsing after a user's json is downloaded
	// We don't do anything with MatchStats from previous matches during game time anyway
	const TArray<TSharedPtr<FJsonValue>>* Matches;
	if (JsonObject->TryGetArrayField(TEXT("Matches"), Matches))
	{
		MatchStats.Empty();
		MatchStats.AddZeroed(Matches->Num());

		for (int MatchIdx = 0; MatchIdx < Matches->Num(); MatchIdx++)
		{
			const TSharedPtr<FJsonValue>& MatchJsonValue = (*Matches)[MatchIdx];

			MatchJsonValue->AsObject()->TryGetStringField(TEXT("GameType"), MatchStats[MatchIdx].GameType);

			const TArray<TSharedPtr<FJsonValue>>* Teams;
			if (MatchJsonValue->AsObject()->TryGetArrayField(TEXT("Teams"), Teams))
			{
				MatchStats[MatchIdx].Teams.AddZeroed(Teams->Num());
				for (int32 TeamIdx = 0; TeamIdx < Teams->Num(); TeamIdx++)
				{
					const TSharedPtr<FJsonValue>& TeamJsonValue = (*Teams)[TeamIdx];
					MatchStats[MatchIdx].Teams[TeamIdx] = TeamJsonValue->AsObject()->GetNumberField(TEXT("Score"));
				}
			}
			
			const TArray<TSharedPtr<FJsonValue>>* Players;
			if (MatchJsonValue->AsObject()->TryGetArrayField(TEXT("Players"), Players))
			{
				MatchStats[MatchIdx].Players.AddZeroed(Players->Num());
				for (int32 PlayerIdx = 0; PlayerIdx < Players->Num(); PlayerIdx++)
				{
					const TSharedPtr<FJsonObject> PlayerJsonObject = (*Players)[PlayerIdx]->AsObject();
					FMatchStatsPlayer& Player = MatchStats[MatchIdx].Players[PlayerIdx];

					Player.PlayerName = PlayerJsonObject->GetStringField(TEXT("PlayerName"));
					Player.StatsID = PlayerJsonObject->GetStringField(TEXT("StatsID"));
					PlayerJsonObject->TryGetNumberField(TEXT("TeamIndex"), Player.TeamIndex);
					PlayerJsonObject->TryGetNumberField(TEXT("Score"), Player.Score);
					PlayerJsonObject->TryGetNumberField(TEXT("Kills"), Player.Kills);
					PlayerJsonObject->TryGetNumberField(TEXT("Deaths"), Player.Deaths);
					PlayerJsonObject->TryGetNumberField(TEXT("FlagCaptures"), Player.FlagCaptures);
					PlayerJsonObject->TryGetNumberField(TEXT("FlagReturns"), Player.FlagReturns);
				}
			}
		}
	}
}

void UStatManager::AddMatchToStats(const FString& GameType, const TArray<class AUTTeamInfo*>* Teams, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates)
{
	// Keep match stats at 5
	if (MatchStats.Num() >= NumMatchesToKeep)
	{
		MatchStats.RemoveAt(0, MatchStats.Num() - NumMatchesToKeep + 1);
	}

	FMatchStats NewMatchStats;

	NewMatchStats.GameType = GameType;

	// Teams is optional
	if (Teams != nullptr)
	{
		for (int32 i = 0; i < Teams->Num(); i++)
		{
			if ((*Teams)[i] != nullptr)
			{
				NewMatchStats.Teams.Add((*Teams)[i]->Score);
			}
		}
	}

	for (int32 i = 0; i < ActivePlayerStates->Num(); i++)
	{
		int32 NewIndex = NewMatchStats.Players.AddZeroed();
		FMatchStatsPlayer& MatchPlayer = NewMatchStats.Players[NewIndex];
		if (Cast<AUTPlayerState>((*ActivePlayerStates)[i]) != nullptr)
		{
			const AUTPlayerState& PlayerState = *(Cast<AUTPlayerState>((*ActivePlayerStates)[i]));
			MatchPlayer.PlayerName = PlayerState.PlayerName;
			if (PlayerState.Team != nullptr)
			{
				MatchPlayer.TeamIndex = PlayerState.Team->TeamIndex;
			}
			else
			{
				MatchPlayer.TeamIndex = 0;
			}
			MatchPlayer.StatsID = PlayerState.StatsID;
			MatchPlayer.Score = PlayerState.Score;
			MatchPlayer.Kills = PlayerState.Kills;
			MatchPlayer.Deaths = PlayerState.Deaths;
			MatchPlayer.Score = PlayerState.Score;
			MatchPlayer.FlagCaptures = PlayerState.FlagCaptures;
			MatchPlayer.FlagReturns = PlayerState.FlagReturns;
		}
	}

	for (int32 i = 0; i < InactivePlayerStates->Num(); i++)
	{
		int32 NewIndex = NewMatchStats.Players.AddZeroed();
		FMatchStatsPlayer& MatchPlayer = NewMatchStats.Players[NewIndex];
		if (Cast<AUTPlayerState>((*InactivePlayerStates)[i]) != nullptr)
		{
			const AUTPlayerState& PlayerState = *(Cast<AUTPlayerState>((*InactivePlayerStates)[i]));
			MatchPlayer.PlayerName = PlayerState.PlayerName;
			if (PlayerState.Team != nullptr)
			{
				MatchPlayer.TeamIndex = PlayerState.Team->TeamIndex;
			}
			else
			{
				MatchPlayer.TeamIndex = 0;
			}
			MatchPlayer.StatsID = PlayerState.StatsID;
			MatchPlayer.Score = PlayerState.Score;
			MatchPlayer.Kills = PlayerState.Kills;
			MatchPlayer.Deaths = PlayerState.Deaths;
			MatchPlayer.Score = PlayerState.Score;
			MatchPlayer.FlagCaptures = PlayerState.FlagCaptures;
			MatchPlayer.FlagReturns = PlayerState.FlagReturns;
		}
	}

	MatchStats.Add(NewMatchStats);
}