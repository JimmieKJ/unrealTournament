// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTCTFGameMode.h"
#include "UTFirstBloodMessage.h"

AUTCTFGameMode::AUTCTFGameMode(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	// By default, we do 2 team CTF
	MaxNumberOfTeams = 2;
	HalftimeDuration = 15;	// 15 second half-time by default...
	HUDClass = AUTHUD_CTF::StaticClass();
	GameStateClass = AUTCTFGameState::StaticClass();
	bAllowOvertime=true;
	bOldSchool = false;
	OvertimeDuration=5;
	bUseTeamStarts = true;

	//Add the translocator here for now :(
	static ConstructorHelpers::FObjectFinder<UClass> WeapTranslocator(TEXT("Blueprint'/Game/UserContent/Translocator/BP_Translocator.BP_Translocator_C'"));
	DefaultInventory.Add(WeapTranslocator.Object);
}

void AUTCTFGameMode::InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage )
{
	Super::InitGame(MapName, Options, ErrorMessage);

	FString InOpt = ParseOption(Options, TEXT("OldSchool"));
	bOldSchool = EvalBoolOptions(InOpt, bOldSchool);

	// HalftimeDuration is in seconds and used in seconds,
	HalftimeDuration = FMath::Max(1, GetIntOption( Options, TEXT("HalftimeDuration"), HalftimeDuration));	

	// OvertimeDuration is in minutes
	OvertimeDuration = FMath::Max(1, GetIntOption( Options, TEXT("OvertimeDuration"), OvertimeDuration));
	OvertimeDuration *= 60;
}

void AUTCTFGameMode::InitGameState()
{
	Super::InitGameState();
	// Store a cached reference to the GameState
	CTFGameState = Cast<AUTCTFGameState>(GameState);
	CTFGameState->SetMaxNumberOfTeams(MaxNumberOfTeams);
}


void AUTCTFGameMode::ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason)
{
	if (Holder != NULL && Holder->Team != NULL)
	{
		float DistanceFromHome = (GameObject->GetActorLocation() - CTFGameState->FlagBases[GameObject->GetTeamNum()]->GetActorLocation()).SizeSquared();
		float DistanceFromScore = (GameObject->GetActorLocation() - CTFGameState->FlagBases[1 - GameObject->GetTeamNum()]->GetActorLocation()).SizeSquared();

		UE_LOG(UT,Log,TEXT("========================================="));
		UE_LOG(UT,Log,TEXT("Flag Score by %s - Reason: %s"), *Holder->PlayerName, *Reason.ToString());

		if ( Reason == FName("SentHome") )
		{
			for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
			{
				AController* C = *Iterator;
				AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
				if (PS != NULL && PS->GetTeamNum() == Holder->GetTeamNum())
				{
					if ( C->GetPawn() != NULL && (GameObject->GetActorLocation() - C->GetPawn()->GetActorLocation()).SizeSquared() <= FlagCombatBonusDistance)
					{
						if (PS == Holder)
						{
							uint32 Points = FlagReturnPoints;
							if (DistanceFromHome > DistanceFromScore)																																							
							{
								Points += FlagReturnEnemyZoneBonus;								
							}

							if (DistanceFromScore <= FlagDenialDistance)
							{
								Points += FlagReturnDenialBonus;
							}
							UE_LOG(UT,Log,TEXT("    Player %s received %i"), *PS->PlayerName, Points);
							PS->FlagReturns++;
							PS->AdjustScore(Points);
						}
						else
						{
							UE_LOG(UT,Log,TEXT("    Player %s received %i"), *PS->PlayerName, ProximityReturnBonus);
							PS->AdjustScore(ProximityReturnBonus);
						}
					}
				}
			}
		}

		else if (Reason == FName("FlagCapture"))
		{
			// Give the team a capture.
			Holder->Team->Score++;
			Holder->FlagCaptures++;

			// We have to count up since it's possible a player left the game...
			int AssistCount = 0;
			for (int i=0;i<GameObject->PreviousHolders.Num();i++)
			{
				if (GameObject->PreviousHolders[i] != NULL) AssistCount++;
			}

			uint32 ScorePerPlayer = FMath::Max<uint32>(1, FlagTotalScorePool / AssistCount);

			// NOTE: It's possible that the first player to pickup this flag might have left so first might be NULL.  Not
			// sure if we should then assign the points to the next in line so I'm ditching the points for now.
			for (int i=0;i<GameObject->PreviousHolders.Num();i++)
			{
				if (GameObject->PreviousHolders[i] != NULL)
				{
					int Points = ScorePerPlayer + (i == 0 ? FlagFirstPickupPoints : 0);
					UE_LOG(UT,Log,TEXT("    Assist Points for %s = %i"), *GameObject->PreviousHolders[i]->PlayerName, Points)
					GameObject->PreviousHolders[i]->AdjustScore(Points);
					if (GameObject->PreviousHolders[i] != Holder)
					{
						GameObject->PreviousHolders[i]->Assists++;
					}
				}
			}

			// Give out bonus points to all teammates near the flag.
			for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
			{
				AController* C = *Iterator;
				AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
				if (PS != NULL && PS->GetTeamNum() == Holder->GetTeamNum())
				{
					if (C->GetPawn()!= NULL && PS != Holder && (GameObject->GetActorLocation() - C->GetPawn()->GetActorLocation()).SizeSquared() <= FlagCombatBonusDistance) 
					{
						UE_LOG(UT,Log, TEXT("    Prox Bonus for %s = %i"), *PS->PlayerName, ProximityCapBonus);
						PS->AdjustScore(ProximityCapBonus);
					}
				}
			}
		}

		UE_LOG(UT,Log,TEXT("========================================="));

		if (CTFGameState->IsMatchInOvertime())
		{
			EndGame(Holder, FName(TEXT("GoldenCap")));	
		}
	}
}

