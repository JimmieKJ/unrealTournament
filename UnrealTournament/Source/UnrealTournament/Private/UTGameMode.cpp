// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/GameMode.h"
#include "UTDeathMessage.h"
#include "UTGameMessage.h"
#include "UTVictoryMessage.h"


AUTGameMode::AUTGameMode(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FObjectFinder<UBlueprint> PlayerPawnObject(TEXT("/Game/Blueprints/BaseUTCharacter"));
	if (PlayerPawnObject.Object != NULL)
	{
		DefaultPawnClass = (UClass*)PlayerPawnObject.Object->GeneratedClass;
	}

	// use our custom HUD class
	HUDClass = AUTHUD::StaticClass();

	GameStateClass = AUTGameState::StaticClass();

	PlayerControllerClass = AUTPlayerController::StaticClass();
	MinRespawnDelay = 1.5f;
	bUseSeamlessTravel = true;
	CountDown = 4;
	bPauseable = false;

	VictoryMessageClass=UUTVictoryMessage::StaticClass();
	DeathMessageClass=UUTDeathMessage::StaticClass();
	GameMessageClass=UUTGameMessage::StaticClass();

	CurrentGameStage = EGameStage::Initializing;

}

void AUTGameMode::SetGameStage(EGameStage::Type NewGameStage)
{
	CurrentGameStage = NewGameStage;
	if (UTGameState != NULL)
	{
		UTGameState->SetGameStage(NewGameStage);
	}
}


// Parse options for this game...
void AUTGameMode::InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage )
{
	Super::InitGame(MapName, Options, ErrorMessage);

	GameDifficulty = FMath::Max(0,GetIntOption(Options, TEXT("Difficulty"), GameDifficulty));

	FString InOpt = ParseOption(Options, TEXT("ForceRespawn"));
	if (!InOpt.IsEmpty())
	{
		if (FCString::Stricmp(*InOpt,TEXT("True") )==0 
			||	FCString::Stricmp(*InOpt,*GTrue.ToString())==0
			||	FCString::Stricmp(*InOpt,TEXT("Yes"))==0
			||	FCString::Stricmp(*InOpt,*GYes.ToString())==0)
		{
			bForceRespawn = true;
		}
		else if(FCString::Stricmp(*InOpt,TEXT("False"))==0
			||	FCString::Stricmp(*InOpt,*GFalse.ToString())==0
			||	FCString::Stricmp(*InOpt,TEXT("No"))==0
			||	FCString::Stricmp(*InOpt,*GNo.ToString())==0)
		{
			bForceRespawn = false;
		}
		else
		{
			bForceRespawn = FCString::Atoi(*InOpt);
		}
	}

	TimeLimit = FMath::Max(0,GetIntOption( Options, TEXT("TimeLimit"), TimeLimit ));

	// Set goal score to end match.
	GoalScore = FMath::Max(0,GetIntOption( Options, TEXT("GoalScore"), GoalScore ));
	SetGameStage(EGameStage::PreGame);
}

void AUTGameMode::InitGameState()
{
	Super::InitGameState();

	UTGameState = Cast<AUTGameState>(GameState);
	if (UTGameState != NULL)
	{
		UTGameState->SetGoalScore(GoalScore);
		UTGameState->SetTimeLimit(TimeLimit);
		UTGameState->SetGameStage(CurrentGameStage);
	}
	else
	{
		UE_LOG(UT,Error, TEXT("UTGameState is NULL %s"), *GameStateClass->GetFullName());
	}
}

void AUTGameMode::Reset()
{
	Super::Reset();

	bGameEnded = false;

	//now respawn all the players
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState != NULL && !Controller->PlayerState->bOnlySpectator)
		{
			RestartPlayer(Controller);
		}
	}

	UTGameState->SetTimeLimit(TimeLimit);
}

void AUTGameMode::RestartGame()
{
	if ( bGameRestarted )
	{
		return;
	}


	if ( EndTime > GetWorld()->TimeSeconds ) // still showing end screen
	{
		return;
	}
}

bool AUTGameMode::IsEnemy(AController * First, AController* Second)
{
	// In DM - Everyone is an enemy
	return First != Second;
}

