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

	MaxPlayersInLobby=200;

	DisplayName = NSLOCTEXT("UTLobbyGameMode", "HUB", "HUB");
}

// Parse options for this game...
void AUTLobbyGameMode::InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage )
{
	UE_LOG(UT,Log,TEXT("===================="));
	UE_LOG(UT,Log,TEXT("  Init Lobby Game"));
	UE_LOG(UT,Log,TEXT("===================="));

	GetWorld()->bShouldSimulatePhysics = false;

	Super::InitGame(MapName, Options, ErrorMessage);

	if (GameSession)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession)
		{
			UTGameSession->MaxPlayers = MaxPlayersInLobby;
		}
	}

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
		UTLobbyGameState->bTrainingGround = bTrainingGround;
		UTLobbyGameState->HubGuid = ServerInstanceGUID;

		// Setupo the beacons to listen for updates from Game Server Instances
		UTLobbyGameState->SetupLobbyBeacons();
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
			PS->DesiredQuickStartGameMode = (QuickStartOption.ToLower() == TEXT("CTF")) ? EEpicDefaultRuleTags::CTF : EEpicDefaultRuleTags::Deathmatch;
		}

		FString MatchId = ParseOption(Options, TEXT("MatchId"));
		if (!MatchId.IsEmpty())
		{
			PS->DesiredMatchIdToJoin = MatchId;
			
			if (GetIntOption(Options, TEXT("SpectatorOnly"), 0) > 0)
			{
				PS->DesiredTeamNum=255;
			}
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
	}
	// Set my Initial Presence
	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(NewPlayer);
	if (PC)
	{
		PC->ClientSay(NULL, UTLobbyGameState->ServerMOTD, ChatDestinations::MOTD);
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

void AUTLobbyGameMode::GetInstanceData(TArray<TSharedPtr<FServerInstanceData>>& InstanceData)
{
	if (UTLobbyGameState)
	{
		for (int32 i=0; i < UTLobbyGameState->AvailableMatches.Num();i++)
		{
			AUTLobbyMatchInfo* MatchInfo = UTLobbyGameState->AvailableMatches[i];

			if (MatchInfo && MatchInfo->ShouldShowInDock())
			{
				int32 NumPlayers = MatchInfo->NumPlayersInMatch();
				TSharedPtr<FServerInstanceData> Data;
				if (MatchInfo->bDedicatedMatch)
				{
					FString Map = FString::Printf(TEXT("%s (%s)"), *MatchInfo->InitialMap, *MatchInfo->DedicatedServerGameMode);
					Data = FServerInstanceData::Make(MatchInfo->UniqueMatchID, MatchInfo->DedicatedServerName, TEXT(""), Map, MatchInfo->DedicatedServerMaxPlayers, MatchInfo->GetMatchFlags(), 1500, false, MatchInfo->bJoinAnytime || !MatchInfo->IsInProgress(), MatchInfo->bSpectatable, MatchInfo->DedicatedServerDescription, TEXT(""), false);
				}
				else
				{
					Data = FServerInstanceData::Make(MatchInfo->UniqueMatchID, MatchInfo->CurrentRuleset->Title, MatchInfo->CurrentRuleset->UniqueTag, (MatchInfo->InitialMapInfo.IsValid() ? MatchInfo->InitialMapInfo->Title : MatchInfo->InitialMap), MatchInfo->CurrentRuleset->MaxPlayers, MatchInfo->GetMatchFlags(), MatchInfo->AverageRank, MatchInfo->CurrentRuleset->bTeamGame, MatchInfo->bJoinAnytime || !MatchInfo->IsInProgress(), MatchInfo->bSpectatable, MatchInfo->CurrentRuleset->Description, TEXT(""), MatchInfo->bQuickPlayMatch);
				}

				Data->MatchData = MatchInfo->MatchUpdate;

				// If this match hasn't started yet, the MatchUpdate will be empty.  So we have to manually fix up the # of players in the match.
				if (!MatchInfo->IsInProgress())
				{
					Data->SetNumPlayers(MatchInfo->NumPlayersInMatch());
					Data->SetNumSpectators(MatchInfo->NumSpectatorsInMatch());
				}

				MatchInfo->GetPlayerData(Data->Players);
				InstanceData.Add(Data);
			}
		}
	}
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
			if (UTLobbyGameState->GameInstances[i].MatchInfo && ( UTLobbyGameState->GameInstances[i].MatchInfo->CurrentState == ELobbyMatchState::InProgress || UTLobbyGameState->GameInstances[i].MatchInfo->CurrentState == ELobbyMatchState::Launching) )
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
	if (GetWorld()->GetTimeSeconds() > ServerRefreshCheckpoint * 60 * 60)
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

				AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
				if (UTGameSession)
				{
					// kill the host beacon before we start the travel so hopefully the port will be released before
					// we are done.
					UTGameSession->DestroyHostBeacon();
				}

				GetWorld()->ServerTravel(MapName);
			}
		}
	}
}

void AUTLobbyGameMode::SendRconMessage(const FString& DestinationId, const FString &Message)
{	
	Super::SendRconMessage(DestinationId, Message);
	if (UTLobbyGameState)
	{
		for (int32 i=0; i < UTLobbyGameState->AvailableMatches.Num(); i++)
		{
			if (UTLobbyGameState->AvailableMatches[i]->InstanceBeacon)
			{
				UTLobbyGameState->AvailableMatches[i]->InstanceBeacon->Instance_ReceieveRconMessage(DestinationId, Message);
			}
		}
	}
}

void AUTLobbyGameMode::RconKick(const FString& NameOrUIDStr, bool bBan, const FString& Reason)
{
	if (UTLobbyGameState)
	{
		for (int32 i=0; i < UTLobbyGameState->AvailableMatches.Num(); i++)
		{
			if (UTLobbyGameState->AvailableMatches[i]->InstanceBeacon)
			{
				UTLobbyGameState->AvailableMatches[i]->InstanceBeacon->Instance_Kick(NameOrUIDStr);
			}
		}
	}
	Super::RconKick(NameOrUIDStr, bBan, Reason);
}

void AUTLobbyGameMode::RconAuth(AUTBasePlayerController* Admin, const FString& Password)
{
	Super::RconAuth(Admin, Password);
	if (Admin && Admin->UTPlayerState && Admin->UTPlayerState->bIsRconAdmin)
	{
		for (int32 i=0; i < UTLobbyGameState->AvailableMatches.Num(); i++)
		{
			if (UTLobbyGameState->AvailableMatches[i]->InstanceBeacon)
			{
				UTLobbyGameState->AvailableMatches[i]->InstanceBeacon->Instance_AuthorizeAdmin(Admin->UTPlayerState->UniqueId.ToString(), true);
			}
		}
	}

}

void AUTLobbyGameMode::RconNormal(AUTBasePlayerController* Admin)
{
	Super::RconNormal(Admin);

	if (Admin && Admin->UTPlayerState && !Admin->UTPlayerState->bIsRconAdmin)
	{
		for (int32 i=0; i < UTLobbyGameState->AvailableMatches.Num(); i++)
		{
			if (UTLobbyGameState->AvailableMatches[i]->InstanceBeacon)
			{
				UTLobbyGameState->AvailableMatches[i]->InstanceBeacon->Instance_AuthorizeAdmin(Admin->UTPlayerState->UniqueId.ToString(),false);
			}
		}
	}
}
