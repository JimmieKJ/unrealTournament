// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "StatNames.h"

DEFINE_LOG_CATEGORY(LogGameStats);

UStatManager::UStatManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StatPrefix = TEXT("STAT_");

	Stats.Add(MakeStat(NAME_SkillRating, false));
	Stats.Add(MakeStat(NAME_TDMSkillRating, false));
	Stats.Add(MakeStat(NAME_DMSkillRating, false));
	Stats.Add(MakeStat(NAME_CTFSkillRating, false));

	Stats.Add(MakeStat(NAME_SkillRatingSamples, false));
	Stats.Add(MakeStat(NAME_TDMSkillRatingSamples, false));
	Stats.Add(MakeStat(NAME_DMSkillRatingSamples, false));
	Stats.Add(MakeStat(NAME_CTFSkillRatingSamples, false));

	Stats.Add(MakeStat(NAME_MatchesPlayed, true));
	Stats.Add(MakeStat(NAME_MatchesQuit, true));
	Stats.Add(MakeStat(NAME_TimePlayed, true));
	Stats.Add(MakeStat(NAME_Wins, true));
	Stats.Add(MakeStat(NAME_Losses, true));
	Stats.Add(MakeStat(NAME_Kills, true));
	Stats.Add(MakeStat(NAME_Deaths, true));
	Stats.Add(MakeStat(NAME_Suicides, true));
	
	Stats.Add(MakeStat(NAME_MultiKillLevel0, true));
	Stats.Add(MakeStat(NAME_MultiKillLevel1, true));
	Stats.Add(MakeStat(NAME_MultiKillLevel2, true));
	Stats.Add(MakeStat(NAME_MultiKillLevel3, true));

	Stats.Add(MakeStat(NAME_SpreeKillLevel0, true));
	Stats.Add(MakeStat(NAME_SpreeKillLevel1, true));
	Stats.Add(MakeStat(NAME_SpreeKillLevel2, true));
	Stats.Add(MakeStat(NAME_SpreeKillLevel3, true));
	Stats.Add(MakeStat(NAME_SpreeKillLevel4, true));


	Stats.Add(MakeStat(NAME_ImpactHammerKills, true));
	Stats.Add(MakeStat(NAME_EnforcerKills, true));
	Stats.Add(MakeStat(NAME_BioRifleKills, true));
	Stats.Add(MakeStat(NAME_ShockBeamKills, true));
	Stats.Add(MakeStat(NAME_ShockCoreKills, true));
	Stats.Add(MakeStat(NAME_ShockComboKills, true));
	Stats.Add(MakeStat(NAME_LinkKills, true));
	Stats.Add(MakeStat(NAME_LinkBeamKills, true));
	Stats.Add(MakeStat(NAME_MinigunKills, true));
	Stats.Add(MakeStat(NAME_MinigunShardKills, true));
	Stats.Add(MakeStat(NAME_FlakShardKills, true));
	Stats.Add(MakeStat(NAME_FlakShellKills, true));
	Stats.Add(MakeStat(NAME_RocketKills, true));
	Stats.Add(MakeStat(NAME_SniperKills, true));
	Stats.Add(MakeStat(NAME_SniperHeadshotKills, true));
	Stats.Add(MakeStat(NAME_RedeemerKills, true));
	Stats.Add(MakeStat(NAME_InstagibKills, true));

	Stats.Add(MakeStat(NAME_ImpactHammerDeaths, true));
	Stats.Add(MakeStat(NAME_EnforcerDeaths, true));
	Stats.Add(MakeStat(NAME_BioRifleDeaths, true));
	Stats.Add(MakeStat(NAME_ShockBeamDeaths, true));
	Stats.Add(MakeStat(NAME_ShockCoreDeaths, true));
	Stats.Add(MakeStat(NAME_ShockComboDeaths, true));
	Stats.Add(MakeStat(NAME_LinkDeaths, true));
	Stats.Add(MakeStat(NAME_LinkBeamDeaths, true));
	Stats.Add(MakeStat(NAME_MinigunDeaths, true));
	Stats.Add(MakeStat(NAME_MinigunShardDeaths, true));
	Stats.Add(MakeStat(NAME_FlakShardDeaths, true));
	Stats.Add(MakeStat(NAME_FlakShellDeaths, true));
	Stats.Add(MakeStat(NAME_RocketDeaths, true));
	Stats.Add(MakeStat(NAME_SniperDeaths, true));
	Stats.Add(MakeStat(NAME_SniperHeadshotDeaths, true));
	Stats.Add(MakeStat(NAME_RedeemerDeaths, true));
	Stats.Add(MakeStat(NAME_InstagibDeaths, true));

	Stats.Add(MakeStat(NAME_PlayerXP, true));

	NumMatchesToKeep = 5;
	NumPreviousPlayerNamesToKeep = 5;

	JSONVersionNumber = 0;
}

UStat* UStatManager::MakeStat(FName StatName, bool bBackendStat)
{
	FString NewName = FString::Printf(TEXT("%s%s"), *StatPrefix, *StatName.ToString());
	UStat* Stat = NewObject<UStat>(this,FName(*NewName));
	Stat->StatName = StatName;
	Stat->HighestPeriodToTrack = EStatRecordingPeriod::Persistent;
	Stat->bBackendStat = bBackendStat;

	return Stat;
}

/** Initialize the manager from the config variables
 *  @param FPC The PC that uses this stat manager
 */
void UStatManager::InitializeManager(AUTPlayerState *inUTPS)
{
	this->UTPS = inUTPS;
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

void UStatManager::PopulateJsonObjectForBackendStats(TSharedPtr<FJsonObject> JsonObject)
{
	for (auto* Stat : Stats)
	{
		if (Stat && Stat->bBackendStat)
		{
			JsonObject->SetNumberField(Stat->StatName.ToString(), GetStatValue(Stat, EStatRecordingPeriod::Persistent));
		}
	}
}

void UStatManager::PopulateJsonObjectForNonBackendStats(TSharedPtr<FJsonObject> JsonObject)
{
	JsonObject->SetNumberField(TEXT("Version"), JSONVersionNumber);

	for (auto* Stat : Stats)
	{
		if (Stat && !Stat->bBackendStat)
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

void UStatManager::InsertDataFromNonBackendJsonObject(TSharedPtr<FJsonObject> JsonObject)
{
	for (auto* Stat : Stats)
	{
		if (Stat && !Stat->bBackendStat)
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

		for (int32 PlayerIdx = 0; PlayerIdx < Aliases->Num(); PlayerIdx++)
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

		for (int32 MatchIdx = 0; MatchIdx < Matches->Num(); MatchIdx++)
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