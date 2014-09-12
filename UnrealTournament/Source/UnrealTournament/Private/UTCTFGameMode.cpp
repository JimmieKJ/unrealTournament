// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTCTFGameMode.h"
#include "UTFirstBloodMessage.h"
#include "UTPickup.h"
#include "UTGameMessage.h"

namespace MatchState
{
	const FName MatchEnteringHalftime = FName(TEXT("MatchEnteringHalftime"));
	const FName MatchIsAtHalftime = FName(TEXT("MatchIsAtHalftime"));
	const FName MatchExitingHalftime = FName(TEXT("MatchExitingHalftime"));
	const FName MatchEnteringSuddenDeath = FName(TEXT("MatchEnteringSuddenDeath"));
	const FName MatchIsInSuddenDeath = FName(TEXT("MatchIsInSuddenDeath"));
}


AUTCTFGameMode::AUTCTFGameMode(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	// By default, we do 2 team CTF
	MaxNumberOfTeams = 2;
	HalftimeDuration = 15;	// 15 second half-time by default...
	HUDClass = AUTHUD_CTF::StaticClass();
	GameStateClass = AUTCTFGameState::StaticClass();
	bAllowOvertime = true;
	bOldSchool = false;
	OvertimeDuration = 5;
	bUseTeamStarts = true;
	bSuddenDeath = true;
	GoalScore = 0;
	TimeLimit = 14;
	MapPrefix = TEXT("CTF");

	SuddenDeathHealthDrain = 5;

	//Add the translocator here for now :(
	static ConstructorHelpers::FObjectFinder<UClass> WeapTranslocator(TEXT("BlueprintGeneratedClass'/Game/RestrictedAssets/UserContent/Translocator/BP_Translocator.BP_Translocator_C'"));
	DefaultInventory.Add(WeapTranslocator.Object);

	FriendlyGameName = TEXT("CTF");
}

void AUTCTFGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	FString InOpt = ParseOption(Options, TEXT("OldSchool"));
	bOldSchool = EvalBoolOptions(InOpt, bOldSchool);

	InOpt = ParseOption(Options, TEXT("SuddenDeath"));
	bSuddenDeath = EvalBoolOptions(InOpt, bSuddenDeath);

	// HalftimeDuration is in seconds and used in seconds,
	HalftimeDuration = FMath::Max(1, GetIntOption(Options, TEXT("HalftimeDuration"), HalftimeDuration));

	// OvertimeDuration is in minutes
	OvertimeDuration = FMath::Max(1, GetIntOption(Options, TEXT("OvertimeDuration"), OvertimeDuration));
	OvertimeDuration *= 60;

	if (TimeLimit > 0)
	{
		TimeLimit = uint32(float(TimeLimit) * 0.5);
	}
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
							PS->Assists++;
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

			// NOTE: It's possible that the first player to pickup this flag might have left so first might be NULL.  Not
			// sure if we should then assign the points to the next in line so I'm ditching the points for now.

			for (int i=0;i<GameObject->AssistTracking.Num();i++)
			{
				AUTPlayerState* Who = GameObject->AssistTracking[i].Holder;
				if (Who != NULL)
				{
					float HeldTime = GameObject->GetHeldTime(Who);
					int32 Points = i == 0 ? FlagFirstPickupPoints : 0;
					if (HeldTime > 0 && GameObject->TotalHeldTime > 0)
					{
						float Perc = HeldTime / GameObject->TotalHeldTime;
						Points = Points + int(float(FlagTotalScorePool * Perc));
					}
					UE_LOG(UT,Log,TEXT("    Assist Points for %s = %i"), *Who->PlayerName, Points)				
					Who->AdjustScore(Points);
					if (Who != Holder)
					{
						Who->Assists++;
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
	else if ( CTFGameState->IsMatchInProgress() )
	{
		UE_LOG(UT,Log, TEXT("Time Remaining %i %i"),CTFGameState->RemainingTime, CTFGameState->bPlayingAdvantage);
		if ( CTFGameState->RemainingTime <= 0 )
		{
			// Halftime?? then exit it
			if (CTFGameState->IsMatchAtHalftime())
			{
				SetMatchState(MatchState::MatchExitingHalftime);
			}

			// If we are in Overtime, look to see if we should start sudden death
			else if ( CTFGameState->IsMatchInOvertime() )
			{
				if (!CTFGameState->IsMatchInSuddenDeath() && bSuddenDeath)
				{
					SetMatchState(MatchState::MatchEnteringSuddenDeath);				
				}
				else if ( CTFGameState->FindLeadingTeam() != NULL )
				{
					// Match is over....
					EndGame(NULL, FName(TEXT("TimeLimit")));	
				}
			}
			else
			{
				if ( CTFGameState->bSecondHalf )
				{
					AUTTeamInfo* WinningTeam = CTFGameState->FindLeadingTeam();
					if (WinningTeam != NULL)
					{
						AUTPlayerState* WinningPS = FindBestPlayerOnTeam(WinningTeam->GetTeamNum());
						EndGame(WinningPS, FName(TEXT("TimeLimit")));	
				
					}
					else 
					{
						if (bAllowOvertime)
						{
							SetMatchState(MatchState::MatchEnteringHalftime);
						}
						else
						{
							// Match is over....
							EndGame(NULL, FName(TEXT("TimeLimit")));
						}
					}
				}
				else 
				{
					SetMatchState(MatchState::MatchEnteringHalftime);
				}
			}
		}
		else if (CTFGameState->bPlayingAdvantage)
		{
			if (!CheckAdvantage())
			{
				CTFGameState->SetTimeLimit(0);
			}
		}
	}
}

void AUTCTFGameMode::HandleMatchHasStarted()
{
	if (!CTFGameState->bSecondHalf)
	{
		Super::HandleMatchHasStarted();
	}
}

void AUTCTFGameMode::HandleHalftime()
{
}

void AUTCTFGameMode::HandleEnteringHalftime()
{
	// Figure out who we should look at

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


	// Freeze all of the pawns and detach their controllers
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		// Detach all controllers from their pawns
		if ((*Iterator)->GetPawn() != NULL)
		{
			(*Iterator)->GetPawn()->TurnOff();
			(*Iterator)->UnPossess();
		}
	}

	// Tell the controllers to look at cool people

	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
		UE_LOG(UT,Log,TEXT("Setting Viewtarget for %s"), *GetNameSafe(PC));
		if (PC != NULL)
		{
			uint8 TeamNum = PC->GetTeamNum();
			AActor* BestTarget = NULL;
			if (TeamNum < BestPlayers.Num() && BestPlayers[TeamNum] != NULL)
			{
				BestTarget = BestPlayers[TeamNum];
			}
			PC->ClientHalftime();
			PC->SetViewTarget(BestTarget);
		}
	}

	CTFGameState->bHalftime = true;
	CTFGameState->SetTimeLimit(HalftimeDuration);	// Reset the Game Clock for Halftime

	SetMatchState(MatchState::MatchIsAtHalftime);

}

void AUTCTFGameMode::HalftimeIsOver()
{
	SetMatchState(MatchState::MatchExitingHalftime);
}

