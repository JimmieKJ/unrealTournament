// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "Net/UnrealNetwork.h"
#include "UTCTFScoring.h"
#include "StatNames.h"

AUTCTFGameState::AUTCTFGameState(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bSecondHalf = false;
	bHalftime = false;
	HalftimeScoreDelay = 2.f;

	GameScoreStats.Add(NAME_RegularKillPoints);
	GameScoreStats.Add(NAME_FCKills);
	GameScoreStats.Add(NAME_FCKillPoints);
	GameScoreStats.Add(NAME_FlagSupportKills);
	GameScoreStats.Add(NAME_FlagSupportKillPoints);
	GameScoreStats.Add(NAME_EnemyFCDamage);
	GameScoreStats.Add(NAME_FlagHeldDeny);
	GameScoreStats.Add(NAME_FlagHeldDenyTime);
	GameScoreStats.Add(NAME_FlagHeldTime);
	GameScoreStats.Add(NAME_FlagReturnPoints);
	GameScoreStats.Add(NAME_CarryAssist);
	GameScoreStats.Add(NAME_CarryAssistPoints);
	GameScoreStats.Add(NAME_FlagCapPoints);
	GameScoreStats.Add(NAME_DefendAssist);
	GameScoreStats.Add(NAME_DefendAssistPoints);
	GameScoreStats.Add(NAME_ReturnAssist);
	GameScoreStats.Add(NAME_ReturnAssistPoints);
	GameScoreStats.Add(NAME_TeamCapPoints);
	GameScoreStats.Add(NAME_FlagGrabs);

	TeamStats.Add(NAME_TeamFlagGrabs);
	TeamStats.Add(NAME_TeamFlagHeldTime);

	SecondaryAttackerStat = NAME_FlagHeldTime;

	HighlightMap.Add(HighlightNames::TopScorerRed, NSLOCTEXT("AUTGameMode", "HighlightTopScoreRed", "Red Team Top Score with <UT.MatchSummary.HighlightText.Value>{0}</> points."));
	HighlightMap.Add(HighlightNames::TopScorerBlue, NSLOCTEXT("AUTGameMode", "HighlightTopScoreBlue", "Blue Team Top Score with <UT.MatchSummary.HighlightText.Value>{0}</> points."));
	HighlightMap.Add(HighlightNames::TopFlagCapturesRed, NSLOCTEXT("AUTGameMode", "TopFlagCapturesRed", "Most Flag Caps for Red with <UT.MatchSummary.HighlightText.Value>{0}</>."));
	HighlightMap.Add(HighlightNames::TopFlagCapturesBlue, NSLOCTEXT("AUTGameMode", "TopFlagCapturesBlue", "Most Flag Caps for Blue with <UT.MatchSummary.HighlightText.Value>{0}</>."));
	HighlightMap.Add(HighlightNames::TopAssistsRed, NSLOCTEXT("AUTGameMode", "TopAssistsRed", "Most Assists for Red with <UT.MatchSummary.HighlightText.Value>{0}</>."));
	HighlightMap.Add(HighlightNames::TopAssistsRed, NSLOCTEXT("AUTGameMode", "TopAssistsRed", "Most Assists for Blue with <UT.MatchSummary.HighlightText.Value>{0}</>."));
	HighlightMap.Add(HighlightNames::TopFlagReturnsRed, NSLOCTEXT("AUTGameMode", "TopFlagReturnsRed", "Most Flag Returns for Red with <UT.MatchSummary.HighlightText.Value>{0}</>."));
	HighlightMap.Add(HighlightNames::TopFlagReturnsBlue, NSLOCTEXT("AUTGameMode", "TopFlagReturnsBlue", "Most Flag Returns for Blue with <UT.MatchSummary.HighlightText.Value>{0}</>."));

	HighlightMap.Add(NAME_FCKills, NSLOCTEXT("AUTGameMode", "FCKills", "Killed Enemy Flag Carrier (<UT.MatchSummary.HighlightText.Value>{0}</>)."));
	HighlightMap.Add(NAME_FlagGrabs, NSLOCTEXT("AUTGameMode", "FlagGrabs", "Grabbed Enemy Flag (<UT.MatchSummary.HighlightText.Value>{0}</>)."));
	HighlightMap.Add(NAME_FlagSupportKills, NSLOCTEXT("AUTGameMode", "FlagSupportKills", "Killed Enemy chasing Flag Carrier (<UT.MatchSummary.HighlightText.Value>{0}</>)."));
	HighlightMap.Add(HighlightNames::FlagCaptures, NSLOCTEXT("AUTGameMode", "FlagCaptures", "Captured Flag (<UT.MatchSummary.HighlightText.Value>{0}</>)."));
	HighlightMap.Add(HighlightNames::Assists, NSLOCTEXT("AUTGameMode", "Assists", "Assisted Flag Capture (<UT.MatchSummary.HighlightText.Value>{0}</>)."));
	HighlightMap.Add(HighlightNames::FlagReturns, NSLOCTEXT("AUTGameMode", "FlagReturns", "Returned Flag (<UT.MatchSummary.HighlightText.Value>{0}</>)."));
}

void AUTCTFGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTCTFGameState, bSecondHalf);
	DOREPLIFETIME(AUTCTFGameState, bHalftime);
	DOREPLIFETIME(AUTCTFGameState, FlagBases);
	DOREPLIFETIME(AUTCTFGameState, bPlayingAdvantage);
	DOREPLIFETIME(AUTCTFGameState, AdvantageTeamIndex);
	DOREPLIFETIME(AUTCTFGameState, ScoringPlays);
}

void AUTCTFGameState::SetMaxNumberOfTeams(int32 TeamCount)
{
	for (int32 TeamIdx = 0; TeamIdx < TeamCount; TeamIdx++)
	{
		FlagBases.Add(NULL);
	}
}

void AUTCTFGameState::CacheFlagBase(AUTCTFFlagBase* BaseToCache)
{
	if (BaseToCache->MyFlag != NULL)
	{
		uint8 TeamNum = BaseToCache->MyFlag->GetTeamNum();
		if (TeamNum < FlagBases.Num())
		{
			FlagBases[TeamNum] = BaseToCache;
		}
		else
		{
			UE_LOG(UT,Warning,TEXT("Found a Flag Base with TeamNum of %i that was unexpected (%i)"), TeamNum, FlagBases.Num());
		}
	}
	else
	{
		UE_LOG(UT,Warning,TEXT("Found a Flag Base (%s) without a flag"), *GetNameSafe(BaseToCache));
	}
}

float AUTCTFGameState::GetClockTime()
{
	if (IsMatchInOvertime())
	{
		return ElapsedTime - OvertimeStartTime;
	}
	return (TimeLimit > 0.f) ? RemainingTime : ElapsedTime;
}

void AUTCTFGameState::OnRep_MatchState()
{
	Super::OnRep_MatchState();

	//Make sure the timers are cleared since advantage-time may have been counting down
	if (IsMatchInOvertime())
	{
		OvertimeStartTime = ElapsedTime;
		RemainingTime = 0;
	}
}

AUTTeamInfo* AUTCTFGameState::FindLeadingTeam()
{
	AUTTeamInfo* WinningTeam = NULL;
	bool bTied;

	if (Teams.Num() > 0)
	{
		WinningTeam = Teams[0];
		bTied = false;
		for (int32 i=1;i<Teams.Num();i++)
		{
			if (Teams[i]->Score == WinningTeam->Score)
			{
				bTied = true;
			}
			else if (Teams[i]->Score > WinningTeam->Score)
			{
				WinningTeam = Teams[i];
				bTied = false;
			}
		}

		if (bTied) WinningTeam = NULL;
	}
	return WinningTeam;	
}

FName AUTCTFGameState::GetFlagState(uint8 TeamNum)
{
	if (TeamNum < FlagBases.Num() && FlagBases[TeamNum] != NULL)
	{
		return FlagBases[TeamNum]->GetFlagState();
	}
	return NAME_None;
}

AUTPlayerState* AUTCTFGameState::GetFlagHolder(uint8 TeamNum)
{
	if (TeamNum < FlagBases.Num() && FlagBases[TeamNum] != NULL)
	{
		return FlagBases[TeamNum]->GetCarriedObjectHolder();
	}
	return NULL;
}

AUTCTFFlagBase* AUTCTFGameState::GetFlagBase(uint8 TeamNum)
{
	if (TeamNum < FlagBases.Num())
	{
		return FlagBases[TeamNum];
	}
	return NULL;
}

void AUTCTFGameState::ResetFlags()
{
	for (int32 i=0; i < FlagBases.Num(); i++)
	{
		if (FlagBases[i] != NULL)
		{
			FlagBases[i]->RecallFlag();
		}
	}
}

