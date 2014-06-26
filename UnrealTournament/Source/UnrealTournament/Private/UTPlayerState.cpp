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

	// We want to be ticked.
	PrimaryActorTick.bCanEverTick = true;

}

void AUTPlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTPlayerState, bWaitingPlayer);
	DOREPLIFETIME(AUTPlayerState, bReadyToPlay);
	DOREPLIFETIME(AUTPlayerState, bOutOfLives);
	DOREPLIFETIME(AUTPlayerState, Kills);
	DOREPLIFETIME(AUTPlayerState, Deaths);
	DOREPLIFETIME(AUTPlayerState, Team);
}

void AUTPlayerState::NotifyTeamChanged_Implementation()
{
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AUTCharacter* P = Cast<AUTCharacter>(*It);
		if (P != NULL && P->PlayerState == this)
		{
			P->NotifyTeamChanged();
		}
	}
}

void AUTPlayerState::SetWaitingPlayer(bool B)
{
	bIsSpectator = B;
	bWaitingPlayer = B;
	ForceNetUpdate();
}

void AUTPlayerState::IncrementKills(bool bEnemyKill)
{
	if (bEnemyKill)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL && GetWorld()->TimeSeconds - LastKillTime < GS->MultiKillDelay)
		{
			MultiKillLevel++;
			if (Cast<APlayerController>(GetOwner()) != NULL)
			{
				((APlayerController*)GetOwner())->ClientReceiveLocalizedMessage(GS->MultiKillMessageClass, MultiKillLevel - 1, this);
			}
		}
		else
		{
			MultiKillLevel = 0;
		}
		if (Cast<AController>(GetOwner()) != NULL && ((AController*)GetOwner())->GetPawn() != NULL)
		{
			Spree++;
			if (Spree % 5 == 0)
			{
				if (GetWorld()->GetAuthGameMode() != NULL)
				{
					GetWorld()->GetAuthGameMode()->BroadcastLocalized(GetOwner(), GS->SpreeMessageClass, Spree / 5, this);
				}
			}
		}
		LastKillTime = GetWorld()->TimeSeconds;
		Kills++;
	}
}

void AUTPlayerState::IncrementDeaths(AUTPlayerState* KillerPlayerState)
{
	Deaths += 1;
	// spree has ended
	if (Spree >= 5 && GetWorld()->GetAuthGameMode() != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			GetWorld()->GetAuthGameMode()->BroadcastLocalized(GetOwner(), GS->SpreeMessageClass, Spree / -5, this, KillerPlayerState);
		}
	}
	Spree = 0;

	SetNetUpdateTime(FMath::Min(NetUpdateTime, GetWorld()->TimeSeconds + 0.3f * FMath::FRand()));

	if (Role == ROLE_Authority)
	{
		// Trigger it locally
		OnDeathsReceived();
	}

}

void AUTPlayerState::AdjustScore(int32 ScoreAdjustment)
{
	Score += ScoreAdjustment;
	ForceNetUpdate();
}

void AUTPlayerState::OnDeathsReceived()
{
	UE_LOG(UT,Log, TEXT("OnDeathsReceived %s"), *GetNameSafe(GetWorld()->GetGameState<AUTGameState>()));
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState != NULL)
	{
		RespawnTime = UTGameState->RespawnWaitTime;
	}
}

void AUTPlayerState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// If we are waiting to respawn then count down
	if (RespawnTime > 0.0f)
	{
		RespawnTime -= DeltaTime;
	}
}