void AUTGameMode::Killed( AController* Killer, AController* KilledPlayer, APawn* KilledPawn, const UDamageType* DamageType )
{
	AUTPlayerState* const KillerPlayerState = Killer ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
	AUTPlayerState* const KilledPlayerState = KilledPlayer ? Cast<AUTPlayerState>(KilledPlayer->PlayerState) : NULL;

	bool const bEnemyKill = IsEnemy(Killer, KilledPlayer);

	if ( KilledPlayerState != NULL )
	{
		KilledPlayerState->IncrementDeaths();
		ScoreKill(Killer, KilledPlayer);
		BroadcastDeathMessage(Killer, KilledPlayer, DamageType);
	}

	NotifyKilled(Killer, KilledPlayer, KilledPawn, DamageType);
}

void AUTGameMode::NotifyKilled(AController* Killer, AController* Killed, APawn* KilledPawn, const UDamageType* DamageType )
{
}


void AUTGameMode::ScoreKill(AController* Killer, AController* Other)
{
	if( (Killer == Other) || (Killer == NULL) )
	{
		// If it's a suicide, subtract a kill from the player...

		if (Other != NULL && Other->PlayerState != NULL && Cast<AUTPlayerState>(Other->PlayerState) != NULL)
		{
			Cast<AUTPlayerState>(Other->PlayerState)->AdjustScore(-1);
		}
	}
	else 
	{
		AUTPlayerState * KillerPlayerState = Cast<AUTPlayerState>(Killer->PlayerState);
		if ( KillerPlayerState != NULL )
		{
			KillerPlayerState->AdjustScore(+1);
			KillerPlayerState->IncrementKills(true);
			CheckScore(KillerPlayerState);
		}
	}
}


bool AUTGameMode::CheckScore(AUTPlayerState* Scorer)
{
	if ( Scorer != NULL )
	{
		if ( (GoalScore > 0) && (Scorer->Score >= GoalScore) )
		{
			EndGame(Scorer,TEXT("fraglimit"));
		}
	}
	return true;
}


void AUTGameMode::StartMatch()
{
	bMatchIsInProgress = true;
	SetGameStage(EGameStage::GameInProgress);

	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AActor* TestActor = *It;
		if (TestActor &&
			!TestActor->IsPendingKill() &&
			TestActor->IsA<APlayerState>())
		{
			Cast<APlayerState>(TestActor)->StartTime = 0;
		}
	}

	GameState->ElapsedTime = 0;
    Super::StartMatch();
}

void AUTGameMode::EndMatch()
{
	bMatchIsInProgress = false;
	UTGameState->bStopCountdown = true;
	SetGameStage(EGameStage::GameOver);
	GetWorldTimerManager().SetTimer(this, &AUTGameMode::PlayEndOfMatchMessage, 1.0f);

	for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator )
	{
		(*Iterator)->TurnOff();
	}
}

void AUTGameMode::EndGame(AUTPlayerState* Winner, const FString& Reason )
{
	if ( (FCString::Stricmp(*Reason, TEXT("triggered")) == 0) ||
		 (FCString::Stricmp(*Reason, TEXT("TimeLimit")) == 0) ||
		 (FCString::Stricmp(*Reason, TEXT("FragLimit")) == 0))
	{

		// If we don't have a winner, then go and find one
		if (Winner == NULL)
		{
			for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
			{
				AController* Controller = *Iterator;
				AUTPlayerState* CPS = Cast<AUTPlayerState> (Controller->PlayerState);
				if ( CPS && ((Winner == NULL) || (CPS->Score >= Winner->Score)) )
				{
					Winner = CPS;
				}
			}
		}

		UTGameState->WinnerPlayerState = Winner;
		EndTime = GetWorld()->TimeSeconds + EndTimeDelay;

		SetEndGameFocus(Winner);

		// Allow replication to happen before reporting scores, stats, etc.
		GetWorldTimerManager().SetTimer(this, &AUTGameMode::PerformEndGameHandling, 1.5f);
		bGameEnded = true;
		EndMatch();
	}
}