bool AUTCTFGameState::IsMatchInProgress() const
{
	FName MatchState = GetMatchState();
	return (MatchState == MatchState::InProgress || MatchState == MatchState::MatchIsInOvertime || MatchState == MatchState::MatchIsAtHalftime ||
		MatchState == MatchState::MatchEnteringHalftime || MatchState == MatchState::MatchExitingHalftime ||
		MatchState == MatchState::MatchEnteringSuddenDeath || MatchState == MatchState::MatchIsInSuddenDeath);
}

bool AUTCTFGameState::IsMatchInOvertime() const
{
	FName MatchState = GetMatchState();
	return (MatchState == MatchState::MatchIsInOvertime || MatchState == MatchState::MatchEnteringSuddenDeath || MatchState == MatchState::MatchIsInSuddenDeath);
}


bool AUTCTFGameState::IsMatchAtHalftime() const
{
	FName MatchState = GetMatchState();
	return (MatchState == MatchState::MatchIsAtHalftime || MatchState == MatchState::MatchEnteringHalftime || MatchState == MatchState::MatchExitingHalftime);
}

bool AUTCTFGameState::IsMatchInSuddenDeath() const
{
	FName MatchState = GetMatchState();
	return (MatchState == MatchState::MatchEnteringSuddenDeath || MatchState == MatchState::MatchIsInSuddenDeath);
}

FName AUTCTFGameState::OverrideCameraStyle(APlayerController* PCOwner, FName CurrentCameraStyle)
{
	return (IsMatchAtHalftime() || HasMatchEnded()) ? FName(TEXT("FreeCam")) : Super::OverrideCameraStyle(PCOwner, CurrentCameraStyle);
}

void AUTCTFGameState::OnHalftimeChanged()
{
	if (bHalftime)
	{
		// delay toggling scoreboard
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCTFGameState::ToggleScoreboards, HalftimeScoreDelay);
	}
	else
	{
		ToggleScoreboards();
	}
}

void AUTCTFGameState::ToggleScoreboards()
{
	for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(It->PlayerController);
		if (PC != NULL)
		{
			PC->ClientToggleScoreboard(bHalftime);
		}
	}
}

void AUTCTFGameState::AddScoringPlay(const FCTFScoringPlay& NewScoringPlay)
{
	if (Role == ROLE_Authority)
	{
		ScoringPlays.AddUnique(NewScoringPlay);
	}
}

FText AUTCTFGameState::GetGameStatusText()
{
	if (HasMatchEnded())
	{
		return NSLOCTEXT("UTGameState", "PostGame", "Game Over");
	}
	else if (GetMatchState() == MatchState::MapVoteHappening)
	{
		return NSLOCTEXT("UTGameState", "Mapvote", "Map Vote");
	}
	else if (bPlayingAdvantage)
	{
		if (AdvantageTeamIndex == 0)
		{
			return NSLOCTEXT("UTCTFGameState", "RedAdvantage", "Red Advantage");
		}
		else
		{
			return NSLOCTEXT("UTCTFGameState", "BlueAdvantage", "Blue Advantage");
		}
	}
	else if (IsMatchAtHalftime()) 
	{
		return bSecondHalf ? NSLOCTEXT("UTCTFGameState", "PreOvertime", "Get Ready!") : NSLOCTEXT("UTCTFGameState", "HalfTime", "HalfTime");
	}
	else if (IsMatchInProgress())
	{
		if (IsMatchInOvertime())
		{
			return NSLOCTEXT("UTFGameState", "Overtime", "Overtime!");
		}

		return bSecondHalf ? NSLOCTEXT("UTCTFGameState", "SecondHalf", "Second Half") : NSLOCTEXT("UTCTFGameState", "FirstHalf", "First Half");
	}

	return Super::GetGameStatusText();
}

float AUTCTFGameState::ScoreCameraView(AUTPlayerState* InPS, AUTCharacter *Character)
{
	// bonus score to player near but not holding enemy flag
	if (InPS && Character && !InPS->CarriedObject && InPS->Team && (InPS->Team->GetTeamNum() < 2))
	{
		uint8 EnemyTeamNum = 1 - InPS->Team->GetTeamNum();
		AUTCTFFlag* EnemyFlag = FlagBases[EnemyTeamNum] ? FlagBases[EnemyTeamNum]->MyFlag : NULL;
		if (EnemyFlag && ((EnemyFlag->GetActorLocation() - Character->GetActorLocation()).Size() < FlagBases[EnemyTeamNum]->LastSecondSaveDistance))
		{
			float MaxScoreDist = FlagBases[EnemyTeamNum]->LastSecondSaveDistance;
			return FMath::Clamp(10.f * (MaxScoreDist - (EnemyFlag->GetActorLocation() - Character->GetActorLocation()).Size()) / MaxScoreDist, 0.f, 10.f);
		}
	}
	return 0.f;
}

