// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTMultiKillMessage.h"
#include "UTSpreeMessage.h"
#include "Net/UnrealNetwork.h"

AUTGameState::AUTGameState(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	MultiKillMessageClass = UUTMultiKillMessage::StaticClass();
	SpreeMessageClass = UUTSpreeMessage::StaticClass();
	MultiKillDelay = 4.0f;
}

void AUTGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTGameState, RemainingMinute);
	DOREPLIFETIME(AUTGameState, WinnerPlayerState);
	DOREPLIFETIME(AUTGameState, RespawnWaitTime);
	DOREPLIFETIME(AUTGameState, TimeLimit);
	
	DOREPLIFETIME_CONDITION(AUTGameState, bWeaponStay, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, bPlayerMustBeReady, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, GoalScore, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, RemainingTime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, bPlayerMustBeReady, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, OverlayMaterials, COND_InitialOnly);
}

void AUTGameState::AddOverlayMaterial(UMaterialInterface* NewOverlay)
{
	if (NewOverlay != NULL && Role == ROLE_Authority)
	{
		if (NetUpdateTime > 0.0f)
		{
			UE_LOG(UT, Warning, TEXT("UTGameState::AddOverlayMaterial() called after startup; may not take effect on clients"));
		}
		for (int32 i = 0; i < ARRAY_COUNT(OverlayMaterials); i++)
		{
			if (OverlayMaterials[i] == NewOverlay)
			{
				return;
			}
			else if (OverlayMaterials[i] == NULL)
			{
				OverlayMaterials[i] = NewOverlay;
				return;
			}
		}
		UE_LOG(UT, Warning, TEXT("UTGameState::AddOverlayMaterial(): Ran out of slots, couldn't add %s"), *NewOverlay->GetFullName());
	}
}

void AUTGameState::BeginPlay()
{
	Super::BeginPlay();

	// HACK: temporary workaround for replicated world gravity getting clobbered on client
	if (GetNetMode() == NM_Client)
	{
		if (GetWorld()->GetWorldSettings()->WorldGravityZ != 0.0f)
		{
			GetWorld()->GetWorldSettings()->bWorldGravitySet = true;
		}
		else
		{
			GetWorld()->GetWorldSettings()->GetGravityZ();
		}
	}

	{
		TArray<UObject*> AllInventory;
		GetObjectsOfClass(AUTInventory::StaticClass(), AllInventory, true, RF_NoFlags);
		for (int32 i = 0; i < AllInventory.Num(); i++)
		{
			if (AllInventory[i]->HasAnyFlags(RF_ClassDefaultObject))
			{
				checkSlow(AllInventory[i]->IsA(AUTInventory::StaticClass()));
				((AUTInventory*)AllInventory[i])->AddOverlayMaterials(this);
			}
		}
	}
}

void AUTGameState::DefaultTimer()
{
	Super::DefaultTimer();

	if (GetWorld()->GetNetMode() == NM_Client)
	{
		if (RemainingMinute > 0)
		{
			RemainingTime = RemainingMinute;
			RemainingMinute = 0;
		}
	}

	if (RemainingTime > 0 && !bStopCountdown)
	{
		RemainingTime--;
		if (GetWorld()->GetNetMode() != NM_Client)
		{
			if (RemainingTime % 60 == 0)
			{
				RemainingMinute	 = RemainingTime;
			}
		}
	}
}

bool AUTGameState::OnSameTeam(class APlayerState* Player1, class APlayerState* Player2)
{
	return false;
}

void AUTGameState::SetTimeLimit(float NewTimeLimit)
{
	TimeLimit = NewTimeLimit;
	RemainingTime = 60.0f * TimeLimit;
	RemainingMinute = RemainingTime;

	ForceNetUpdate();
}

void AUTGameState::SetGoalScore(uint32 NewGoalScore)
{
	GoalScore = NewGoalScore;
	ForceNetUpdate();
}

/**
  * returns true if P1 should be sorted before P2
  */
bool AUTGameState::InOrder( AUTPlayerState* P1, AUTPlayerState* P2 )
{
	// spectators are sorted last
    if( P1->bOnlySpectator )
    {
		return P2->bOnlySpectator;
    }
    else if ( P2->bOnlySpectator )
	{
		return true;
	}

	// sort by Score
    if( P1->Score < P2->Score )
	{
		return false;
	}
    if( P1->Score == P2->Score )
    {
		// if score tied, use deaths to sort
		if ( P1->Deaths > P2->Deaths )
			return false;

		// keep local player highest on list
		if ( (P1->Deaths == P2->Deaths) && (Cast<APlayerController>(P2->GetOwner()) != NULL) )
		{
			ULocalPlayer* LP2 = Cast<ULocalPlayer>(Cast<APlayerController>(P2->GetOwner())->Player);
			if ( LP2 != NULL )
			{
				// make sure ordering is consistent for splitscreen players
				ULocalPlayer* LP1 = Cast<ULocalPlayer>(Cast<APlayerController>(P2->GetOwner())->Player);
				return ( LP1 != NULL );
			}
		}
	}
    return true;
}

/** 
  * Sort the PRI Array based on InOrder() prioritization
  */
void AUTGameState::SortPRIArray()
{
	for (int32 i=0; i<PlayerArray.Num()-1; i++)
	{
		AUTPlayerState* P1 = Cast<AUTPlayerState>(PlayerArray[i]);
		for (int32 j=i+1; j<PlayerArray.Num(); j++)
		{
			AUTPlayerState* P2 = Cast<AUTPlayerState>(PlayerArray[j]);
			if( !InOrder( P1, P2 ) )
			{
				PlayerArray[i] = P2;
				PlayerArray[j] = P1;
				P1 = P2;
			}
		}
	}
}

bool AUTGameState::HasMatchStarted() const
{
	return Super::HasMatchStarted() && GetMatchState() != MatchState::CountdownToBegin;
}

bool AUTGameState::IsMatchInProgress() const
{
	if (GetMatchState() == MatchState::InProgress || GetMatchState() == MatchState::MatchIsInOvertime)
	{
		return true;
	}

	return false;
}

bool AUTGameState::IsMatchInOvertime() const
{
	if (GetMatchState() == MatchState::MatchEnteringOvertime || GetMatchState() == MatchState::MatchIsInOvertime)
	{
		return true;
	}

	return false;
}

void AUTGameState::OnWinnerReceived()
{
}

