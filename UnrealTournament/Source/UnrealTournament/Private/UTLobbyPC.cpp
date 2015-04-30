// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/PlayerController.h"
#include "UTLobbyGameMode.h"
#include "UTLobbyHUD.h"
#include "UTLocalPlayer.h"
#include "UTLobbyPlayerState.h"
#include "UTLobbyPC.h"
#include "UTLobbyMatchInfo.h"
#include "UTCharacterMovement.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "Online.h"
#include "UTOnlineGameSearchBase.h"
#include "OnlineSubsystemTypes.h"

AUTLobbyPC::AUTLobbyPC(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/* Cache a copy of the PlayerState cast'd to AUTPlayerState for easy reference.  Do it both here and when the replicated copy of APlayerState arrives in OnRep_PlayerState */
void AUTLobbyPC::InitPlayerState()
{
	Super::InitPlayerState();
	UTLobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerState);
	UTLobbyPlayerState->ChatDestination = ChatDestinations::Global;
}

void AUTLobbyPC::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	UTLobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerState);

	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP)
	{
		if (!LP->IsInSession())
		{
			LP->MessageBox(NSLOCTEXT("UTLobbyPC","SessionErrorTitle","Session Error"), NSLOCTEXT("UTLobbyPC","SessionErrorMsg","You can not connect directly to a Hub.  Please select 'PLAY' then 'Find a Game...' from the main menu."));
			ConsoleCommand("Disconnect");
		}
		else
		{
			LP->UpdatePresence(TEXT("In Hub"), true, true, true, false, false);
		}
	}
}

void AUTLobbyPC::PlayerTick( float DeltaTime )
{
	Super::PlayerTick(DeltaTime);

	if (!bInitialReplicationCompleted)
	{
		if (UTLobbyPlayerState && GetWorld()->GetGameState())
		{
			bInitialReplicationCompleted = true;
			UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
			if (LP)
			{
				LP->ShowMenu();
			}
		}
	}
}

void AUTLobbyPC::SetName(const FString& S)
{
}


void  AUTLobbyPC::ServerRestartPlayer_Implementation()
{
}

bool AUTLobbyPC::CanRestartPlayer()
{
	return false;
}


void AUTLobbyPC::MatchChat(FString Message)
{
	Chat(ChatDestinations::Match, Message);
}

void AUTLobbyPC::GlobalChat(FString Message)
{
	Chat(ChatDestinations::Global, Message);
}


void AUTLobbyPC::Chat(FName Destination, FString Message)
{
	// Send the Chat to the server so it can be routed to the right place.  
	// TODO - Once we have MCP friends support, look at routing friends chat directly through the MCP

	Message = Message.Left(128);
	ServerChat(Destination, Message);
}

void AUTLobbyPC::ServerChat_Implementation(const FName Destination, const FString& Message)
{
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->BroadcastChat(UTLobbyPlayerState, Destination, Message);

	}
}

bool AUTLobbyPC::ServerChat_Validate(const FName Destination, const FString& Message)
{
	return true;
}

void AUTLobbyPC::ReceivedPlayer()
{
}


void AUTLobbyPC::ServerDebugTest_Implementation(const FString& TestCommand)
{
}

bool AUTLobbyPC::ServerSetReady_Validate(uint32 bNewReadyToPlay) { return true; }
void AUTLobbyPC::ServerSetReady_Implementation(uint32 bNewReadyToPlay)
{
	UTLobbyPlayerState->bReadyToPlay = bNewReadyToPlay;
}

void AUTLobbyPC::SetLobbyDebugLevel(int32 NewLevel)
{
	AUTLobbyHUD* H = Cast<AUTLobbyHUD>(MyHUD);
	if (H) H->LobbyDebugLevel = NewLevel;
}

void AUTLobbyPC::MatchChanged(AUTLobbyMatchInfo* CurrentMatch)
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP)
	{
		LP->UpdatePresence(CurrentMatch == NULL ? TEXT("In Hub") : TEXT("Setting up a Match"), true, true, true, false, false);
	}
}

void AUTLobbyPC::HandleNetworkFailureMessage(enum ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// If we are in a match and we get a network failure, leave that match.
	UTLobbyPlayerState->ServerDestroyOrLeaveMatch();	
}
