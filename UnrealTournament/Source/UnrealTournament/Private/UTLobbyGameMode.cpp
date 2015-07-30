
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
	MinPlayersToStart = 2;

	// use our custom HUD class
	HUDClass = AUTLobbyHUD::StaticClass();

	GameStateClass = AUTLobbyGameState::StaticClass();
	PlayerStateClass = AUTLobbyPlayerState::StaticClass();
	PlayerControllerClass = AUTLobbyPC::StaticClass();
	DefaultPlayerName = FText::FromString(TEXT("Player"));
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

	MinPlayersToStart = FMath::Max(1, GetIntOption(Options, TEXT("MinPlayers"), MinPlayersToStart));

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
		UTLobbyGameState->HubGuid = ServerInstanceGUID;

		// Setupo the beacons to listen for updates from Game Server Instances
		UTLobbyGameState->SetupLobbyBeacons();

		// Break the MOTD up in to strings to be sent to clients when they login.
		FString Converted = UTLobbyGameState->ServerMOTD.Replace( TEXT("\\n"), TEXT("\n"));
		Converted.ParseIntoArray(ParsedMOTD,TEXT("\n"),true);
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
					if ( FCString::Stricmp(*Other->PlayerState->PlayerName, *DefaultPlayerName.ToString()) == 0 )
					{
						Other->PlayerState->SetPlayerName(FString::Printf(TEXT("%s%i"), *DefaultPlayerName.ToString(), Other->PlayerState->PlayerId));
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

		if ( QuickStartOption != TEXT("") )
		{
			PS->DesiredQuickStartGameMode = (QuickStartOption.ToLower() == TEXT("CTF")) ? FQuickMatchTypeRulesetTag::CTF : FQuickMatchTypeRulesetTag::DM;
		}

		FString FriendID = ParseOption(Options, TEXT("Friend"));
		if (FriendID != TEXT(""))
		{
			PS->DesiredFriendToJoin = FriendID;
		}

		PS->bReturnedFromMatch = HasOption(Options,"RTM");
	}


	return Result;
}


void AUTLobbyGameMode::PostLogin( APlayerController* NewPlayer )
{
	Super::PostLogin(NewPlayer);

	UpdateLobbySession();

	AUTLobbyPlayerState* LPS = Cast<AUTLobbyPlayerState>(NewPlayer->PlayerState);
	if (LPS)
	{
		UTLobbyGameState->InitializeNewPlayer(LPS);

		if (!LPS->DesiredQuickStartGameMode.IsEmpty())
		{
			for (int32 i=0;i<UTLobbyGameState->GameInstances.Num();i++)
			{
				AUTLobbyMatchInfo* MatchInfo = UTLobbyGameState->GameInstances[i].MatchInfo;
				if (MatchInfo && (MatchInfo->CurrentState == ELobbyMatchState::Setup || (MatchInfo->CurrentState == ELobbyMatchState::InProgress && MatchInfo->bJoinAnytime)))
				{
					if ( LPS->DesiredQuickStartGameMode.Equals(MatchInfo->CurrentRuleset->UniqueTag, ESearchCase::IgnoreCase))
					{
						// Potential match.. see if there is room.
						//if (MatchInfo->PlayersInMatchInstance.Num() < MatchInfo->MaxPlayers && MatchInfo->bJoinAnytime)
						{
							LPS->DesiredQuickStartGameMode = TEXT("");
							UTLobbyGameState->JoinMatch(MatchInfo, LPS);
							return;
						}
					}
				}
			}

			// There wasn't a match to play on here

			UTLobbyGameState->QuickStartMatch(LPS, LPS->DesiredQuickStartGameMode.Equals(FQuickMatchTypeRulesetTag::CTF, ESearchCase::IgnoreCase));
		}
	}
	// Set my Initial Presence
	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(NewPlayer);
	if (PC)
	{
		for (int32 i = 0; i < ParsedMOTD.Num(); i++)
		{
			PC->ClientSay(NULL, ParsedMOTD[i], ChatDestinations::MOTD);
		}

		// Set my initial presence....
		PC->ClientSetPresence(TEXT("Sitting in a Hub"), true, true, true, false);
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

	UpdateLobbySession();
}

bool AUTLobbyGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
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

int32 AUTLobbyGameMode::GetInstanceData(TArray<FGuid>& InstanceIDs)
{
	InstanceIDs.Empty();
	if (UTLobbyGameState)
	{
		for (int32 i=0;i<UTLobbyGameState->GameInstances.Num(); i++)
		{
			AUTLobbyMatchInfo* MatchInfo = UTLobbyGameState->GameInstances[i].MatchInfo;

			if (MatchInfo && !MatchInfo->bDedicatedMatch && MatchInfo->ShouldShowInDock())
			{
				InstanceIDs.Add(MatchInfo->UniqueMatchID);
			}
		}

		for (int32 i=0; i < UTLobbyGameState->AvailableMatches.Num();i++)
		{
			AUTLobbyMatchInfo* MatchInfo = UTLobbyGameState->AvailableMatches[i];

			if (MatchInfo && !MatchInfo->bDedicatedMatch && MatchInfo->ShouldShowInDock())
			{
				InstanceIDs.Add(MatchInfo->UniqueMatchID);
			}
		}
	}
	return InstanceIDs.Num();
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
	int32 Cnt = 0;
	if (UTLobbyGameState && UTLobbyGameState->GameInstances.Num())
	{
		for (int32 i = 0; i < UTLobbyGameState->GameInstances.Num(); i++)
		{
			if (UTLobbyGameState->GameInstances[i].MatchInfo && UTLobbyGameState->GameInstances[i].MatchInfo->CurrentState == ELobbyMatchState::InProgress)
			{
				Cnt++;
			}
		}
	}

	return Cnt;
}

void AUTLobbyGameMode::UpdateLobbySession()
{
	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->UpdateGameState();
		}
	}
}

// Lobbies don't want to track inactive players.  We match up players based on a GUID.  
void AUTLobbyGameMode::AddInactivePlayer(APlayerState* PlayerState, APlayerController* PC)
{
	PlayerState->Destroy();
	return;
}

bool AUTLobbyGameMode::IsHandlingReplays()
{
	// No replays for HUB
	return false;
}

void AUTLobbyGameMode::DefaultTimer()
{
	if (GetWorld()->GetTimeSeconds() > ServerRefreshCheckpoint * 60)
	{
		if (NumPlayers == 0)
		{

			bool bOkToRestart = true;

			// Look to see if there are any non dedicated instances left standing.
			for (int32 i=0; i < UTLobbyGameState->GameInstances.Num(); i++)
			{
				if (!UTLobbyGameState->GameInstances[i].MatchInfo->bDedicatedMatch)
				{
					bOkToRestart = false;
					break;
				}
			}

			if (bOkToRestart)
			{
				FString MapName = GetWorld()->GetMapName();
				if ( FPackageName::IsShortPackageName(MapName) )
				{
					FPackageName::SearchForPackageOnDisk(MapName, &MapName); 
				}

				GetWorld()->ServerTravel(MapName);
			}
		}
	}
}

