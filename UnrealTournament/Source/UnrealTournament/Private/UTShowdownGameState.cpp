// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTShowdownGameState.h"
#include "UnrealNetwork.h"

AUTShowdownGameState::AUTShowdownGameState(const FObjectInitializer& OI)
: Super(OI)
{
	MinSpawnDistance = 2000.0f;
}

void AUTShowdownGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTShowdownGameState, SpawnSelector);
	DOREPLIFETIME(AUTShowdownGameState, IntermissionStageTime);
	DOREPLIFETIME(AUTShowdownGameState, bFinalIntermissionDelay);
}

bool AUTShowdownGameState::IsAllowedSpawnPoint_Implementation(AUTPlayerState* Chooser, APlayerStart* DesiredStart) const
{
	if (DesiredStart == NULL || SpawnSelector != Chooser || Chooser == NULL)
	{
		return false;
	}
	else
	{
		for (APlayerState* PS : PlayerArray)
		{
			if (PS != Chooser && !PS->bOnlySpectator)
			{
				AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
				if (UTPS != NULL && UTPS->RespawnChoiceA != NULL)
				{
					if (UTPS->RespawnChoiceA == DesiredStart || (UTPS->Team != Chooser->Team && (DesiredStart->GetActorLocation() - UTPS->RespawnChoiceA->GetActorLocation()).Size() < MinSpawnDistance))
					{
						return false;
					}
				}
			}
		}
		return true;
	}
}