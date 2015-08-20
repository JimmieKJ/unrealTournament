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
			LP->UpdatePresence(TEXT("In Hub"), true, true, true, false);
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
	UUTLocalPlayer* UTLocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (UTLocalPlayer)
	{
		ServerSetAvatar(UTLocalPlayer->GetAvatar());
	}
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
		LP->UpdatePresence(CurrentMatch == NULL ? TEXT("In Hub") : TEXT("Setting up a Match"), true, true, true, false);
	}
}

void AUTLobbyPC::HandleNetworkFailureMessage(enum ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// If we are in a match and we get a network failure, leave that match.
	UTLobbyPlayerState->ServerDestroyOrLeaveMatch();	
}

void AUTLobbyPC::Say(FString Message)
{
	if ( Message.Equals(TEXT("@match"),ESearchCase::IgnoreCase) )
	{
		if (UTLobbyPlayerState->CurrentMatch)
		{
			UE_LOG(UT,Log,TEXT("======================================="));
			UE_LOG(UT,Log,TEXT("Current Match [%s]"), *UTLobbyPlayerState->CurrentMatch->CurrentState.ToString());
			UE_LOG(UT,Log,TEXT("======================================="));
			if (UTLobbyPlayerState->CurrentMatch->CurrentRuleset.IsValid())
			{
				UE_LOG(UT,Log,TEXT("--- Ruleset [%s]"), *UTLobbyPlayerState->CurrentMatch->CurrentRuleset->Title);
			}
			else
			{
				UE_LOG(UT,Log,TEXT("--- Ruleset is invalid"));
			}

			for (int32 i=0; i < UTLobbyPlayerState->CurrentMatch->Players.Num();i++)
			{
				if (UTLobbyPlayerState->CurrentMatch->Players[i].IsValid())
				{
					UE_LOG(UT,Log,TEXT("--- Player %i = %s (%i)"), i, *UTLobbyPlayerState->CurrentMatch->Players[i]->PlayerName , UTLobbyPlayerState->CurrentMatch->Players[i]->AverageRank);
				}
			}

		}
		return;
	}
	if (Message.Equals(TEXT("@maps"),ESearchCase::IgnoreCase) )
	{
		UE_LOG(UT,Log,TEXT("======================================="));
		UE_LOG(UT,Log,TEXT("Maps on Server"));
		UE_LOG(UT,Log,TEXT("======================================="));

		AUTLobbyGameState* GameState = GetWorld()->GetGameState<AUTLobbyGameState>();
		if (GameState)
		{
			for (int32 i = 0; i < GameState->AllMapsOnServer.Num(); i++)
			{
				FString Title = GameState->AllMapsOnServer[i]->Title;  if (Title.IsEmpty()) Title = TEXT("none");
				FString Package = GameState->AllMapsOnServer[i]->Redirect.PackageName; if (Package.IsEmpty()) Package = TEXT("none");
				FString Url = GameState->AllMapsOnServer[i]->Redirect.PackageURL; if (Url.IsEmpty()) Url = TEXT("none");
				FString MD5 = GameState->AllMapsOnServer[i]->Redirect.PackageChecksum; if (MD5.IsEmpty()) MD5 = TEXT("none");
				UE_LOG(UT,Log,TEXT("  Map Title: %s  Redirect: %s / %s / %s"), *Title, *Package, *Url,*MD5);
			}
		}
		return;
	}

	Super::Say(Message);

}