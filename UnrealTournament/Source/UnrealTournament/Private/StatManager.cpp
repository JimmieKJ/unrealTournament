// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "StatNames.h"
#include "UTCTFScoring.h"

DEFINE_LOG_CATEGORY(LogGameStats);

UStatManager::UStatManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Stats.Add(NAME_SkillRating, new FStat(false));
	Stats.Add(NAME_TDMSkillRating, new FStat(false));
	Stats.Add(NAME_DMSkillRating, new FStat(false));
	Stats.Add(NAME_CTFSkillRating, new FStat(false));

	Stats.Add(NAME_SkillRatingSamples, new FStat(false));
	Stats.Add(NAME_TDMSkillRatingSamples, new FStat(false));
	Stats.Add(NAME_DMSkillRatingSamples, new FStat(false));
	Stats.Add(NAME_CTFSkillRatingSamples, new FStat(false));

	Stats.Add(NAME_MatchesPlayed, new FStat(true));
	Stats.Add(NAME_MatchesQuit, new FStat(true));
	Stats.Add(NAME_TimePlayed, new FStat(true));
	Stats.Add(NAME_Wins, new FStat(true));
	Stats.Add(NAME_Losses, new FStat(true));
	Stats.Add(NAME_Kills, new FStat(true));
	Stats.Add(NAME_Deaths, new FStat(true));
	Stats.Add(NAME_Suicides, new FStat(true));
	
	Stats.Add(NAME_MultiKillLevel0, new FStat(true));
	Stats.Add(NAME_MultiKillLevel1, new FStat(true));
	Stats.Add(NAME_MultiKillLevel2, new FStat(true));
	Stats.Add(NAME_MultiKillLevel3, new FStat(true));

	Stats.Add(NAME_SpreeKillLevel0, new FStat(true));
	Stats.Add(NAME_SpreeKillLevel1, new FStat(true));
	Stats.Add(NAME_SpreeKillLevel2, new FStat(true));
	Stats.Add(NAME_SpreeKillLevel3, new FStat(true));
	Stats.Add(NAME_SpreeKillLevel4, new FStat(true));

	Stats.Add(NAME_ImpactHammerKills, new FStat(true));
	Stats.Add(NAME_EnforcerKills, new FStat(true));
	Stats.Add(NAME_BioRifleKills, new FStat(true));
	Stats.Add(NAME_ShockBeamKills, new FStat(true));
	Stats.Add(NAME_ShockCoreKills, new FStat(true));
	Stats.Add(NAME_ShockComboKills, new FStat(true));
	Stats.Add(NAME_LinkKills, new FStat(true));
	Stats.Add(NAME_LinkBeamKills, new FStat(true));
	Stats.Add(NAME_MinigunKills, new FStat(true));
	Stats.Add(NAME_MinigunShardKills, new FStat(true));
	Stats.Add(NAME_FlakShardKills, new FStat(true));
	Stats.Add(NAME_FlakShellKills, new FStat(true));
	Stats.Add(NAME_RocketKills, new FStat(true));
	Stats.Add(NAME_SniperKills, new FStat(true));
	Stats.Add(NAME_SniperHeadshotKills, new FStat(true));
	Stats.Add(NAME_RedeemerKills, new FStat(true));
	Stats.Add(NAME_InstagibKills, new FStat(true));
	Stats.Add(NAME_TelefragKills, new FStat(true));

	Stats.Add(NAME_ImpactHammerDeaths, new FStat(true));
	Stats.Add(NAME_EnforcerDeaths, new FStat(true));
	Stats.Add(NAME_BioRifleDeaths, new FStat(true));
	Stats.Add(NAME_ShockBeamDeaths, new FStat(true));
	Stats.Add(NAME_ShockCoreDeaths, new FStat(true));
	Stats.Add(NAME_ShockComboDeaths, new FStat(true));
	Stats.Add(NAME_LinkDeaths, new FStat(true));
	Stats.Add(NAME_LinkBeamDeaths, new FStat(true));
	Stats.Add(NAME_MinigunDeaths, new FStat(true));
	Stats.Add(NAME_MinigunShardDeaths, new FStat(true));
	Stats.Add(NAME_FlakShardDeaths, new FStat(true));
	Stats.Add(NAME_FlakShellDeaths, new FStat(true));
	Stats.Add(NAME_RocketDeaths, new FStat(true));
	Stats.Add(NAME_SniperDeaths, new FStat(true));
	Stats.Add(NAME_SniperHeadshotDeaths, new FStat(true));
	Stats.Add(NAME_RedeemerDeaths, new FStat(true));
	Stats.Add(NAME_InstagibDeaths, new FStat(true));
	Stats.Add(NAME_TelefragDeaths, new FStat(true));

	Stats.Add(NAME_EnforcerShots, new FStat(true));
	Stats.Add(NAME_BioRifleShots, new FStat(true));
	Stats.Add(NAME_ShockRifleShots, new FStat(true));
	Stats.Add(NAME_LinkShots, new FStat(true));
	Stats.Add(NAME_MinigunShots, new FStat(true));
	Stats.Add(NAME_FlakShots, new FStat(true));
	Stats.Add(NAME_RocketShots, new FStat(true));
	Stats.Add(NAME_SniperShots, new FStat(true));
	Stats.Add(NAME_RedeemerShots, new FStat(true));
	Stats.Add(NAME_InstagibShots, new FStat(true));

	Stats.Add(NAME_EnforcerHits, new FStat(true));
	Stats.Add(NAME_BioRifleHits, new FStat(true));
	Stats.Add(NAME_ShockRifleHits, new FStat(true));
	Stats.Add(NAME_LinkHits, new FStat(true));
	Stats.Add(NAME_MinigunHits, new FStat(true));
	Stats.Add(NAME_FlakHits, new FStat(true));
	Stats.Add(NAME_RocketHits, new FStat(true));
	Stats.Add(NAME_SniperHits, new FStat(true));
	Stats.Add(NAME_RedeemerHits, new FStat(true));
	Stats.Add(NAME_InstagibHits, new FStat(true));

	Stats.Add(NAME_UDamageTime, new FStat(true));
	Stats.Add(NAME_BerserkTime, new FStat(true));
	Stats.Add(NAME_InvisibilityTime, new FStat(true));
	Stats.Add(NAME_UDamageCount, new FStat(true));
	Stats.Add(NAME_BerserkCount, new FStat(true));
	Stats.Add(NAME_InvisibilityCount, new FStat(true));
	Stats.Add(NAME_BootJumps, new FStat(true));
	Stats.Add(NAME_ShieldBeltCount, new FStat(true));
	Stats.Add(NAME_ArmorVestCount, new FStat(true));
	Stats.Add(NAME_ArmorPadsCount, new FStat(true));
	Stats.Add(NAME_HelmetCount, new FStat(true));

	Stats.Add(NAME_BestShockCombo, new FStat(true));
	Stats.Add(NAME_AmazingCombos, new FStat(true));
	Stats.Add(NAME_AirRox, new FStat(true));
	Stats.Add(NAME_FlakShreds, new FStat(true));
	Stats.Add(NAME_AirSnot, new FStat(true));

	Stats.Add(NAME_RunDist, new FStat(true));
	Stats.Add(NAME_SprintDist, new FStat(true));
	Stats.Add(NAME_InAirDist, new FStat(true));
	Stats.Add(NAME_SwimDist, new FStat(true));
	Stats.Add(NAME_TranslocDist, new FStat(true));
	Stats.Add(NAME_NumDodges, new FStat(true));
	Stats.Add(NAME_NumWallDodges, new FStat(true));
	Stats.Add(NAME_NumJumps, new FStat(true));
	Stats.Add(NAME_NumLiftJumps, new FStat(true));
	Stats.Add(NAME_NumFloorSlides, new FStat(true));
	Stats.Add(NAME_NumWallRuns, new FStat(true));
	Stats.Add(NAME_NumImpactJumps, new FStat(true));
	Stats.Add(NAME_NumRocketJumps, new FStat(true));
	Stats.Add(NAME_SlideDist, new FStat(true));
	Stats.Add(NAME_WallRunDist, new FStat(true));

	Stats.Add(NAME_FlagHeldDeny, new FStat(true));
	Stats.Add(NAME_FlagHeldDenyTime, new FStat(true));
	Stats.Add(NAME_FlagHeldTime, new FStat(true));
	Stats.Add(NAME_FlagReturnPoints, new FStat(true));
	Stats.Add(NAME_CarryAssist, new FStat(true));
	Stats.Add(NAME_CarryAssistPoints, new FStat(true));
	Stats.Add(NAME_FlagCapPoints, new FStat(true));
	Stats.Add(NAME_DefendAssist, new FStat(true));
	Stats.Add(NAME_DefendAssistPoints, new FStat(true));
	Stats.Add(NAME_ReturnAssist, new FStat(true));
	Stats.Add(NAME_ReturnAssistPoints, new FStat(true));
	Stats.Add(NAME_TeamCapPoints, new FStat(true));
	Stats.Add(NAME_EnemyFCDamage, new FStat(true));
	Stats.Add(NAME_FCKills, new FStat(true));
	Stats.Add(NAME_FCKillPoints, new FStat(true));
	Stats.Add(NAME_FlagSupportKills, new FStat(true));
	Stats.Add(NAME_FlagSupportKillPoints, new FStat(true));
	Stats.Add(NAME_RegularKillPoints, new FStat(true));
	Stats.Add(NAME_FlagGrabs, new FStat(true));
	Stats.Add(NAME_AttackerScore, new FStat(true));
	Stats.Add(NAME_DefenderScore, new FStat(true));
	Stats.Add(NAME_SupporterScore, new FStat(true));
	
	Stats.Add(NAME_PlayerXP, new FStat(true));

	NumMatchesToKeep = 5;
	NumPreviousPlayerNamesToKeep = 5;

	JSONVersionNumber = 0;
}

