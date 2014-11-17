// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/PlayerState.h"
#include "UTLobbyGameMode.h"
#include "UTLobbyPlayerState.h"
#include "Net/UnrealNetwork.h"

AUTLobbyPlayerState::AUTLobbyPlayerState(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void AUTLobbyPlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTLobbyPlayerState, CurrentMatch);
}

void AUTLobbyPlayerState::MatchButtonPressed()
{
	if (CurrentMatch == NULL)
	{
		ServerCreateMatch();
	}
	else
	{
		ServerDestroyOrLeaveMatch();
	}
}

bool AUTLobbyPlayerState::ServerCreateMatch_Validate() { return true; }
void AUTLobbyPlayerState::ServerCreateMatch_Implementation()
{
	AUTLobbyGameState* GameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (GameState)
	{
		GameState->AddMatch(this);
	}
}

bool AUTLobbyPlayerState::ServerDestroyOrLeaveMatch_Validate() { return true; }
void AUTLobbyPlayerState::ServerDestroyOrLeaveMatch_Implementation()
{
	AUTLobbyGameState* GameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (GameState)
	{
		GameState->RemoveMatch(this);
	}
}

void AUTLobbyPlayerState::JoinMatch(AUTLobbyMatchInfo* MatchToJoin)
{
	ServerJoinMatch(MatchToJoin);
}

bool AUTLobbyPlayerState::ServerJoinMatch_Validate(AUTLobbyMatchInfo* MatchToJoin) { return true; }
void AUTLobbyPlayerState::ServerJoinMatch_Implementation(AUTLobbyMatchInfo* MatchToJoin)
{
	MatchToJoin->AddPlayer(this,false);
}


void AUTLobbyPlayerState::AddedToMatch(AUTLobbyMatchInfo* Match)
{
	CurrentMatch = Match;
}

void AUTLobbyPlayerState::RemovedFromMatch(AUTLobbyMatchInfo* Match)
{
	CurrentMatch = NULL;
}

void AUTLobbyPlayerState::ClientMatchError_Implementation(const FText &MatchErrorMessage)
{
	AUTBasePlayerController* BasePC = Cast<AUTBasePlayerController>(GetOwner());
	if (BasePC)
	{
#if !UE_SERVER
		BasePC->ShowMessage(NSLOCTEXT("LobbyMessage","MatchMessage","Match Message"), MatchErrorMessage, UTDIALOG_BUTTON_OK, NULL);	
#endif
	}
}
