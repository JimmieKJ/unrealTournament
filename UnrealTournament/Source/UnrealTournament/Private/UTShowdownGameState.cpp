// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTShowdownGameState.h"
#include "UnrealNetwork.h"
#include "UTTeamPlayerStart.h"

AUTShowdownGameState::AUTShowdownGameState(const FObjectInitializer& OI)
: Super(OI)
{
}

void AUTShowdownGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTShowdownGameState, SpawnSelector);
	DOREPLIFETIME(AUTShowdownGameState, IntermissionStageTime);
	DOREPLIFETIME(AUTShowdownGameState, bStartedSpawnSelection);
	DOREPLIFETIME(AUTShowdownGameState, bFinalIntermissionDelay);
	
	DOREPLIFETIME(AUTShowdownGameState, bActivateXRayVision);
}

bool AUTShowdownGameState::IsAllowedSpawnPoint_Implementation(AUTPlayerState* Chooser, APlayerStart* DesiredStart) const
{
	if (DesiredStart == NULL || SpawnSelector != Chooser || Chooser == NULL || (Cast<AUTPlayerStart>(DesiredStart) != NULL && ((AUTPlayerStart*)DesiredStart)->bIgnoreInShowdown))
	{
		return false;
	}
	else
	{
		AUTTeamPlayerStart* TeamStart = Cast<AUTTeamPlayerStart>(DesiredStart);
		if (TeamStart != NULL && TeamStart->TeamNum != Chooser->GetTeamNum())
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
						if (UTPS->RespawnChoiceA == DesiredStart)
						{
							return false;
						}
					}
				}
			}
			return true;
		}
	}
}

void AUTShowdownGameState::OnRep_XRayVision()
{
	if (bActivateXRayVision)
	{
		for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
		{
			AUTCharacter* UTC = Cast<AUTCharacter>(It->Get());
			if (UTC != NULL && !UTC->bTearOff && (!UTC->IsLocallyControlled() || Cast<APlayerController>(UTC->Controller) == NULL))
			{
				UTC->UpdateTacComMesh(true);
			}
		}
	}
}

void AUTShowdownGameState::OnRep_MatchState()
{
	// clean up old corpses after intermission
	if (PreviousMatchState == MatchState::MatchIntermission && MatchState == MatchState::InProgress)
	{
		for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
		{
			AUTCharacter* UTC = Cast<AUTCharacter>(It->Get());
			if (UTC != NULL && UTC->IsDead())
			{
				UTC->Destroy();
			}
		}
	}
	else if (MatchState == MatchState::MatchIntermission)
	{
		for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
		{
			AUTCharacter* UTC = Cast<AUTCharacter>(It->Get());
			if (UTC != NULL && !UTC->IsDead())
			{
				UTC->GetRootComponent()->SetHiddenInGame(true, true);
			}
		}
	}

	Super::OnRep_MatchState();
}