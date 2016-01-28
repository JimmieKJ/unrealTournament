// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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
	bIsAtIntermission = false;
	HalftimeScoreDelay = 2.f;
	GoalScoreText = NSLOCTEXT("UTScoreboard", "CTFGoalScoreFormat", "First to {0} Caps");

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

	HighlightMap.Add(HighlightNames::TopFlagCapturesRed, NSLOCTEXT("AUTGameMode", "TopFlagCapturesRed", "Most Flag Caps for Red with <UT.MatchSummary.HighlightText.Value>{0}</>."));
	HighlightMap.Add(HighlightNames::TopFlagCapturesBlue, NSLOCTEXT("AUTGameMode", "TopFlagCapturesBlue", "Most Flag Caps for Blue with <UT.MatchSummary.HighlightText.Value>{0}</>."));
	HighlightMap.Add(HighlightNames::TopAssistsRed, NSLOCTEXT("AUTGameMode", "TopAssistsRed", "Most Assists for Red with <UT.MatchSummary.HighlightText.Value>{0}</>."));
	HighlightMap.Add(HighlightNames::TopAssistsBlue , NSLOCTEXT("AUTGameMode", "TopAssistsBlue", "Most Assists for Blue with <UT.MatchSummary.HighlightText.Value>{0}</>."));
	HighlightMap.Add(HighlightNames::TopFlagReturnsRed, NSLOCTEXT("AUTGameMode", "TopFlagReturnsRed", "Most Flag Returns for Red with <UT.MatchSummary.HighlightText.Value>{0}</>."));
	HighlightMap.Add(HighlightNames::TopFlagReturnsBlue, NSLOCTEXT("AUTGameMode", "TopFlagReturnsBlue", "Most Flag Returns for Blue with <UT.MatchSummary.HighlightText.Value>{0}</>."));

	HighlightMap.Add(NAME_FCKills, NSLOCTEXT("AUTGameMode", "FCKills", "Killed Enemy Flag Carrier (<UT.MatchSummary.HighlightText.Value>{0}</>)."));
	HighlightMap.Add(NAME_FlagGrabs, NSLOCTEXT("AUTGameMode", "FlagGrabs", "Grabbed Enemy Flag (<UT.MatchSummary.HighlightText.Value>{0}</>)."));
	HighlightMap.Add(NAME_FlagSupportKills, NSLOCTEXT("AUTGameMode", "FlagSupportKills", "Killed Enemy chasing Flag Carrier (<UT.MatchSummary.HighlightText.Value>{0}</>)."));
	HighlightMap.Add(HighlightNames::FlagCaptures, NSLOCTEXT("AUTGameMode", "FlagCaptures", "Captured Flag (<UT.MatchSummary.HighlightText.Value>{0}</>)."));
	HighlightMap.Add(HighlightNames::Assists, NSLOCTEXT("AUTGameMode", "Assists", "Assisted Flag Capture (<UT.MatchSummary.HighlightText.Value>{0}</>)."));
	HighlightMap.Add(HighlightNames::FlagReturns, NSLOCTEXT("AUTGameMode", "FlagReturns", "Returned Flag (<UT.MatchSummary.HighlightText.Value>{0}</>)."));

	ShortHighlightMap.Add(HighlightNames::TopFlagCapturesRed, NSLOCTEXT("AUTGameMode", "ShortTopFlagCapturesRed", "Most Flag Caps for Red"));
	ShortHighlightMap.Add(HighlightNames::TopFlagCapturesBlue, NSLOCTEXT("AUTGameMode", "ShortTopFlagCapturesBlue", "Most Flag Caps for Blue"));
	ShortHighlightMap.Add(HighlightNames::TopAssistsRed, NSLOCTEXT("AUTGameMode", "ShortTopAssistsRed", "Most Assists for Red"));
	ShortHighlightMap.Add(HighlightNames::TopAssistsBlue, NSLOCTEXT("AUTGameMode", "ShortTopAssistsBlue", "Most Assists for Blue"));
	ShortHighlightMap.Add(HighlightNames::TopFlagReturnsRed, NSLOCTEXT("AUTGameMode", "ShortTopFlagReturnsRed", "Most Flag Returns for Red"));
	ShortHighlightMap.Add(HighlightNames::TopFlagReturnsBlue, NSLOCTEXT("AUTGameMode", "ShortTopFlagReturnsBlue", "Most Flag Returns for Blue"));

	ShortHighlightMap.Add(NAME_FCKills, NSLOCTEXT("AUTGameMode", "ShortFCKills", "{0} Enemy Flag Carrier Kills"));
	ShortHighlightMap.Add(NAME_FlagGrabs, NSLOCTEXT("AUTGameMode", "ShortFlagGrabs", "{0} Flag Grabs"));
	ShortHighlightMap.Add(NAME_FlagSupportKills, NSLOCTEXT("AUTGameMode", "ShortFlagSupportKills", "Killed Enemy chasing Flag Carrier"));
	ShortHighlightMap.Add(HighlightNames::FlagCaptures, NSLOCTEXT("AUTGameMode", "ShortFlagCaptures", "{0} Flag Captures"));
	ShortHighlightMap.Add(HighlightNames::Assists, NSLOCTEXT("AUTGameMode", "ShortAssists", "{0} Assists"));
	ShortHighlightMap.Add(HighlightNames::FlagReturns, NSLOCTEXT("AUTGameMode", "ShortFlagReturns", "{0} Flag Returns"));

	HighlightPriority.Add(HighlightNames::TopFlagCapturesRed, 4.5f);
	HighlightPriority.Add(HighlightNames::TopFlagCapturesBlue, 4.5f);
	HighlightPriority.Add(HighlightNames::TopAssistsRed, 3.5f);
	HighlightPriority.Add(HighlightNames::TopAssistsRed, 3.5f);
	HighlightPriority.Add(HighlightNames::TopFlagReturnsRed, 3.3f);
	HighlightPriority.Add(HighlightNames::TopFlagReturnsBlue, 3.3f);
	HighlightPriority.Add(NAME_FCKills, 3.5f);
	HighlightPriority.Add(NAME_FlagGrabs, 1.5f);
	HighlightPriority.Add(NAME_FlagSupportKills, 2.5f);
	HighlightPriority.Add(HighlightNames::FlagCaptures, 3.5f);
	HighlightPriority.Add(HighlightNames::Assists, 3.f);
	HighlightPriority.Add(HighlightNames::FlagReturns, 1.9f);
}

void AUTCTFGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTCTFGameState, bSecondHalf);
	DOREPLIFETIME(AUTCTFGameState, bIsAtIntermission);
	DOREPLIFETIME(AUTCTFGameState, FlagBases);
	DOREPLIFETIME(AUTCTFGameState, bPlayingAdvantage);
	DOREPLIFETIME(AUTCTFGameState, AdvantageTeamIndex);
	DOREPLIFETIME(AUTCTFGameState, ScoringPlays);
	DOREPLIFETIME(AUTCTFGameState, CTFRound); 
	DOREPLIFETIME(AUTCTFGameState, RedLivesRemaining);
	DOREPLIFETIME(AUTCTFGameState, BlueLivesRemaining);
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
	return ((TimeLimit > 0.f) || !HasMatchStarted()) ? RemainingTime : ElapsedTime;
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
	return (MatchState == MatchState::InProgress || MatchState == MatchState::MatchIsInOvertime || MatchState == MatchState::MatchIntermission || MatchState == MatchState::MatchExitingIntermission);
}

bool AUTCTFGameState::IsMatchInOvertime() const
{
	FName MatchState = GetMatchState();
	return (MatchState == MatchState::MatchIsInOvertime);
}

bool AUTCTFGameState::IsMatchIntermission() const
{
	FName MatchState = GetMatchState();
	return (MatchState == MatchState::MatchIntermission) || (MatchState == MatchState::MatchIntermission || MatchState == MatchState::MatchExitingIntermission);
}

FName AUTCTFGameState::OverrideCameraStyle(APlayerController* PCOwner, FName CurrentCameraStyle)
{
	return (IsMatchIntermission() || HasMatchEnded()) ? FName(TEXT("FreeCam")) : Super::OverrideCameraStyle(PCOwner, CurrentCameraStyle);
}

