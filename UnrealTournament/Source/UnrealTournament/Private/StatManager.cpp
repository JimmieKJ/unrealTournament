// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "StatNames.h"
#include "UTCTFScoring.h"

DEFINE_LOG_CATEGORY(LogGameStats);

UStatManager::UStatManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Stats.Add(NAME_MatchesPlayed, new FStat());
	Stats.Add(NAME_MatchesQuit, new FStat());
	Stats.Add(NAME_TimePlayed, new FStat());
	Stats.Add(NAME_Wins, new FStat());
	Stats.Add(NAME_Losses, new FStat());
	Stats.Add(NAME_Kills, new FStat());
	Stats.Add(NAME_Deaths, new FStat());
	Stats.Add(NAME_Suicides, new FStat());
	
	Stats.Add(NAME_MultiKillLevel0, new FStat(FNewKillAwardXP(5)));
	Stats.Add(NAME_MultiKillLevel1, new FStat(FNewKillAwardXP(7)));
	Stats.Add(NAME_MultiKillLevel2, new FStat(FNewKillAwardXP(9)));
	Stats.Add(NAME_MultiKillLevel3, new FStat(FNewKillAwardXP(10)));

	Stats.Add(NAME_SpreeKillLevel0, new FStat(FNewKillAwardXP(10)));
	Stats.Add(NAME_SpreeKillLevel1, new FStat(FNewKillAwardXP(15)));
	Stats.Add(NAME_SpreeKillLevel2, new FStat(FNewKillAwardXP(20)));
	Stats.Add(NAME_SpreeKillLevel3, new FStat(FNewKillAwardXP(25)));
	Stats.Add(NAME_SpreeKillLevel4, new FStat(FNewKillAwardXP(30)));

	Stats.Add(NAME_ImpactHammerKills, new FStat());
	Stats.Add(NAME_EnforcerKills, new FStat());
	Stats.Add(NAME_BioRifleKills, new FStat());
	Stats.Add(NAME_ShockBeamKills, new FStat());
	Stats.Add(NAME_ShockCoreKills, new FStat());
	Stats.Add(NAME_ShockComboKills, new FStat());
	Stats.Add(NAME_LinkKills, new FStat());
	Stats.Add(NAME_LinkBeamKills, new FStat());
	Stats.Add(NAME_MinigunKills, new FStat());
	Stats.Add(NAME_MinigunShardKills, new FStat());
	Stats.Add(NAME_FlakShardKills, new FStat());
	Stats.Add(NAME_FlakShellKills, new FStat());
	Stats.Add(NAME_RocketKills, new FStat());
	Stats.Add(NAME_SniperKills, new FStat());
	Stats.Add(NAME_SniperHeadshotKills, new FStat(FNewKillAwardXP(3)));
	Stats.Add(NAME_RedeemerKills, new FStat());
	Stats.Add(NAME_InstagibKills, new FStat());
	Stats.Add(NAME_TelefragKills, new FStat());

	Stats.Add(NAME_ImpactHammerDeaths, new FStat());
	Stats.Add(NAME_EnforcerDeaths, new FStat());
	Stats.Add(NAME_BioRifleDeaths, new FStat());
	Stats.Add(NAME_ShockBeamDeaths, new FStat());
	Stats.Add(NAME_ShockCoreDeaths, new FStat());
	Stats.Add(NAME_ShockComboDeaths, new FStat());
	Stats.Add(NAME_LinkDeaths, new FStat());
	Stats.Add(NAME_LinkBeamDeaths, new FStat());
	Stats.Add(NAME_MinigunDeaths, new FStat());
	Stats.Add(NAME_MinigunShardDeaths, new FStat());
	Stats.Add(NAME_FlakShardDeaths, new FStat());
	Stats.Add(NAME_FlakShellDeaths, new FStat());
	Stats.Add(NAME_RocketDeaths, new FStat());
	Stats.Add(NAME_SniperDeaths, new FStat());
	Stats.Add(NAME_SniperHeadshotDeaths, new FStat());
	Stats.Add(NAME_RedeemerDeaths, new FStat());
	Stats.Add(NAME_InstagibDeaths, new FStat());
	Stats.Add(NAME_TelefragDeaths, new FStat());

	Stats.Add(NAME_EnforcerShots, new FStat());
	Stats.Add(NAME_BioRifleShots, new FStat());
	Stats.Add(NAME_ShockRifleShots, new FStat());
	Stats.Add(NAME_LinkShots, new FStat());
	Stats.Add(NAME_MinigunShots, new FStat());
	Stats.Add(NAME_FlakShots, new FStat());
	Stats.Add(NAME_RocketShots, new FStat());
	Stats.Add(NAME_SniperShots, new FStat());
	Stats.Add(NAME_RedeemerShots, new FStat());
	Stats.Add(NAME_InstagibShots, new FStat());

	// Hits can be fractional, multiply by 100 to make sure that we don't lose much precision when going to integers
	Stats.Add(NAME_EnforcerHits, new FStat(100.0f));
	Stats.Add(NAME_BioRifleHits, new FStat(100.0f));
	Stats.Add(NAME_ShockRifleHits, new FStat(100.0f));
	Stats.Add(NAME_LinkHits, new FStat(100.0f));
	Stats.Add(NAME_MinigunHits, new FStat(100.0f));
	Stats.Add(NAME_FlakHits, new FStat(100.0f));
	Stats.Add(NAME_RocketHits, new FStat(100.0f));
	Stats.Add(NAME_SniperHits, new FStat(100.0f));
	Stats.Add(NAME_RedeemerHits, new FStat(100.0f));
	Stats.Add(NAME_InstagibHits, new FStat(100.0f));

	Stats.Add(NAME_UDamageTime, new FStat());
	Stats.Add(NAME_BerserkTime, new FStat());
	Stats.Add(NAME_InvisibilityTime, new FStat());
	Stats.Add(NAME_UDamageCount, new FStat());
	Stats.Add(NAME_BerserkCount, new FStat());
	Stats.Add(NAME_InvisibilityCount, new FStat());
	Stats.Add(NAME_KegCount, new FStat());
	Stats.Add(NAME_BootJumps, new FStat());
	Stats.Add(NAME_ShieldBeltCount, new FStat());
	Stats.Add(NAME_ArmorVestCount, new FStat());
	Stats.Add(NAME_ArmorPadsCount, new FStat());
	Stats.Add(NAME_HelmetCount, new FStat());

	Stats.Add(NAME_BestShockCombo, new FStat());
	Stats.Add(NAME_AmazingCombos, new FStat(FNewKillAwardXP(3)));
	Stats.Add(NAME_AirRox, new FStat(FNewKillAwardXP(3)));
	Stats.Add(NAME_FlakShreds, new FStat(FNewKillAwardXP(3)));
	Stats.Add(NAME_AirSnot, new FStat(FNewKillAwardXP(3)));

	Stats.Add(NAME_RunDist, new FStat());
	Stats.Add(NAME_SprintDist, new FStat());
	Stats.Add(NAME_InAirDist, new FStat());
	Stats.Add(NAME_SwimDist, new FStat());
	Stats.Add(NAME_TranslocDist, new FStat());
	Stats.Add(NAME_NumDodges, new FStat());
	Stats.Add(NAME_NumWallDodges, new FStat());
	Stats.Add(NAME_NumJumps, new FStat());
	Stats.Add(NAME_NumLiftJumps, new FStat());
	Stats.Add(NAME_NumFloorSlides, new FStat());
	Stats.Add(NAME_NumWallRuns, new FStat());
	Stats.Add(NAME_NumImpactJumps, new FStat());
	Stats.Add(NAME_NumRocketJumps, new FStat());
	Stats.Add(NAME_SlideDist, new FStat());
	Stats.Add(NAME_WallRunDist, new FStat());

	Stats.Add(NAME_FlagCaptures, new FStat());
	Stats.Add(NAME_FlagReturns, new FStat());
	Stats.Add(NAME_FlagAssists, new FStat());
	Stats.Add(NAME_FlagHeldDeny, new FStat());
	Stats.Add(NAME_FlagHeldDenyTime, new FStat());
	Stats.Add(NAME_FlagHeldTime, new FStat());
	Stats.Add(NAME_FlagReturnPoints, new FStat());
	Stats.Add(NAME_CarryAssist, new FStat(FNewOffenseXP(5)));
	Stats.Add(NAME_CarryAssistPoints, new FStat());
	Stats.Add(NAME_FlagCapPoints, new FStat());
	Stats.Add(NAME_DefendAssist, new FStat(FNewDefenseXP(5)));
	Stats.Add(NAME_DefendAssistPoints, new FStat());
	Stats.Add(NAME_ReturnAssist, new FStat(FNewDefenseXP(5)));
	Stats.Add(NAME_ReturnAssistPoints, new FStat());
	Stats.Add(NAME_TeamCapPoints, new FStat());
	Stats.Add(NAME_EnemyFCDamage, new FStat());
	Stats.Add(NAME_FCKills, new FStat(FNewDefenseXP(5)));
	Stats.Add(NAME_FCKillPoints, new FStat());
	Stats.Add(NAME_FlagSupportKills, new FStat(FNewOffenseXP(3)));
	Stats.Add(NAME_FlagSupportKillPoints, new FStat());
	Stats.Add(NAME_RegularKillPoints, new FStat());
	Stats.Add(NAME_FlagGrabs, new FStat(FNewOffenseXP(1)));
	Stats.Add(NAME_AttackerScore, new FStat());
	Stats.Add(NAME_DefenderScore, new FStat());
	Stats.Add(NAME_SupporterScore, new FStat());
	
	Stats.Add(NAME_PlayerXP, new FStat());

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
		if (PS)
		{
			float NewStatValue = PS->GetStatsValue(Stat.Key());
			if (Stat.Value()->WriteMultiplier > 0.0f)
			{
				NewStatValue *= Stat.Value()->WriteMultiplier;
			}

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
			MatchStatJson->SetStringField(TEXT("MapName"), MatchStats[MatchIdx].MapName);

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
			MatchJsonValue->AsObject()->TryGetStringField(TEXT("MapName"), MatchStats[MatchIdx].MapName);

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

void UStatManager::AddMatchToStats(const FString& MapName, const FString& GameType, const TArray<class AUTTeamInfo*>* Teams, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates)
{
	// Keep match stats at 5
	if (MatchStats.Num() >= NumMatchesToKeep)
	{
		MatchStats.RemoveAt(0, MatchStats.Num() - NumMatchesToKeep + 1);
	}

	FMatchStats NewMatchStats;

	NewMatchStats.MapName = MapName;
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