void AUTCTFGameMode::HandleExitingHalftime()
{
	TArray<APawn*> PawnsToDestroy;

	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		if (*It)
		{
			PawnsToDestroy.Add(*It);
		}
	}

	for (int i=0;i<PawnsToDestroy.Num();i++)
	{
		APawn* Pawn = PawnsToDestroy[i];
		if (Pawn != NULL && !Pawn->IsPendingKill())
		{
			Pawn->Destroy();	
		}
	}

	// reset everything
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		IUTResetInterface* ResetActor = InterfaceCast<IUTResetInterface>(*It);
		if (ResetActor != NULL)
		{
			ResetActor->Execute_Reset(*It);
		}
	}

	//now respawn all the players
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState != NULL && !Controller->PlayerState->bOnlySpectator)
		{
			RestartPlayer(Controller);
		}
	}

	// Send all flags home..

	CTFGameState->ResetFlags();
	CTFGameState->bHalftime = false;
	CTFGameState->bPlayingAdvantage = false;
	CTFGameState->AdvantageTeamIndex = -1;
	
	if (CTFGameState->bSecondHalf)
	{
		SetMatchState(MatchState::MatchEnteringOvertime);
	}
	else
	{
		CTFGameState->bSecondHalf = true;
		CTFGameState->SetTimeLimit(TimeLimit);		// Reset the GameClock for the second time.
		SetMatchState(MatchState::InProgress);
	}
}

void AUTCTFGameMode::GameObjectiveInitialized(AUTGameObjective* Obj)
{
	AUTCTFFlagBase* FlagBase = Cast<AUTCTFFlagBase>(Obj);
	if (FlagBase != NULL && FlagBase->MyFlag)
	{
		CTFGameState->CacheFlagBase(FlagBase);
	}
}

bool AUTCTFGameMode::PlayerCanRestart( APlayerController* Player )
{
	// Can't restart in overtime
	if (!CTFGameState->IsMatchInProgress() || CTFGameState->IsMatchAtHalftime() || CTFGameState->IsMatchInSuddenDeath()|| 
			Player == NULL || Player->IsPendingKillPending())
	{
		return false;
	}
	
	// Ask the player controller if it's ready to restart as well
	return Player->CanRestartPlayer();
}


bool AUTCTFGameMode::IsCloseToFlagCarrier(AActor* Who, float CheckDistanceSquared, uint8 TeamNum)
{
	if (CTFGameState == NULL || CTFGameState->FlagBases.Num() < 2)
	{
		AUTCTFFlag* Flags[2];
		Flags[0] = CTFGameState->FlagBases[0]->MyFlag;
		Flags[1] = CTFGameState->FlagBases[1]->MyFlag;

		bool bCloseToRed = Flags[0]->ObjectState == CarriedObjectState::Held && (Flags[0]->GetActorLocation() - Who->GetActorLocation()).SizeSquared() <= CheckDistanceSquared;
		bool bCloseToBlue = Flags[1]->ObjectState == CarriedObjectState::Held && (Flags[1]->GetActorLocation() - Who->GetActorLocation()).SizeSquared() <= CheckDistanceSquared;

		if ( (TeamNum == 0 && bCloseToRed) || (TeamNum == 1 && bCloseToBlue) ||
				(TeamNum == 255 && (bCloseToRed || bCloseToBlue)) )
		{
			return true;
		}
	}

	return false;
}

void AUTCTFGameMode::ScorePickup(AUTPickup* Pickup, AUTPlayerState* PickedUpBy, AUTPlayerState* LastPickedUpBy)
{
	if (PickedUpBy != NULL && Pickup != NULL)
	{
		int Points = 0;
		if (Pickup->PickupType == PC_Minor) Points = MinorPickupScore;
		if (Pickup->PickupType == PC_Major) Points = MajorPickupScore;
		if (Pickup->PickupType == PC_Super) Points = SuperPickupScore;

		if (PickedUpBy == LastPickedUpBy) 
		{
			Points = uint32( float(Points) * ControlFreakMultiplier);
		}

		PickedUpBy->AdjustScore(Points);

		UE_LOG(UT,Log,TEXT("========================================="));
		UE_LOG(UT,Log,TEXT("ScorePickup: %s %s %i"), *PickedUpBy->PlayerName, *GetNameSafe(Pickup), Points);
		UE_LOG(UT,Log,TEXT("========================================="));
	}
}


