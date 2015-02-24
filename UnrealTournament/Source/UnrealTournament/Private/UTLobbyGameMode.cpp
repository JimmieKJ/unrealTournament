
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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



AUTLobbyGameMode::AUTLobbyGameMode(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultPawnClass = NULL;

	// use our custom HUD class
	HUDClass = AUTLobbyHUD::StaticClass();

	GameStateClass = AUTLobbyGameState::StaticClass();
	PlayerStateClass = AUTLobbyPlayerState::StaticClass();
	PlayerControllerClass = AUTLobbyPC::StaticClass();
	DefaultPlayerName = FString("Malcolm");
	GameMessageClass = UUTGameMessage::StaticClass();

	DisplayName = NSLOCTEXT("UTLobbyGameMode", "HUB", "HUB");

}

// Parse options for this game...
void AUTLobbyGameMode::InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage )
{
	UE_LOG(UT,Log,TEXT("===================="));
	UE_LOG(UT,Log,TEXT("  Init Lobby Game"));
	UE_LOG(UT,Log,TEXT("===================="));

	Super::InitGame(MapName, Options, ErrorMessage);

	// I should move this code up in to UTBaseGameMode and probably will (the code hooks are all there) but
	// for right now I want to limit this to just Lobbies.

	MinAllowedRank = FMath::Max(0, GetIntOption( Options, TEXT("MinAllowedRank"), MinAllowedRank));
	if (MinAllowedRank > 0)
	{
		UE_LOG(UT,Log,TEXT("  Minimum Allowed ELO Rank is: %i"), MinAllowedRank)
	}

	MaxAllowedRank = FMath::Max(0, GetIntOption( Options, TEXT("MaxAllowedRank"), MaxAllowedRank));
	if (MaxAllowedRank > 0)
	{
		UE_LOG(UT,Log,TEXT("  Maximum Allowed ELO Rank is: %i"), MaxAllowedRank)
	}
}


void AUTLobbyGameMode::InitGameState()
{
	Super::InitGameState();

	UTLobbyGameState = Cast<AUTLobbyGameState>(GameState);
	if (UTLobbyGameState != NULL)
	{
		// Setupo the beacons to listen for updates from Game Server Instances
		UTLobbyGameState->SetupLobbyBeacons();

		// If there are auto-launch
		if (AutoLaunchGameMode != TEXT("") && AutoLaunchMap != TEXT(""))
		{
			UTLobbyGameState->CreateAutoMatch(AutoLaunchGameMode, AutoLaunchGameOptions, AutoLaunchMap);
		}
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

	SetMatchState(MatchState::InProgress);
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

FString AUTLobbyGameMode::InitNewPlayer(class APlayerController* NewPlayerController, const TSharedPtr<FUniqueNetId>& UniqueId, const FString& Options, const FString& Portal)
{
	FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	
	AUTLobbyPlayerState* PS = Cast<AUTLobbyPlayerState>(NewPlayerController->PlayerState);
	if (PS)
	{	
		FString QuickStartOption = ParseOption(Options, TEXT("QuickStart"));

		UE_LOG(UT,Log,TEXT("##### QUICKSTART %s"), *QuickStartOption);
		if ( QuickStartOption != TEXT("") )
		{
			PS->DesiredQuickStartGameMode = (QuickStartOption.ToLower() == TEXT("CTF")) ? TEXT("/Script/UnrealTournament.UTCTFGameMode") : TEXT("/Script/UnrealTournament.UTDMGameMode");
		}
	}


	return Result;
}


void AUTLobbyGameMode::PostLogin( APlayerController* NewPlayer )
{
	Super::PostLogin(NewPlayer);

	UE_LOG(UT,Log,TEXT("POST LOGIN: %s"), *NewPlayer->PlayerState->GetName());

	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->UpdateGameState();
		}
	}

	AUTLobbyPlayerState* LPS = Cast<AUTLobbyPlayerState>(NewPlayer->PlayerState);
	if (LPS)
	{
		UTLobbyGameState->InitializeNewPlayer(LPS);

		if (LPS->DesiredQuickStartGameMode != TEXT(""))
		{
			for (int32 i=0;i<UTLobbyGameState->GameInstances.Num();i++)
			{
				AUTLobbyMatchInfo* MatchInfo = UTLobbyGameState->GameInstances[i].MatchInfo;
				if (MatchInfo)
				{
					if ( LPS->DesiredQuickStartGameMode == TEXT("") || LPS->DesiredQuickStartGameMode == MatchInfo->MatchGameMode)		
					{
						// Potential match.. see if there is room.
						if (MatchInfo->PlayersInMatchInstance.Num() < MatchInfo->MaxPlayers && MatchInfo->bJoinAnytime)
						{
							LPS->DesiredQuickStartGameMode = TEXT("");
							UTLobbyGameState->JoinMatch(MatchInfo, LPS);
							return;
						}
					}
				}
			}

			// There wasn't a match to play on here

			UTLobbyGameState->QuickStartMatch(LPS, LPS->DesiredQuickStartGameMode == TEXT("/Script/UnrealTournament.UTCTFGameMode"));
		}
	}

	// Set my Initial Presence
	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(NewPlayer);
	if (PC)
	{
		// Set my initial presence....
		PC->ClientSetPresence(TEXT("Sitting in a Hub"), true, true, true, false, false);
	}

}


void AUTLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	AUTLobbyPlayerState* LPS = Cast<AUTLobbyPlayerState>(Exiting->PlayerState);
	if (LPS && LPS->CurrentMatch)
	{
		UTLobbyGameState->RemoveFromAMatch(LPS);
	}

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

FName AUTLobbyGameMode::GetNextChatDestination(AUTPlayerState* PlayerState, FName CurrentChatDestination)
{
	if (CurrentChatDestination == ChatDestinations::Global)
	{
		AUTLobbyPlayerState* PS = Cast<AUTLobbyPlayerState>(PlayerState);
		if (PS && PS->CurrentMatch)
		{
			return ChatDestinations::Match;
		}

		return ChatDestinations::Friends;
	}

	if (CurrentChatDestination == ChatDestinations::Match) return ChatDestinations::Friends;

	return ChatDestinations::Global;
}

void AUTLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage)
{
	if (MinAllowedRank > 0 || MaxAllowedRank > 0)
	{
		int32 PendingRank = GetIntOption(Options, TEXT("Rank"), 0);
		if (MinAllowedRank > 0 && PendingRank < MinAllowedRank)
		{
			ErrorMessage = TEXT("TOOWEAK");
			return;
		}

		if (MaxAllowedRank > 0 && PendingRank > MaxAllowedRank)
		{
			ErrorMessage = TEXT("TOOSTRONG");
			return;
		}
	}

	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
}

int32 AUTLobbyGameMode::GetInstanceData(TArray<FString>& HostNames, TArray<FString>& Descriptions)
{
	if (UTLobbyGameState)
	{
		for (int32 i=0;i<UTLobbyGameState->GameInstances.Num(); i++)
		{
			AUTLobbyMatchInfo* MatchInfo = UTLobbyGameState->GameInstances[i].MatchInfo;

			if (MatchInfo && MatchInfo->CurrentState == ELobbyMatchState::InProgress && (MatchInfo->bDedicatedMatch || MatchInfo->PlayersInMatchInstance.Num() >0))
			{
				if (MatchInfo->PlayersInMatchInstance.Num() > 0)
				{
					HostNames.Add( FString::Printf(TEXT("%s=%s"), *MatchInfo->PlayersInMatchInstance[0].PlayerName, *MatchInfo->PlayersInMatchInstance[0].PlayerID.ToString()));
				}
				else
				{
					HostNames.Add(TEXT("None"));
				}

				Descriptions.Add(MatchInfo->MatchBadge);
			}
		}

		if (HostNames.Num() == Descriptions.Num() && HostNames.Num() != 0)
		{
			return HostNames.Num();
		}

	}
	return 0;
}

int32 AUTLobbyGameMode::GetNumPlayers()
{
	int32 TotalPlayers = NumPlayers;
	for (int32 i=0;i<UTLobbyGameState->AvailableMatches.Num();i++)
	{
		TotalPlayers += UTLobbyGameState->AvailableMatches[i]->PlayersInMatchInstance.Num();
	}

	return TotalPlayers;
}


int32 AUTLobbyGameMode::GetNumMatches()
{
	return UTLobbyGameState->GameInstances.Num();
}