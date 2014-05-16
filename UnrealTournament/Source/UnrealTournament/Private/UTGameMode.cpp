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

	PlayerControllerClass = AUTPlayerController::StaticClass();
	MinRespawnDelay = 1.5f;
	bUseSeamlessTravel = true;
	CountDown = 4;
	bPauseable = false;

	VictoryMessageClass=UUTVictoryMessage::StaticClass();
	DeathMessageClass=UUTDeathMessage::StaticClass();
	GameMessageClass=UUTGameMessage::StaticClass();
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


void AUTGameMode::StartNewPlayer(APlayerController* NewPlayer)
{
	// Delayed start, don't give a pawn to the player yet
	NewPlayer->bPlayerIsWaiting = true;
	NewPlayer->ChangeState(NAME_Spectating);
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

	//NotifyKilled(Killer, KilledPlayer, KilledPawn, DamageType);
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


void AUTGameMode::EndGame(AUTPlayerState* Winner, const FString& Reason )
{
	if ( (FCString::Stricmp(*Reason, TEXT("triggered")) == 0) ||
		 (FCString::Stricmp(*Reason, TEXT("TimeLimit")) == 0) ||
		 (FCString::Stricmp(*Reason, TEXT("FragLimit")) == 0))
	{
		// Allow replication to happen before reporting scores, stats, etc.
		GetWorldTimerManager().SetTimer(this, &AUTGameMode::PerformEndGameHandling, 1.5f);
		bGameEnded = true;
		EndMatch();
	}
}

void AUTGameMode::EndMatch()
{
	bMatchIsInProgress = false;
	UTGameState->bStopCountdown = true;
	GetWorldTimerManager().SetTimer(this, &AUTGameMode::PlayEndOfMatchMessage, 1.0f);

	for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator )
	{
		(*Iterator)->TurnOff();
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