void AUTCTFGameMode::ScoreDamage(int DamageAmount, AController* Victim, AController* Attacker)
{
	Super::ScoreDamage(DamageAmount, Victim, Attacker);
	
	// No Damage for environmental damage
	if (Attacker == NULL) return;
	
	AUTPlayerState* AttackerPS = Cast<AUTPlayerState>(Attacker->PlayerState);

	if (AttackerPS != NULL)
	{
		int AdjustedDamageAmount = FMath::Clamp<int>(DamageAmount,0,100);
		if (Attacker != Victim)
		{
			if (Attacker->GetPawn() != NULL && IsCloseToFlagCarrier(Attacker->GetPawn(), FlagCombatBonusDistance, 255))
			{
				AdjustedDamageAmount = uint32( float(AdjustedDamageAmount) * FlagCarrierCombatMultiplier);
			}
			AttackerPS->AdjustScore(AdjustedDamageAmount);
			UE_LOG(UT,Log,TEXT("========================================="));
			UE_LOG(UT,Log,TEXT("DamageScore: %s %i"), *AttackerPS->PlayerName, AdjustedDamageAmount);
			UE_LOG(UT,Log,TEXT("========================================="));
		}
	}
}

void AUTCTFGameMode::ScoreKill(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType)
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
			AttackerPS->IncrementKills(true);

			UE_LOG(UT,Log,TEXT("========================================="));
			UE_LOG(UT,Log,TEXT("Kill Score: %s %i"), *AttackerPS->PlayerName, Points);
			UE_LOG(UT,Log,TEXT("========================================="));

			if (!bFirstBloodOccurred)
			{
				BroadcastLocalized(this, UUTFirstBloodMessage::StaticClass(), 0, AttackerPS, NULL, NULL);
				bFirstBloodOccurred = true;
			}

			if (CTFGameState->IsMatchInSuddenDeath())
			{
				if (Killer != NULL)
				{
					AUTCharacter* Char = Cast<AUTCharacter>(Killer->GetPawn());
					if (Char != NULL)
					{
						Char->Health = 100;
					}
				}

				// Search through all players and determine if anyone is still alive and 

				FName LastMan = TEXT("LastManStanding");

				int TeamCounts[2] = { 0, 0 };
				for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
				{
					AUTCharacter* Char = Cast<AUTCharacter>(*It);
					if (Char != NULL && !Char->IsDead())
					{
						int TeamNum = Char->GetTeamNum();
						if (TeamNum >= 0 && TeamNum <= 1)
						{
							TeamCounts[TeamNum]++;
						}
					}
				}

				if (TeamCounts[AttackerPS->GetTeamNum()] > 0 && TeamCounts[1 - AttackerPS->GetTeamNum()] <= 0)
				{
					EndGame(AttackerPS, LastMan);
				}
			}
		}
	}
}


bool AUTCTFGameMode::IsMatchInSuddenDeath()
{
	return CTFGameState->IsMatchInSuddenDeath();
}

void AUTCTFGameMode::SetMatchState(FName NewState)
{
	if (MatchState == NewState)
	{
		return;
	}

	// Look to see if one team has an advantage.  If they do set for advantage instead of half-time..

	if (NewState == MatchState::MatchEnteringHalftime && !CTFGameState->bPlayingAdvantage)		// Only check if we aren't already playing advantage
	{
		int32 AdvantageTeam = TeamWithAdvantage();
		if (AdvantageTeam >= 0)
		{
			// A team has advantage.. so set the flags.
			CTFGameState->bPlayingAdvantage = true;
			CTFGameState->AdvantageTeamIndex = AdvantageTeam;
			CTFGameState->SetTimeLimit(60);
			return;
		}
	}

	Super::SetMatchState(NewState);

	if (NewState == MatchState::MatchEnteringHalftime)
	{
		HandleEnteringHalftime();
	}
	else if (NewState == MatchState::MatchIsAtHalftime)
	{
		HandleHalftime();
	}
	else if (NewState == MatchState::MatchExitingHalftime)
	{
		HandleExitingHalftime();
	}
	else if (NewState == MatchState::MatchEnteringSuddenDeath)
	{
		HandleEnteringSuddenDeath();
	}
	else if (NewState == MatchState::MatchIsInSuddenDeath)
	{
		HandleSuddenDeath();
	}
}

