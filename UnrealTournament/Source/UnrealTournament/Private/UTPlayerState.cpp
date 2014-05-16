// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameMode.h"
#include "UTPlayerState.h"
#include "Net/UnrealNetwork.h"

AUTPlayerState::AUTPlayerState(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bWaitingPlayer = false;
	bReadyToPlay = false;
	LastKillTime = 0.0f;
	int32 Kills = 0;
	bOutOfLives = false;
	int32 Deaths = 0;
}


void AUTPlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTPlayerState, bWaitingPlayer);
	DOREPLIFETIME(AUTPlayerState, bReadyToPlay);
	DOREPLIFETIME(AUTPlayerState, bOutOfLives);
	DOREPLIFETIME(AUTPlayerState, Deaths);

}



void AUTPlayerState::SetWaitingPlayer(bool B)
{
	bIsSpectator = B;
	bWaitingPlayer = B;
	ForceNetUpdate();
}

void AUTPlayerState::IncrementKills(bool bEnemyKill )
{
	if ( bEnemyKill )
	{
		LastKillTime = GetWorld()->TimeSeconds;
		Kills++;
	}
}

void AUTPlayerState::IncrementDeaths()
{
	Deaths += 1;
	SetNetUpdateTime(FMath::Min(NetUpdateTime, GetWorld()->TimeSeconds + 0.3f * FMath::FRand()));
}

void AUTPlayerState::AdjustScore(int ScoreAdjustment)
{
	Score += ScoreAdjustment;
	ForceNetUpdate();
}