bool AUTCTFGameMode::CheckScore(AUTPlayerState* Scorer)
{
	if (bOldSchool)
	{
		if ( Scorer->Team != NULL && GoalScore > 0 && Scorer->Team->Score >= GoalScore)
		{
			EndGame(Scorer,FName(TEXT("scorelimit")));
		}
	}

	return true;
}

bool AUTCTFGameMode::IsAWinner(AUTPlayerController* PC)
{
	return (PC->UTPlayerState->Team != NULL && UTGameState->WinningTeam != NULL && PC->UTPlayerState->Team == UTGameState->WinningTeam);
}

void AUTCTFGameMode::CheckGameTime()
{
	if (bOldSchool || TimeLimit == 0)
	{
		Super::CheckGameTime();
	}
	else if (CTFGameState->IsMatchInProgress())
	{
		if (!CTFGameState->bHalftime)
		{
			if (CTFGameState->RemainingTime <= 0)
			{
				if (CTFGameState->bSecondHalf)
				{
					if (!bAllowOvertime || UTGameState->IsMatchInOvertime() || CTFGameState->FindLeadingTeam() != NULL)
					{
						EndGame(NULL, FName(TEXT("TimeLimit")));	
					}
					else if (bAllowOvertime && !UTGameState->IsMatchInOvertime())
					{
						StartHalftime();
					}
				}		
				else 
				{
					StartHalftime();
				}
			}
		}
	}
}

void AUTCTFGameMode::StartHalftime()
{
	FreezePlayers();
	FocusOnBestPlayer();
	CTFGameState->bHalftime = true;
	CTFGameState->SetTimeLimit(HalftimeDuration);	// Reset the Game Clock for Halftime
	GetWorldTimerManager().SetTimer(this, &AUTCTFGameMode::HalftimeIsOver, HalftimeDuration, false);
}


void AUTCTFGameMode::HalftimeIsOver()
{
	if (CTFGameState->bSecondHalf)
	{
		CTFGameState->SetTimeLimit(OvertimeDuration);
		SetMatchState(MatchState::MatchEnteringOvertime);
	}
	else
	{
		CTFGameState->bSecondHalf = true;
		CTFGameState->SetTimeLimit(TimeLimit);		// Reset the GameClock for the second time.
	}

	// Reset all players

	//now respawn all the players
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState != NULL && !Controller->PlayerState->bOnlySpectator)
		{
			if (Controller->GetPawn() != NULL)
			{
				AUTCharacter* Char = Cast<AUTCharacter>(Controller->GetPawn());
				if (Char != NULL)
				{
					Char->Died(NULL,FDamageEvent(UUTDamageType::StaticClass()));
				}
				
			}
			RestartPlayer(Controller);
		}
	}

	// Send all flags home..

	CTFGameState->ResetFlags();
	CTFGameState->bHalftime = false;
}

void AUTCTFGameMode::GameObjectiveInitialized(AUTGameObjective* Obj)
{
	AUTCTFFlagBase* FlagBase = Cast<AUTCTFFlagBase>(Obj);
	if (FlagBase != NULL && FlagBase->MyFlag)
	{
		CTFGameState->CacheFlagBase(FlagBase);
	}
}