void AUTCTFGameMode::HandleEnteringOvertime()
{
	CTFGameState->SetTimeLimit(OvertimeDuration);
	SetMatchState(MatchState::MatchIsInOvertime);
}

void AUTCTFGameMode::HandleEnteringSuddenDeath()
{
	BroadcastLocalized(this, UUTGameMessage::StaticClass(), 7, NULL, NULL, NULL);
}

void AUTCTFGameMode::HandleSuddenDeath()
{

}

void AUTCTFGameMode::DefaultTimer()
{	
	Super::DefaultTimer();

	if (CTFGameState->IsMatchInSuddenDeath())
	{
		// Iterate over all of the pawns and slowly drain their health

		for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
		{
			AController* C = Cast<AController>(*Iterator);
			if (C != NULL && C->GetPawn() != NULL)
			{
				AUTCharacter* Char = Cast<AUTCharacter>(C->GetPawn());
				if (Char!=NULL && Char->Health > 0)
				{
					int Damage = SuddenDeathHealthDrain;
					if (Char->GetCarriedObject() != NULL)
					{
						Damage = int(float(Damage) * 1.5f);
					}

					Char->TakeDamage(Damage, FDamageEvent(UUTDamageType::StaticClass()), NULL, NULL);
				}
			}
		}
	}
}

void AUTCTFGameMode::ScoreHolder(AUTPlayerState* Holder)
{
	Holder->Score += FlagHolderPointsPerSecond;
}

uint8 AUTCTFGameMode::TeamWithAdvantage()
{
	AUTCTFFlag* Flags[2];
	Flags[0] = CTFGameState->FlagBases[0]->MyFlag;
	Flags[1] = CTFGameState->FlagBases[1]->MyFlag;

	if ((Flags[0]->ObjectState == CarriedObjectState::Held && Flags[1]->ObjectState == CarriedObjectState::Held) ||
		(Flags[0]->ObjectState == CarriedObjectState::Home && Flags[1]->ObjectState == CarriedObjectState::Home))
	{
		// Both flags are either at home or held the no one has advantage.
		return -1;
	}

	int8 AdvantageNum = Flags[0]->ObjectState == CarriedObjectState::Held ? 0 : 1;
	if (AdvantageNum < 0 || AdvantageNum > 1 || Flags[AdvantageNum] == NULL || Flags[AdvantageNum]->ObjectState == CarriedObjectState::Dropped) return -1;

	return AdvantageNum;
}

bool AUTCTFGameMode::CheckAdvantage()
{
	AUTCTFFlag* Flags[2];
	Flags[0] = CTFGameState->FlagBases[0]->MyFlag;
	Flags[1] = CTFGameState->FlagBases[1]->MyFlag;

	uint8 OtherTeam = 1 - CTFGameState->AdvantageTeamIndex;

	// The Flag was returned so advantage lost
	if (Flags[CTFGameState->AdvantageTeamIndex]->ObjectState == CarriedObjectState::Home) 
	{
		UE_LOG(UT,Log,TEXT("Advantage Flag Home"));
		return false;	
	}

	if (Flags[OtherTeam]->ObjectState == CarriedObjectState::Held)
	{
		AdvantageGraceTime++;
		if (AdvantageGraceTime >= 5)		// 5 second grace period.. make this config :)
		{
			UE_LOG(UT,Log,TEXT("Timer"));
			return false;
		}
	}
	else
	{
		AdvantageGraceTime = 0;
	}
		
	return true;
}