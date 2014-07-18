// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTCTFGameMode.h"

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
		Holder->Team->Score++;
		// Send score message and play sound.....

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
	else
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