void AUTCTFGameMode::FreezePlayers()
{
	for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator )
	{
		(*Iterator)->TurnOff();
	}

}

void AUTCTFGameMode::FocusOnBestPlayer()
{
	// Init targets
	TArray<AUTCharacter*> BestPlayers;
	for (int i=0;i<Teams.Num();i++)
	{
		BestPlayers.Add(NULL);
	}

	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		if (Controller->GetPawn() != NULL && Controller->PlayerState != NULL)
		{
			AUTCharacter* Char = Cast<AUTCharacter>(Controller->GetPawn());
			if (Char != NULL)
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(Controller->PlayerState);
				uint8 TeamNum = PS->GetTeamNum();
				if (TeamNum < BestPlayers.Num())
				{
					if (BestPlayers[TeamNum] == NULL || PS->Score > BestPlayers[TeamNum]->PlayerState->Score || Cast<AUTCTFFlag>(PS->CarriedObject) != NULL)
					{
						BestPlayers[TeamNum] = Char;
					}
				}
			}
		}
	}	

	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
		if (PC != NULL)
		{
			uint8 TeamNum = PC->GetTeamNum();
			AActor* BestTarget = NULL;
			if (BestPlayers[TeamNum] != NULL)
			{
				BestTarget = BestPlayers[TeamNum];
			}
			PC->SetViewTarget(BestTarget);
		}
	}
}

void AUTCTFGameMode::RestartPlayer(AController* aPlayer)
{
	if (CTFGameState != NULL && CTFGameState->bHalftime) return;
	Super::RestartPlayer(aPlayer);
}

bool AUTCTFGameMode::IsCloseToFlagCarrier(AActor* Who, float CheckDistanceSquared, uint8 TeamNum)
{
	if (CTFGameState == NULL || CTFGameState->FlagBases.Num() < 2)
	{
		AUTCTFFlag* Flags[2];
		Flags[0] = CTFGameState->FlagBases[0]->MyFlag;
		Flags[1] = CTFGameState->FlagBases[1]->MyFlag;

		BOOL bCloseToRed = Flags[0]->ObjectState == CarriedObjectState::Held && (Flags[0]->GetActorLocation() - Who->GetActorLocation()).SizeSquared() <= CheckDistanceSquared;
		BOOL bCloseToBlue = Flags[1]->ObjectState == CarriedObjectState::Held && (Flags[1]->GetActorLocation() - Who->GetActorLocation()).SizeSquared() <= CheckDistanceSquared;

		if ( (TeamNum == 0 && bCloseToRed) || (TeamNum == 1 && bCloseToBlue) ||
				(TeamNum == 255 && (bCloseToRed || bCloseToBlue)) )
		{
			return true;
		}
	}

	return false;
}


void AUTCTFGameMode::ScoreDamage(int DamageAmount, AController* Victim, AController* Attacker)
{
	Super::ScoreDamage(DamageAmount, Victim, Attacker);
	if (Attacker == NULL) return;
	
	AUTPlayerState* AttackerPS = Cast<AUTPlayerState>(Attacker->PlayerState);

	if (AttackerPS != NULL)
	{
		int AdjustedDamageAmount = FMath::Clamp<int>(DamageAmount,0,100);
		if (Attacker != Victim)
		{
			if (Attacker->GetPawn() != NULL && IsCloseToFlagCarrier(Attacker->GetPawn(), FlagCombatBonusDistance, 255))
			{
				AdjustedDamageAmount *= FlagCarrierCombatMultiplier;
			}
			AttackerPS->AdjustScore(AdjustedDamageAmount);
		}
	}
}

void AUTCTFGameMode::ScoreKill(AController* Killer, AController* Other)
{
	if( (Killer != NULL && Killer != Other) )
	{
		AUTPlayerState* AttackerPS = Cast<AUTPlayerState>(Killer->PlayerState);
		if (AttackerPS != NULL)
		{
			uint32 Points = BaseKillScore;
			if (Killer->GetPawn() != NULL &&IsCloseToFlagCarrier(Killer->GetPawn(), FlagCombatBonusDistance, 255))
			{
				Points += CombatBonusKillBonus;
			}

			AttackerPS->AdjustScore(Points);

			if (!bFirstBloodOccurred)
			{
				BroadcastLocalized(this, UUTFirstBloodMessage::StaticClass(), 0, AttackerPS, NULL, NULL);
				bFirstBloodOccurred = true;
			}
		}
	
	}
}

