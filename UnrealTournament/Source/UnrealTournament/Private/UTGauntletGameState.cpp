// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGauntletGame.h"
#include "UTGauntletGameState.h"
#include "UTGauntletFlag.h"
#include "UTGauntletGameMessage.h"
#include "UTCountdownMessage.h"
#include "Net/UnrealNetwork.h"

AUTGauntletGameState::AUTGauntletGameState(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	AttackingTeam = 255;
}

void AUTGauntletGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTGauntletGameState, AttackingTeam);
	DOREPLIFETIME(AUTGauntletGameState, FlagSpawnTimer);
	DOREPLIFETIME(AUTGauntletGameState, FlagSwapTime);
	DOREPLIFETIME(AUTGauntletGameState, Flag);
	
}

void AUTGauntletGameState::DefaultTimer()
{
	Super::DefaultTimer();
	AUTGauntletGame* Game = GetWorld()->GetAuthGameMode<AUTGauntletGame>();
	if (Role == ROLE_Authority && FlagSpawnTimer > 0)
	{
		FlagSpawnTimer--;
		if (FlagSpawnTimer == 11)
		{
			Game->BroadcastLocalized(this, UUTGauntletGameMessage::StaticClass(), 1, NULL, NULL, NULL);
		}
		if (FlagSpawnTimer <= 10 && FlagSpawnTimer > 0)
		{
			Game->BroadcastLocalized(this, UUTCountDownMessage::StaticClass(), FlagSpawnTimer, NULL, NULL, NULL);
		}
	}

}

void AUTGauntletGameState::AttackingTeamChanged()
{
	// Update hud/etc.
}

void AUTGauntletGameState::SetFlagSpawnTimer(int32 NewValue)
{
	AUTGauntletGame* Game = GetWorld()->GetAuthGameMode<AUTGauntletGame>();

	FlagSpawnTimer = NewValue;
	if (FlagSpawnTimer > 0)
	{
		Game->BroadcastLocalized(this, UUTGauntletGameMessage::StaticClass(), 0, NULL, NULL, NULL);
	}
}

