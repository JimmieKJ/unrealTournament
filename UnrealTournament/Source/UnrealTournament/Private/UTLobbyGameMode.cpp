
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/GameMode.h"
#include "UTLobbyGameMode.h"
#include "UTLobbyGameState.h"
#include "UTLobbyPC.h"
#include "UTLobbyPlayerState.h"
#include "UTLobbyHUD.h"
#include "UTGameMessage.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

namespace ChatDestinations
{
	const FName GlobalChat = FName(TEXT("CHAT_Global"));
	const FName PrivateLocal = FName(TEXT("CHAT_LocalPrivate"));
	const FName CurrentMatch = FName(TEXT("CHAT_CurrentMatch"));
	const FName Friends = FName(TEXT("CHAT_Friends"));
}


AUTLobbyGameMode::AUTLobbyGameMode(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// set default pawn class to our Blueprinted character

	static ConstructorHelpers::FObjectFinder<UClass> PlayerPawnObject(TEXT("Class'/Game/RestrictedAssets/Blueprints/WIP/Steve/SteveUTCharacter.SteveUTCharacter_C'"));

	DefaultPawnClass = NULL;

	// use our custom HUD class
	HUDClass = AUTHUD::StaticClass();

	GameStateClass = AUTLobbyGameState::StaticClass();
	PlayerStateClass = AUTLobbyPlayerState::StaticClass();
	PlayerControllerClass = AUTLobbyPC::StaticClass();
	DefaultPlayerName = FString("Malcolm");
	GameMessageClass = UUTGameMessage::StaticClass();

}

// Parse options for this game...
void AUTLobbyGameMode::InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage )
{
	UE_LOG(UT,Log,TEXT("===================="));
	UE_LOG(UT,Log,TEXT("  Init Lobby Game"));
	UE_LOG(UT,Log,TEXT("===================="));

	Super::InitGame(MapName, Options, ErrorMessage);
}


void AUTLobbyGameMode::InitGameState()
{
	Super::InitGameState();

	UTLobbyGameState = Cast<AUTLobbyGameState>(GameState);
	if (UTLobbyGameState != NULL)
	{
	}
	else
	{
		UE_LOG(UT,Error, TEXT("UTLobbyGameState is NULL %s"), *GameStateClass->GetFullName());
	}

	// Register our Lobby with the lobby Server
	if (GameSession != NULL && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		GameSession->RegisterServer();
	}
}

void AUTLobbyGameMode::StartMatch()
{
	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->StartMatch();
		}
	}
}

void AUTLobbyGameMode::RestartPlayer(AController* aPlayer)
{
	return;
}

void AUTLobbyGameMode::ChangeName(AController* Other, const FString& S, bool bNameChange)
{
	// Cap player name's at 15 characters...
	FString SMod = S;
	if (SMod.Len()>15)
	{
		SMod = SMod.Left(15);
	}

    if ( !Other->PlayerState|| FCString::Stricmp(*Other->PlayerState->PlayerName, *SMod) == 0 )
    {
		return;
	}

	// Look to see if someone else is using the the new name
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState && FCString::Stricmp(*Controller->PlayerState->PlayerName, *SMod) == 0)
		{
			if ( Cast<APlayerController>(Other) != NULL )
			{
					Cast<APlayerController>(Other)->ClientReceiveLocalizedMessage( GameMessageClass, 5 );
					if ( FCString::Stricmp(*Other->PlayerState->PlayerName, *DefaultPlayerName) == 0 )
					{
						Other->PlayerState->SetPlayerName(FString::Printf(TEXT("%s%i"), *DefaultPlayerName, Other->PlayerState->PlayerId));
					}
				return;
			}
		}
	}

    Other->PlayerState->SetPlayerName(SMod);
}

void AUTLobbyGameMode::OverridePlayerState(APlayerController* PC, APlayerState* OldPlayerState)
{
	Super::OverridePlayerState(PC, OldPlayerState);

	// if we're in this function GameMode swapped PlayerState objects so we need to update the precasted copy
	AUTLobbyPC* UTPC = Cast<AUTLobbyPC>(PC);
	if (UTPC != NULL)
	{
		UTPC->UTLobbyPlayerState = Cast<AUTLobbyPlayerState>(UTPC->PlayerState);
	}
}

void AUTLobbyGameMode::GenericPlayerInitialization(AController* C)
{
}

void AUTLobbyGameMode::PostLogin( APlayerController* NewPlayer )
{
	Super::PostLogin(NewPlayer);

	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->UpdateGameState();
		}
	}
}


void AUTLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->UpdateGameState();
		}
	}
}

bool AUTLobbyGameMode::PlayerCanRestart( APlayerController* Player )
{
	return false;
}


TSubclassOf<AGameSession> AUTLobbyGameMode::GetGameSessionClass() const
{
	return AUTGameSession::StaticClass();
}

void AUTLobbyGameMode::Destroyed()
{
	UE_LOG(UT,Log,TEXT("DESTROYED"));
	UE_LOG(UT,Log,TEXT("DESTROYED"));
	UE_LOG(UT,Log,TEXT("DESTROYED"));
	UE_LOG(UT,Log,TEXT("DESTROYED"));
	UE_LOG(UT,Log,TEXT("DESTROYED"));
	UE_LOG(UT,Log,TEXT("DESTROYED"));
	UE_LOG(UT,Log,TEXT("DESTROYED"));
	UE_LOG(UT,Log,TEXT("DESTROYED"));
	
	Super::Destroyed();
}