void AUTCTFGameState::OnIntermissionChanged()
{
	if (bIsAtIntermission)
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
			PC->ClientToggleScoreboard(bIsAtIntermission);
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
	else if (CTFRound > 0)
	{
		FFormatNamedArguments Args;
		Args.Add("RoundNum", FText::AsNumber(CTFRound));
		return FText::Format(NSLOCTEXT("UTCharacter", "CTFRoundDisplay", "Round {RoundNum}"), Args);
	}
	else if (IsMatchIntermission())
	{
		return bSecondHalf ? NSLOCTEXT("UTCTFGameState", "PreOvertime", "Get Ready!") : NSLOCTEXT("UTCTFGameState", "HalfTime", "HalfTime");
	}
	else if (IsMatchInProgress())
	{
		if (IsMatchInOvertime())
		{
			return (ElapsedTime - OvertimeStartTime < TimeLimit) ? NSLOCTEXT("UTCTFGameState", "Overtime", "Overtime!") : NSLOCTEXT("UTCTFGameState", "ExtendedOvertime", "Extended Overtime!");
		}
		if (TimeLimit == 0)
		{
			return FText::GetEmpty();
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
		// if there is only one of this pickup, return 255
		bool bFoundAnother = false;
		AUTPickupInventory* InPickupInventory = Cast<AUTPickupInventory>(InActor);
		if (InPickupInventory)
		{
			for (FActorIterator It(GetWorld()); It; ++It)
			{
				AUTPickupInventory* PickupInventory = Cast<AUTPickupInventory>(*It);
				if (PickupInventory && (PickupInventory != InPickupInventory) && (PickupInventory->GetInventoryType() == InPickupInventory->GetInventoryType()))
				{
					bFoundAnother = true;
					break;
				}
			}
		}
		if (bFoundAnother)
		{
			float DistDiff = (InActor->GetActorLocation() - FlagBases[0]->GetActorLocation()).Size() - (InActor->GetActorLocation() - FlagBases[1]->GetActorLocation()).Size();
			return (DistDiff < 0) ? 0 : 1;
		}
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
	AUTPlayerState* TopFlagCaps[2] = { NULL, NULL };
	AUTPlayerState* TopAssists[2] = { NULL, NULL };
	AUTPlayerState* TopFlagReturns[2] = { NULL, NULL };

	for (TActorIterator<AUTPlayerState> It(GetWorld()); It; ++It)
	{
		AUTPlayerState* PS = *It;
		if (PS && !PS->bOnlySpectator)
		{
			int32 TeamIndex = PS->Team ? PS->Team->TeamIndex : 0;
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

	for (TActorIterator<AUTPlayerState> It(GetWorld()); It; ++It)
	{
		AUTPlayerState* PS = *It;
		if (PS && !PS->bOnlySpectator)
		{
			int32 TeamIndex = PS->Team ? PS->Team->TeamIndex : 0;
			if ((TopFlagCaps[TeamIndex] != NULL) && (PS->FlagCaptures == TopFlagCaps[TeamIndex]->FlagCaptures))
			{
				PS->AddMatchHighlight((TeamIndex == 0) ? HighlightNames::TopFlagCapturesRed : HighlightNames::TopFlagCapturesBlue, PS->FlagCaptures);
			}
			else if (PS->FlagCaptures > 0)
			{
				PS->AddMatchHighlight(HighlightNames::FlagCaptures, PS->FlagCaptures);
			}
			if ((TopAssists[TeamIndex] != NULL) && (PS->Assists == TopAssists[TeamIndex]->Assists))
			{
				PS->AddMatchHighlight((TeamIndex == 0) ? HighlightNames::TopAssistsRed : HighlightNames::TopAssistsBlue, PS->Assists);
			}
			else if (PS->Assists > 0)
			{
				PS->AddMatchHighlight(HighlightNames::Assists, PS->Assists);
			}
			if ((TopFlagReturns[TeamIndex] != NULL) && (PS->FlagReturns == TopFlagReturns[TeamIndex]->FlagReturns))
			{
				PS->AddMatchHighlight((TeamIndex == 0) ? HighlightNames::TopFlagReturnsRed : HighlightNames::TopFlagReturnsBlue, PS->FlagReturns);
			}
			else if (PS->FlagReturns > 0)
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
	if (PS->MatchHighlights[3] != NAME_None)
	{
		return;
	}

	if (PS->GetStatsValue(NAME_FCKills) > 0)
	{
		PS->AddMatchHighlight(NAME_FCKills, PS->GetStatsValue(NAME_FCKills));
		if (PS->MatchHighlights[3] != NAME_None)
		{
			return;
		}
	}
	Super::AddMinorHighlights_Implementation(PS);
	if (PS->MatchHighlights[3] != NAME_None)
	{
		return;
	}

	if (PS->GetStatsValue(NAME_FlagGrabs) > 0)
	{
		PS->AddMatchHighlight(NAME_FlagGrabs, PS->GetStatsValue(NAME_FlagGrabs));
		if (PS->MatchHighlights[3] != NAME_None)
		{
			return;
		}
	}
	if (PS->GetStatsValue(NAME_FlagSupportKills) > 0)
	{
		PS->AddMatchHighlight(NAME_FlagSupportKills, PS->GetStatsValue(NAME_FlagSupportKills));
		if (PS->MatchHighlights[3] != NAME_None)
		{
			return;
		}
	}
}