UStatManager::~UStatManager()
{
	for (auto Stat = Stats.CreateIterator(); Stat; ++Stat)
	{
		delete Stat.Value();
	}
	Stats.Empty();
}

void FStat::ModifyStat(int32 Amount, EStatMod::Type ModType)
{
	if (ModType == EStatMod::Set)
	{
		StatData = Amount;
	}
	else if (ModType == EStatMod::Maximum)
	{
		StatData = FMath::Max(StatData, Amount);
	}
	else
	{
		if (Amount < 0 && StatData + Amount > StatData)
		{
			//Bind to MIN_int32
			StatData = MIN_int32;
		}
		//Attempted to add and the result is less than what we started at.
		else if (Amount > 0 && StatData + Amount < StatData)
		{
			//Bind to MAX_int32
			StatData = MAX_int32;
		}
		else
		{
			StatData = StatData + Amount;
		}
	}
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
	FStat* Stat = GetStatByName(StatName);
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

FStat* UStatManager::GetStatByName(FName StatName)
{
	return Stats.FindRef(StatName);
}

const FStat* UStatManager::GetStatByName(FName StatName) const
{
	return Stats.FindRef(StatName);
}

int32 UStatManager::GetStatValueByName(FName StatName) const
{
	const FStat* Stat = GetStatByName(StatName);
	if (Stat != NULL)
	{
		return Stat->StatData;
	}
	return 0;
}

int32 UStatManager::GetStatValue(const FStat *Stat) const
{
	if (Stat != NULL)
	{
		return Stat->StatData;
	}
	return 0;
}

void UStatManager::PopulateJsonObjectForBackendStats(TSharedPtr<FJsonObject> JsonObject, AUTPlayerState* PS)
{
	for (auto Stat = Stats.CreateConstIterator(); Stat; ++Stat)
	{
		if (Stat.Value()->bBackendStat && PS)
		{
			float NewStatValue = PS->GetStatsValue(Stat.Key());
			if (NewStatValue != 0)
			{
				JsonObject->SetNumberField(Stat.Key().ToString(), NewStatValue);
			}
		}
	}
}

void UStatManager::PopulateJsonObjectForNonBackendStats(TSharedPtr<FJsonObject> JsonObject)
{
	JsonObject->SetNumberField(TEXT("Version"), JSONVersionNumber);

	for (auto Stat = Stats.CreateConstIterator(); Stat; ++Stat)
	{
		if (Stat.Value()->bBackendStat)
		{
			JsonObject->SetNumberField(Stat.Key().ToString(), GetStatValue(Stat.Value()));
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
	for (auto Stat = Stats.CreateConstIterator(); Stat; ++Stat)
	{
		if (!Stat.Value()->bBackendStat)
		{
			int32 StatInput = 0;
			if (JsonObject->TryGetNumberField(Stat.Key().ToString(), StatInput))
			{
				Stat.Value()->ModifyStat(StatInput, EStatMod::Set);
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
		if (Cast<AUTPlayerState>((*ActivePlayerStates)[i]) != nullptr)
		{
			const AUTPlayerState& PlayerState = *(Cast<AUTPlayerState>((*ActivePlayerStates)[i]));
			if (!PlayerState.bOnlySpectator)
			{
				int32 NewIndex = NewMatchStats.Players.AddZeroed();
				FMatchStatsPlayer& MatchPlayer = NewMatchStats.Players[NewIndex];

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
	}

	for (int32 i = 0; i < InactivePlayerStates->Num(); i++)
	{
		if (Cast<AUTPlayerState>((*InactivePlayerStates)[i]) != nullptr)
		{
			const AUTPlayerState& PlayerState = *(Cast<AUTPlayerState>((*InactivePlayerStates)[i]));
			if (!PlayerState.bOnlySpectator)
			{
				int32 NewIndex = NewMatchStats.Players.AddZeroed();
				FMatchStatsPlayer& MatchPlayer = NewMatchStats.Players[NewIndex];

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
	}

	MatchStats.Add(NewMatchStats);
}