uint8 AUTCTFGameState::NearestTeamSide(AActor* InActor)
{
	if (FlagBases.Num() > 1)
	{
		return ((InActor->GetActorLocation() - FlagBases[0]->GetActorLocation()).Size() < (InActor->GetActorLocation() - FlagBases[1]->GetActorLocation()).Size()) ? 0 : 1;
	}
	return 255;
}

bool AUTCTFGameState::GetImportantPickups_Implementation(TArray<AUTPickup*>& PickupList)
{
	Super::GetImportantPickups_Implementation(PickupList);
	TMap<UClass*, TArray<AUTPickup*> > PickupGroups;

	//Collect the Powerups without bOverride_TeamSide and group by class
	for (AUTPickup* Pickup : PickupList)
	{
		if (!Pickup->bOverride_TeamSide)
		{
			UClass* PickupClass = (Cast<AUTPickupInventory>(Pickup) != nullptr) ? *Cast<AUTPickupInventory>(Pickup)->GetInventoryType() : Pickup->GetClass();
			TArray<AUTPickup*>& PickupGroup = PickupGroups.FindOrAdd(PickupClass);
			PickupGroup.Add(Pickup);
		}
	}

	//Auto get the TeamSide
	if (FlagBases.Num() > 1)
	{
		for (auto& Pair : PickupGroups)
		{
			TArray<AUTPickup*>& PickupGroup = Pair.Value;

			//Find the midfield pickup for an odd number of pickups per group
			if (PickupGroup.Num() % 2 != 0 && PickupGroup.Num() > 2)
			{
				AUTPickup* MidfieldPickup = nullptr;
				float FarthestDist = 0.0;

				//Find the furthest pickup that would've been returned by NearestTeamSide()
				for (AUTPickup* Pickup : PickupGroup)
				{
					float ClosestFlagDist = MAX_FLT;
					for (AUTCTFFlagBase* Flag : FlagBases)
					{
						if (Flag != nullptr)
						{
							ClosestFlagDist = FMath::Min(ClosestFlagDist, FVector::Dist(Pickup->GetActorLocation(), Flag->GetActorLocation()));
						}
					}

					if (FarthestDist < ClosestFlagDist)
					{
						MidfieldPickup = Pickup;
						FarthestDist = ClosestFlagDist;
					}
				}

				if (MidfieldPickup != nullptr)
				{
					MidfieldPickup->TeamSide = 255;
				}
			}
		}
	}

	//Sort the list by team and by respawn time 
	//TODO: powerup priority so different armors sort properly
	PickupList.Sort([](const AUTPickup& A, const AUTPickup& B) -> bool
	{
		return A.TeamSide > B.TeamSide || (A.TeamSide == B.TeamSide && A.RespawnTime > B.RespawnTime);
	});

	return true;
}

