// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "Net/UnrealNetwork.h"

AUTGameState::AUTGameState(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
}

void AUTGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTGameState, RemainingMinute);
	DOREPLIFETIME(AUTGameState, WinnerPlayerState);

	DOREPLIFETIME_CONDITION(AUTGameState, bWeaponStay, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, bPlayerMustBeReady, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, GoalScore, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, TimeLimit, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, RemainingTime, COND_InitialOnly);

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
	ForceNetUpdate();
}

void AUTGameState::SetGoalScore(uint32 NewGoalScore)
{
	GoalScore = NewGoalScore;
	ForceNetUpdate();
}