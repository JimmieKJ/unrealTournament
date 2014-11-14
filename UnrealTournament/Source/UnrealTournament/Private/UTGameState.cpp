// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTMultiKillMessage.h"
#include "UTSpreeMessage.h"
#include "UTRemoteRedeemer.h"
#include "Net/UnrealNetwork.h"
#include "UTTimerMessage.h"

AUTGameState::AUTGameState(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	MultiKillMessageClass = UUTMultiKillMessage::StaticClass();
	SpreeMessageClass = UUTSpreeMessage::StaticClass();
	MultiKillDelay = 4.0f;
	SpawnProtectionTime = 2.5f;
	bWeaponStay = true;
	bViewKillerOnDeath = true;

	ServerName = TEXT("My First Server");
	ServerMOTD = TEXT("Welcome!");
}

void AUTGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTGameState, RemainingMinute);
	DOREPLIFETIME(AUTGameState, WinnerPlayerState);
	DOREPLIFETIME(AUTGameState, WinningTeam);
	DOREPLIFETIME(AUTGameState, RespawnWaitTime);
	DOREPLIFETIME(AUTGameState, TimeLimit);
	DOREPLIFETIME(AUTGameState, bTeamGame);
	DOREPLIFETIME(AUTGameState, bOnlyTheStrongSurvive);
	DOREPLIFETIME(AUTGameState, bViewKillerOnDeath);
	
	DOREPLIFETIME_CONDITION(AUTGameState, bWeaponStay, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, bPlayerMustBeReady, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, GoalScore, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, RemainingTime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, bPlayerMustBeReady, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, OverlayMaterials, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, SpawnProtectionTime, COND_InitialOnly);

	DOREPLIFETIME_CONDITION(AUTGameState, ServerName, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTGameState, ServerMOTD, COND_InitialOnly);
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

	// HACK: temporary hack around config property replication bug; force to be different from defaults
	ServerName += TEXT(" ");
	ServerMOTD += TEXT(" ");

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

	if (GetNetMode() == NM_Client)
	{
		// hook up any TeamInfos that were received prior
		for (FActorIterator It(GetWorld()); It; ++It)
		{
			AUTTeamInfo* Team = Cast<AUTTeamInfo>(*It);
			if (Team != NULL)
			{
				Team->ReceivedTeamIndex();
			}
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

void AUTGameState::OnRep_RemainingTime()
{
	// if we received RemainingTime, it takes precedence
	// note that this relies on all variables being received prior to any notifies being called
	RemainingMinute = 0;
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

	if (RemainingTime > 0 && !bStopGameClock && IsMatchInProgress())
	{
		RemainingTime--;
		if (GetWorld()->GetNetMode() != NM_Client)
		{
			if (RemainingTime % 60 == 0)
			{
				RemainingMinute	= RemainingTime;
			}
		}
	}

	if (GetWorld()->GetNetMode() != NM_DedicatedServer && IsMatchInProgress() && !bStopGameClock)
	{
		int32 TimerMessageIndex = -1;
		switch (RemainingTime)
		{
			case 300 : TimerMessageIndex = 13; break;		// 5 mins remain
			case 180 : TimerMessageIndex = 12; break;		// 3 mins remain
			case 60  : TimerMessageIndex = 11; break;		// 1 min remains
			case 30  : TimerMessageIndex = 10; break;		// 30 seconds remain
			default:
				if (RemainingTime >0 && RemainingTime <= 10)
				{
					TimerMessageIndex = (RemainingTime -1);
				}
				break;
		}

		if (TimerMessageIndex >= 0)
		{
			TArray<APlayerController*> PlayerList;
			GEngine->GetAllLocalPlayerControllers(PlayerList);
			for (auto It = PlayerList.CreateIterator(); It; ++It)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(*It);
				if (PC != NULL)
				{
					PC->ClientReceiveLocalizedMessage(UUTTimerMessage::StaticClass(), TimerMessageIndex);			
				}
			}
		}
	}

}

bool AUTGameState::OnSameTeam(const AActor* Actor1, const AActor* Actor2)
{
	const IUTTeamInterface* TeamInterface1 = InterfaceCast<IUTTeamInterface>(Actor1);
	const IUTTeamInterface* TeamInterface2 = InterfaceCast<IUTTeamInterface>(Actor2);
	if (TeamInterface1 == NULL || TeamInterface2 == NULL)
	{
		return false;
	}
	else if (TeamInterface1->IsFriendlyToAll() || TeamInterface2->IsFriendlyToAll())
	{
		return true;
	}
	else
	{
		uint8 TeamNum1 = TeamInterface1->GetTeamNum();
		uint8 TeamNum2 = TeamInterface2->GetTeamNum();

		if (TeamNum1 == 255 || TeamNum2 == 255)
		{
			return false;
		}
		else
		{
			return TeamNum1 == TeamNum2;
		}
	}
}

void AUTGameState::SetTimeLimit(uint32 NewTimeLimit)
{

	UE_LOG(UT,Log,TEXT("=============================="));
	UE_LOG(UT,Log,TEXT("UTGameState.SetTimeLimit %i"),NewTimeLimit);
	UE_LOG(UT,Log,TEXT("=============================="));
	TimeLimit = NewTimeLimit;
	RemainingTime = TimeLimit;
	RemainingMinute = TimeLimit;

	ForceNetUpdate();
}

void AUTGameState::SetGoalScore(uint32 NewGoalScore)
{
	GoalScore = NewGoalScore;
	ForceNetUpdate();
}

void AUTGameState::SetWinner(AUTPlayerState* NewWinner)
{
	WinnerPlayerState = NewWinner;
	WinningTeam	= NewWinner != NULL ?  NewWinner->Team : 0;
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
	FName MatchState = GetMatchState();
	if (MatchState == MatchState::InProgress || MatchState == MatchState::MatchIsInOvertime)
	{
		return true;
	}

	return false;
}

bool AUTGameState::IsMatchInOvertime() const
{
	FName MatchState = GetMatchState();
	if (MatchState == MatchState::MatchEnteringOvertime || MatchState == MatchState::MatchIsInOvertime)
	{
		return true;
	}

	return false;
}

void AUTGameState::OnWinnerReceived()
{
}

FName AUTGameState::OverrideCameraStyle(APlayerController* PCOwner, FName CurrentCameraStyle)
{
	if (Cast<AUTRemoteRedeemer>(PCOwner->GetPawn()) != nullptr)
	{
		return FName(TEXT("FirstPerson"));
	}

	return CurrentCameraStyle;
}

FText AUTGameState::ServerRules()
{
	if (GameModeClass != NULL && GameModeClass->IsChildOf(AUTGameMode::StaticClass()))
	{
		return GameModeClass->GetDefaultObject<AUTGameMode>()->BuildServerRules(this);
	}
	else
	{
		return FText();
	}
}
