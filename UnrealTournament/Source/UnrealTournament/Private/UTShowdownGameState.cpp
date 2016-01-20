// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTShowdownGameState.h"
#include "UnrealNetwork.h"
#include "UTTeamPlayerStart.h"
#include "UTTimerMessage.h"
#include "UTShowdownGame.h"

AUTShowdownGameState::AUTShowdownGameState(const FObjectInitializer& OI)
: Super(OI)
{
	bMatchHasStarted = false;
	bPersistentKillIconMessages = true;
	GoalScoreText = NSLOCTEXT("UTScoreboard", "CTFGoalScoreFormat", "Win {0} Rounds");
}

void AUTShowdownGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTShowdownGameState, SpawnSelector);
	DOREPLIFETIME(AUTShowdownGameState, IntermissionStageTime);
	DOREPLIFETIME(AUTShowdownGameState, bStartedSpawnSelection);
	DOREPLIFETIME(AUTShowdownGameState, bFinalIntermissionDelay);
	DOREPLIFETIME(AUTShowdownGameState, bMatchHasStarted);
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
		for (APlayerState* PS : PlayerArray)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
			if (UTPS)
			{
				UTPS->RoundDamageDone = 0;
			}
		}
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

void AUTShowdownGameState::CheckTimerMessage()
{
	// in Showdown do this server side so we can include the currently winning team
	if (Role == ROLE_Authority && IsMatchInProgress())
	{
		int32 TimerMessageIndex = -1;
		switch (RemainingTime)
		{
			case 300: TimerMessageIndex = 13; break;		// 5 mins remain
			case 180: TimerMessageIndex = 12; break;		// 3 mins remain
			case 60: TimerMessageIndex = 11; break;		// 1 min remains
			case 30: TimerMessageIndex = 10; break;		// 30 seconds remain
			default:
				if (RemainingTime <= 10)
				{
					TimerMessageIndex = RemainingTime - 1;
				}
				break;
		}

		if (TimerMessageIndex >= 0)
		{
			AInfo* CurrentTiebreakWinner = GetWorld()->GetAuthGameMode<AUTShowdownGame>()->GetTiebreakWinner();
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(*It);
				if (PC != NULL)
				{
					PC->ClientReceiveLocalizedMessage(UUTTimerMessage::StaticClass(), TimerMessageIndex, NULL, NULL, CurrentTiebreakWinner);
				}
			}
		}
	}
}