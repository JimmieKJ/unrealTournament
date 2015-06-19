// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GameState.cpp: GameState C++ code.
=============================================================================*/

#include "EnginePrivate.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameState, Log, All);

AGameState::AGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
		.DoNotCreateDefaultSubobject(TEXT("Sprite")) )
{
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	bReplicateMovement = false;

	// Note: this is very important to set to false. Though all replication infos are spawned at run time, during seamless travel
	// they are held on to and brought over into the new world. In ULevel::InitializeNetworkActors, these PlayerStates may be treated as map/startup actors
	// and given static NetGUIDs. This also causes their deletions to be recorded and sent to new clients, which if unlucky due to name conflicts,
	// may end up deleting the new PlayerStates they had just spaned.
	bNetLoadOnClient = false;
	MatchState = MatchState::EnteringMap;
	PreviousMatchState = MatchState::EnteringMap;
}

/** Helper to return the default object of the GameMode class corresponding to this GameState. */
AGameMode* AGameState::GetDefaultGameMode() const
{
	if ( GameModeClass )
	{
		AGameMode* const DefaultGameActor = GameModeClass->GetDefaultObject<AGameMode>();
		return DefaultGameActor;
	}
	return NULL;
}

void AGameState::DefaultTimer()
{
	if (IsMatchInProgress())
	{
		++ElapsedTime;
		if (GetNetMode() != NM_DedicatedServer)
		{
			OnRep_ElapsedTime();
		}
	}

	GetWorldTimerManager().SetTimer(TimerHandle_DefaultTimer, this, &AGameState::DefaultTimer, GetWorldSettings()->GetEffectiveTimeDilation(), true);
}

void AGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	GetWorldTimerManager().SetTimer(TimerHandle_DefaultTimer, this, &AGameState::DefaultTimer, GetWorldSettings()->GetEffectiveTimeDilation(), true);

	UWorld* World = GetWorld();
	World->GameState = this;

	for (TActorIterator<APlayerState> It(World); It; ++It)
	{
		AddPlayerState(*It);
	}
}

void AGameState::OnRep_GameModeClass()
{
	ReceivedGameModeClass();
}

void AGameState::OnRep_SpectatorClass()
{
	ReceivedSpectatorClass();
}

void AGameState::ReceivedGameModeClass()
{
	// Tell each PlayerController that the Game class is here
	for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* const PlayerController = *Iterator;
		if (PlayerController)
		{
			PlayerController->ReceivedGameModeClass(GameModeClass);
		}
	}
}

void AGameState::ReceivedSpectatorClass()
{
	// Tell each PlayerController that the Spectator class is here
	for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
	{
		if (It->PlayerController)
		{
			It->PlayerController->ReceivedSpectatorClass(GameModeClass);
		}
	}
}

void AGameState::SeamlessTravelTransitionCheckpoint(bool bToTransitionMap)
{
	UWorld* World = GetWorld();

	// mark all existing player states as from previous level for various bookkeeping
	for (int32 i = 0; i < World->GameState->PlayerArray.Num(); i++)
	{
		World->GameState->PlayerArray[i]->bFromPreviousLevel = true;
	}
}

void AGameState::AddPlayerState(APlayerState* PlayerState)
{
	// Determine whether it should go in the active or inactive list
	if (!PlayerState->bIsInactive)
	{
		// make sure no duplicates
		for (int32 i=0; i<PlayerArray.Num(); i++)
		{
			if (PlayerArray[i] == PlayerState)
				return;
		}

		PlayerArray.Add(PlayerState);
	}
	else
	{
		// Add once only
		if (InactivePlayerArray.Find(PlayerState) == INDEX_NONE)
		{
			InactivePlayerArray.Add(PlayerState);
		}
	}
}

void AGameState::RemovePlayerState(APlayerState* PlayerState)
{
	for (int32 i=0; i<PlayerArray.Num(); i++)
	{
		if (PlayerArray[i] == PlayerState)
		{
			PlayerArray.RemoveAt(i,1);
			return;
		}
	}
}

void AGameState::HandleMatchIsWaitingToStart()
{
	if (Role != ROLE_Authority)
	{
		// Server handles this in AGameMode::HandleMatchIsWaitingToStart
		GetWorldSettings()->NotifyBeginPlay();
	}
}

void AGameState::HandleMatchHasStarted()
{
	if (Role != ROLE_Authority)
	{
		// Server handles this in AGameMode::HandleMatchHasStarted
		GetWorldSettings()->NotifyMatchStarted();
	}
}

void AGameState::HandleMatchHasEnded()
{

}

void AGameState::HandleLeavingMap()
{

}

bool AGameState::HasMatchStarted() const
{
	if (GetMatchState() == MatchState::EnteringMap || GetMatchState() == MatchState::WaitingToStart)
	{
		return false;
	}

	return true;
}

bool AGameState::IsMatchInProgress() const
{
	if (GetMatchState() == MatchState::InProgress)
	{
		return true;
	}

	return false;
}

bool AGameState::HasMatchEnded() const
{
	if (GetMatchState() == MatchState::WaitingPostMatch || GetMatchState() == MatchState::LeavingMap)
	{
		return true;
	}

	return false;
}

void AGameState::SetMatchState(FName NewState)
{
	if (Role == ROLE_Authority)
	{
		MatchState = NewState;

		// Call the onrep to make sure the callbacks happen
		OnRep_MatchState();
	}
}

void AGameState::OnRep_MatchState()
{
	if (MatchState == MatchState::WaitingToStart || PreviousMatchState == MatchState::EnteringMap)
	{
		// Call MatchIsWaiting to start even if you join in progress at a later state
		HandleMatchIsWaitingToStart();
	}
	
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::WaitingPostMatch)
	{
		HandleMatchHasEnded();
	}
	else if (MatchState == MatchState::LeavingMap)
	{
		HandleLeavingMap();
	}

	PreviousMatchState = MatchState;
}

void AGameState::OnRep_ElapsedTime()
{
	//Blank on purpose
}

bool AGameState::ShouldShowGore() const
{
	return true;
}

void AGameState::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( AGameState, MatchState );
	DOREPLIFETIME( AGameState, SpectatorClass );

	DOREPLIFETIME_CONDITION( AGameState, GameModeClass,	COND_InitialOnly );
	DOREPLIFETIME_CONDITION( AGameState, ElapsedTime,	COND_InitialOnly );
}