void AUTCTFGameState::UpdateHighlights_Implementation()
{
	AUTPlayerState* TopScorer[2] = { NULL, NULL };
	AUTPlayerState* TopFlagCaps[2] = { NULL, NULL };
	AUTPlayerState* TopAssists[2] = { NULL, NULL };
	AUTPlayerState* TopFlagReturns[2] = { NULL, NULL };

	for (int32 i = 0; i < PlayerArray.Num() - 1; i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerArray[i]);
		if (PS && PS->Team)
		{
			int32 TeamIndex = PS->Team->TeamIndex;
			// @TODO FIXMESTEVE support tie scores!
			if (PS->Score >(TopScorer[TeamIndex] ? TopScorer[TeamIndex]->Score : 0))
			{
				TopScorer[TeamIndex] = PS;
			}
			if (PS->FlagCaptures > (TopFlagCaps[TeamIndex] ? TopFlagCaps[TeamIndex]->FlagCaptures : 0))
			{
				TopFlagCaps[TeamIndex] = PS;
			}
			if (PS->Assists > (TopAssists[TeamIndex] ? TopAssists[TeamIndex]->Assists : 0))
			{
				TopAssists[TeamIndex] = PS;
			}
			if (PS->FlagReturns > (TopFlagReturns[TeamIndex] ? TopFlagReturns[TeamIndex]->FlagReturns : 0))
			{
				TopFlagReturns[TeamIndex] = PS;
			}
		}
	}

	// FIXMESTEVE move to team game, with flag to turn off (for duel, etc.)
	if (TopScorer[0] == NULL)
	{
		if (TopScorer[1] != NULL)
		{
			TopScorer[1]->AddMatchHighlight(HighlightNames::TopScorerBlue, TopScorer[1]->Score);
		}
	}
	else if (TopScorer[1] == NULL)
	{
		if (TopScorer[0] != NULL)
		{
			TopScorer[0]->AddMatchHighlight(HighlightNames::TopScorerRed, TopScorer[0]->Score);
		}
	}
	else if (TopScorer[0]->Score == TopScorer[1]->Score)
	{
		TopScorer[0]->AddMatchHighlight(HighlightNames::TopScorerRed, TopScorer[0]->Score);
		TopScorer[1]->AddMatchHighlight(HighlightNames::TopScorerBlue, TopScorer[1]->Score);
	}
	else if (TopScorer[0]->Score > TopScorer[1]->Score)
	{
		TopScorer[1]->AddMatchHighlight(HighlightNames::TopScorerBlue, TopScorer[1]->Score);
	}
	else
	{
		TopScorer[0]->AddMatchHighlight(HighlightNames::TopScorerRed, TopScorer[0]->Score);
	}


	if (TopFlagCaps[0] != NULL)
	{
		TopFlagCaps[0]->AddMatchHighlight(HighlightNames::TopFlagCapturesRed, TopFlagCaps[0]->FlagCaptures);
	}
	if (TopFlagCaps[1] != NULL)
	{
		TopFlagCaps[1]->AddMatchHighlight(HighlightNames::TopFlagCapturesBlue, TopFlagCaps[1]->FlagCaptures);
	}
	if (TopAssists[0] != NULL)
	{
		TopAssists[0]->AddMatchHighlight(HighlightNames::TopAssistsRed, TopAssists[0]->Assists);
	}
	if (TopAssists[1] != NULL)
	{
		TopAssists[1]->AddMatchHighlight(HighlightNames::TopAssistsBlue, TopAssists[1]->Assists);
	}
	if (TopFlagReturns[0] != NULL)
	{
		TopFlagReturns[0]->AddMatchHighlight(HighlightNames::TopFlagReturnsRed, TopFlagReturns[0]->FlagReturns);
	}
	if (TopFlagReturns[1] != NULL)
	{
		TopFlagReturns[1]->AddMatchHighlight(HighlightNames::TopFlagReturnsBlue, TopFlagReturns[1]->FlagReturns);
	}

	// add flag results for non-top players
	for (int32 i = 0; i < PlayerArray.Num() - 1; i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerArray[i]);
		if (PS && PS->Team)
		{
			int32 TeamIndex = PS->Team->TeamIndex;
			if ((PS != TopFlagCaps[TeamIndex]) && (PS->FlagCaptures > 0))
			{
				PS->AddMatchHighlight(HighlightNames::FlagCaptures, PS->FlagCaptures);
			}
			if ((PS != TopAssists[TeamIndex]) && (PS->Assists > 0))
			{
				PS->AddMatchHighlight(HighlightNames::Assists, PS->Assists);
			}
			if ((PS != TopFlagReturns[TeamIndex]) && (PS->FlagReturns > 0))
			{
				PS->AddMatchHighlight(HighlightNames::FlagReturns, PS->FlagReturns);
			}
		}
	}

	Super::UpdateHighlights_Implementation();
}

void AUTCTFGameState::AddMinorHighlights_Implementation(AUTPlayerState* PS)
{
	// skip if already filled with major highlights
	if (PS->MatchHighlights[4] != NAME_None)
	{
		return;
	}

	if (PS->GetStatsValue(NAME_FCKills) > 0)
	{
		PS->AddMatchHighlight(NAME_FCKills, PS->GetStatsValue(NAME_FCKills));
		if (PS->MatchHighlights[4] != NAME_None)
		{
			return;
		}
	}
	if (PS->GetStatsValue(NAME_FlagGrabs) > 0)
	{
		PS->AddMatchHighlight(NAME_FlagGrabs, PS->GetStatsValue(NAME_FlagGrabs));
		if (PS->MatchHighlights[4] != NAME_None)
		{
			return;
		}
	}
	if (PS->GetStatsValue(NAME_FlagSupportKills) > 0)
	{
		PS->AddMatchHighlight(NAME_FlagSupportKills, PS->GetStatsValue(NAME_FlagSupportKills));
		if (PS->MatchHighlights[4] != NAME_None)
		{
			return;
		}
	}
	Super::AddMinorHighlights_Implementation(PS);
}