void AUTGameMode::SetEndGameFocus(AUTPlayerState* Winner)
{
	EndGameFocus = Cast<AController>(Winner->GetOwner())->GetPawn();
	if ( (EndGameFocus == NULL) && (Cast<AController>(Winner->GetOwner()) != NULL) )
	{
		// If the controller of the winner does not have a pawn, give him one.
		RestartPlayer(Cast<AController>(Winner->GetOwner()));
		EndGameFocus = Cast<AController>(Winner->GetOwner())->GetPawn();
	}

	if ( EndGameFocus != NULL )
	{
		EndGameFocus->bAlwaysRelevant = true;
	}

	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		Controller->GameHasEnded(EndGameFocus, (Controller->PlayerState != NULL) && (Controller->PlayerState == Winner) );
	}
}


void AUTGameMode::BroadcastDeathMessage(AController* Killer, AController* Other, const UDamageType* DamageType)
{
	if (DeathMessageClass != NULL)
	{
		if ( (Killer == Other) || (Killer == NULL) )
		{
			BroadcastLocalized(this, DeathMessageClass, 1, NULL, Other->PlayerState, const_cast<UDamageType*>(DamageType));
		}
		else
		{
			BroadcastLocalized(this, DeathMessageClass, 0, Killer->PlayerState, Other->PlayerState, const_cast<UDamageType*>(DamageType));
		}
	}
}


bool AUTGameMode::IsAWinner(AUTPlayerController* PC)
{
	return ( PC != NULL && ( PC->UTPlayerState->bOnlySpectator || PC->UTPlayerState == UTGameState->WinnerPlayerState));
}

void AUTGameMode::PlayEndOfMatchMessage()
{
	for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* Controller = *Iterator;
		if (Controller->IsA(AUTPlayerController::StaticClass()))
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
			if ( (PC->PlayerState != NULL) && !PC->PlayerState->bOnlySpectator )
			{
				PC->ClientReceiveLocalizedMessage(VictoryMessageClass, IsAWinner(PC) ? 0 : 1);
			}
		}
	}
}

void AUTGameMode::RestartPlayer(AController* aPlayer)
{
	if ( CurrentGameStage == EGameStage::PreGame && Cast<AUTPlayerState>(aPlayer->PlayerState) )
	{
		// If we are in the pre-game stage then flag the player as ready to play.  The game starting will be handled in the DefaultTimer() event
		Cast<AUTPlayerState>(aPlayer->PlayerState)->bReadyToPlay = true;
		return;
	}

	if (CurrentGameStage != EGameStage::GameInProgress)
	{
		return;
	}

	Super::RestartPlayer(aPlayer);
}

/* 
  Make sure pawn properties are back to default
  Also add default inventory
*/
void AUTGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);

	AUTCharacter* UTCharacter = Cast<AUTCharacter>(PlayerPawn);
	if ( UTCharacter != NULL && UTCharacter->GetInventory() == NULL )
	{
		UTCharacter->AddDefaultInventory(DefaultInventory);
	}
}

void AUTGameMode::ChangeName(AController* Other, const FString& S, bool bNameChange)
{
	// Cap player name's at 15 characters...
	FString SMod = S;
	if (SMod.Len()>15)
	{
		SMod = SMod.Left(15);
	}

    if ( !Other->PlayerState|| FCString::Stricmp(*Other->PlayerState->PlayerName, *SMod) == 0 )
    {
		return;
	}

	// Look to see if someone else is using the the new name
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState && FCString::Stricmp(*Controller->PlayerState->PlayerName, *SMod) == 0)
		{
			if ( Cast<APlayerController>(Other) != NULL )
			{
					Cast<APlayerController>(Other)->ClientReceiveLocalizedMessage( GameMessageClass, 5 );
					if ( FCString::Stricmp(*Other->PlayerState->PlayerName, *DefaultPlayerName) == 0 )
					{
						Other->PlayerState->SetPlayerName(DefaultPlayerName+Other->PlayerState->PlayerId);
					}
				return;
			}
		}
	}

    Other->PlayerState->SetPlayerName(SMod);
}

