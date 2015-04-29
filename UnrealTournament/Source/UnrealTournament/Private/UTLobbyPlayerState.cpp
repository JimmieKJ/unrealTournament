// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/PlayerState.h"
#include "UTLobbyGameMode.h"
#include "UTLobbyPlayerState.h"
#include "Net/UnrealNetwork.h"

AUTLobbyPlayerState::AUTLobbyPlayerState(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void AUTLobbyPlayerState::PreInitializeComponents()
{
	DesiredQuickStartGameMode = TEXT("");
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
		// TODO Check to make sure the rulesets are replicated

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
	if (CurrentMatch == NULL)
	{
		AUTLobbyGameState* GameState = GetWorld()->GetGameState<AUTLobbyGameState>();
		if (GameState)
		{
			if (GameState->AvailableGameRulesets.Num() >0)
			{
				GameState->AddNewMatch(this);
			}
			else
			{
				ClientMatchError(NSLOCTEXT("LobbyMessage","MatchRuleError","The server does not have any rulesets defined.  Please notify the server admin."));
			}
		}
	}
}

bool AUTLobbyPlayerState::ServerDestroyOrLeaveMatch_Validate() { return true; }
void AUTLobbyPlayerState::ServerDestroyOrLeaveMatch_Implementation()
{
	AUTLobbyGameState* GameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (GameState)
	{
		GameState->RemoveFromAMatch(this);
	}
}

bool AUTLobbyPlayerState::ServerJoinMatch_Validate(AUTLobbyMatchInfo* MatchToJoin, bool bAsSpectator) { return true; }
void AUTLobbyPlayerState::ServerJoinMatch_Implementation(AUTLobbyMatchInfo* MatchToJoin, bool bAsSpectator)
{
	// CurrentMatch may have been set by a previous call if client is lagged and sends multiple requests
	if (CurrentMatch != MatchToJoin)
	{
		AUTLobbyGameState* GameState = GetWorld()->GetGameState<AUTLobbyGameState>();
		if (GameState != NULL)
		{
			if (CurrentMatch != NULL)
			{
				GameState->RemoveFromAMatch(this);
			}
			GameState->JoinMatch(MatchToJoin, this, bAsSpectator);
		}
	}
}


void AUTLobbyPlayerState::AddedToMatch(AUTLobbyMatchInfo* Match)
{
	CurrentMatch = Match;
	bIsInMatch = true;
}

void AUTLobbyPlayerState::RemovedFromMatch(AUTLobbyMatchInfo* Match)
{
	CurrentMatch = NULL;
	bIsInMatch = false;
	bReadyToPlay = false;
}

void AUTLobbyPlayerState::ClientMatchError_Implementation(const FText &MatchErrorMessage, int32 OptionalInt)
{
	AUTBasePlayerController* BasePC = Cast<AUTBasePlayerController>(GetOwner());
	if (BasePC)
	{
#if !UE_SERVER
		BasePC->ShowMessage(NSLOCTEXT("LobbyMessage","MatchMessage","Match Message"), FText::Format(MatchErrorMessage, FText::AsNumber(OptionalInt)), UTDIALOG_BUTTON_OK, NULL);	
#endif
	}
}

void AUTLobbyPlayerState::ClientConnectToInstance_Implementation(const FString& GameInstanceGUIDString, const FString& LobbyGUIDString, bool bAsSpectator)
{
	AUTBasePlayerController* BPC = Cast<AUTBasePlayerController>(GetOwner());
	if (BPC)
	{
		UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(BPC->Player);
		if (LocalPlayer)
		{
			LocalPlayer->RememberLobby(LobbyGUIDString);
		}

		BPC->ConnectToServerViaGUID(GameInstanceGUIDString, bAsSpectator);
	}
}

void AUTLobbyPlayerState::OnRep_CurrentMatch()
{
	bIsInMatch = CurrentMatch != NULL;
	AUTLobbyPC* PC = Cast<AUTLobbyPC>(GetOwner());
	if (PC)
	{
		PC->MatchChanged(CurrentMatch);
	}

#if !UE_SERVER
	if (CurrentMatchChangedDelegate.IsBound())
	{
		CurrentMatchChangedDelegate.Execute(this);
	}

#endif
}
