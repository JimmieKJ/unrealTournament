// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTServerBeaconLobbyClient.h"
#include "Net/UnrealNetwork.h"
#include "UTGameState.h"
#include "UTLobbyGameState.h"
#include "UTLobbyMatchInfo.h"

AUTServerBeaconLobbyClient::AUTServerBeaconLobbyClient(const class FObjectInitializer& PCIP) :
Super(PCIP)
{
}

void AUTServerBeaconLobbyClient::InitLobbyBeacon(FURL LobbyURL, uint32 LobbyInstanceID, FGuid InstanceGUID, FString AccessKey)
{
	InitClient(LobbyURL);
	GameInstanceID = LobbyInstanceID;
	GameInstanceGUID = InstanceGUID;
	HubKey = AccessKey;
	bDedicatedInstance = !AccessKey.IsEmpty();
}

void AUTServerBeaconLobbyClient::OnConnected()
{
	UE_LOG(UT,Verbose,TEXT("Instance %i [%s] has Connected to the hub (%i)"), GameInstanceID, *GameInstanceGUID.ToString(),bDedicatedInstance);

	AUTGameMode* UTGameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();

	if (bDedicatedInstance)
	{
		UE_LOG(UT,Verbose,TEXT("Becoming a dedicated instances"));
		AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
		int32 NumPlayers = UTGameMode != NULL ? UTGameMode->GameSession->MaxPlayers : 0;
		bool bTeamGame = UTGameMode != NULL ? UTGameMode->bTeamGame : false;
		FString Description = UTGameMode->GetGameRulesDescription();
		Lobby_IsDedicatedInstance(GameInstanceGUID, HubKey, GameState ? GameState->ServerName : TEXT("My Instance"), UTGameMode->DisplayName.ToString(), Description, NumPlayers, bTeamGame );
	}

	if (UTGameMode)
	{
		Lobby_PrimeMapList(GameInstanceID);

		// Try to notify that we are ready.
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(UTGameMode->GameSession);
		if (UTGameSession && UTGameSession->bSessionValid)
		{
			UTGameMode->NotifyLobbyGameIsReady();
		}

	}
}

void AUTServerBeaconLobbyClient::OnFailure()
{
	UE_LOG(UT,Log,TEXT("Instance %i [%s] could not connect to the hub"), GameInstanceID, *GameInstanceGUID.ToString());
}


void AUTServerBeaconLobbyClient::UpdateMatch(const FMatchUpdate& MatchUpdate)
{
	UE_LOG(UT,Verbose,TEXT("UpdateMatch: Instance %i [%s]"), GameInstanceID, *GameInstanceGUID.ToString());
	Lobby_UpdateMatch(GameInstanceID, MatchUpdate);
}

void AUTServerBeaconLobbyClient::UpdatePlayer(AUTBaseGameMode* GameMode, AUTPlayerState* PlayerState, bool bLastUpdate)
{
	int32 RankCheck = PlayerState->GetRankCheck(GameMode);

	FRemotePlayerInfo PlayerInfo(PlayerState,RankCheck);
	UE_LOG(UT,Verbose,TEXT("UpdatePlayer: Instance %i [%s] Player = %s [%s] Score = %i"), GameInstanceID, *GameInstanceGUID.ToString(), *PlayerInfo.PlayerName, *PlayerInfo.PlayerID.ToString(),  PlayerInfo.PlayerScore);

	Lobby_UpdatePlayer(GameInstanceID, PlayerInfo, bLastUpdate);
}

void AUTServerBeaconLobbyClient::EndGame(const FMatchUpdate& FinalMatchUpdate)
{
	Lobby_EndGame(GameInstanceID, FinalMatchUpdate);
}

void AUTServerBeaconLobbyClient::Empty()
{
	UE_LOG(UT,Verbose,TEXT("Empty: Instance %i [%s] %i"), GameInstanceID, *GameInstanceGUID.ToString(), bInstancePendingKill);

	if (!bInstancePendingKill)
	{
		bInstancePendingKill = true;
		Lobby_InstanceEmpty(GameInstanceID);
	}
}


bool AUTServerBeaconLobbyClient::Lobby_NotifyInstanceIsReady_Validate(uint32 InstanceID, FGuid InstanceGUID, const FString& MapName) { return true; }
void AUTServerBeaconLobbyClient::Lobby_NotifyInstanceIsReady_Implementation(uint32 InstanceID, FGuid InstanceGUID, const FString& MapName)
{
	UE_LOG(UT,Verbose,TEXT("[Hub] NotifyInstanceIsReady: Instance %i [%s]"), InstanceID, *InstanceGUID.ToString());
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->GameInstance_Ready(InstanceID, InstanceGUID, MapName, this);
		Instance_ReceiveHubID(LobbyGameState->HubGuid);
	}


}

bool AUTServerBeaconLobbyClient::Lobby_UpdateMatch_Validate(uint32 InstanceID, const FMatchUpdate& MatchUpdate) { return true; }
void AUTServerBeaconLobbyClient::Lobby_UpdateMatch_Implementation(uint32 InstanceID, const FMatchUpdate& MatchUpdate)
{
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->GameInstance_MatchUpdate(InstanceID, MatchUpdate);
	}
}


bool AUTServerBeaconLobbyClient::Lobby_UpdatePlayer_Validate(uint32 InstanceID, FRemotePlayerInfo PlayerInfo, bool bLastUpdate) { return true; }
void AUTServerBeaconLobbyClient::Lobby_UpdatePlayer_Implementation(uint32 InstanceID, FRemotePlayerInfo PlayerInfo, bool bLastUpdate)
{
	UE_LOG(UT,Verbose,TEXT("[Hub] UpdatePlayer: Instance %i PlayerName = %s [%s] Score = %i, bLastUpdate = %i, RankCheck = %i"), InstanceID, *PlayerInfo.PlayerName, *PlayerInfo.PlayerID.ToString(), PlayerInfo.PlayerScore, bLastUpdate, PlayerInfo.RankCheck);
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->GameInstance_PlayerUpdate(InstanceID, PlayerInfo, bLastUpdate);
	}
}



bool AUTServerBeaconLobbyClient::Lobby_EndGame_Validate(uint32 InstanceID, const FMatchUpdate& FinalMatchUpdate) { return true; }
void AUTServerBeaconLobbyClient::Lobby_EndGame_Implementation(uint32 InstanceID, const FMatchUpdate& FinalMatchUpdate)
{
	UE_LOG(UT,Verbose,TEXT("[Hub] EndGame: Instance %i"), InstanceID);
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->GameInstance_EndGame(InstanceID,FinalMatchUpdate);
	}
}

bool AUTServerBeaconLobbyClient::Lobby_InstanceEmpty_Validate(uint32 InstanceID) { return true; }
void AUTServerBeaconLobbyClient::Lobby_InstanceEmpty_Implementation(uint32 InstanceID)
{
	UE_LOG(UT,Verbose,TEXT("[Hub] Empty: Instance %i"), InstanceID);
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->GameInstance_Empty(InstanceID);
	}

}


bool AUTServerBeaconLobbyClient::Lobby_IsDedicatedInstance_Validate(FGuid InstanceGUID, const FString& InHubKey, const FString& ServerName, const FString& ServerGameMode, const FString& ServerDescription, int32 MaxPlayers, bool bTeamGame) { return true; }
void AUTServerBeaconLobbyClient::Lobby_IsDedicatedInstance_Implementation(FGuid InstanceGUID, const FString& InHubKey, const FString& ServerName, const FString& ServerGameMode, const FString& ServerDescription,  int32 MaxPlayers, bool bTeamGame)
{
	UE_LOG(UT, Verbose, TEXT("[Hub] Dedicated Instance (%s) requesting authorization with key %s"), *ServerName, *InHubKey);
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->AuthorizeDedicatedInstance(this, InstanceGUID, InHubKey, ServerName, ServerGameMode, ServerDescription, MaxPlayers, bTeamGame);
	}
}

void AUTServerBeaconLobbyClient::AuthorizeDedicatedInstance_Implementation(FGuid HubGuid, int32 InstanceID)
{
	UE_LOG(UT, Verbose, TEXT("[Hub] This server has been authorized as a dedicated instance with id: %i!"), InstanceID);
	AUTGameMode* CurrentGameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (CurrentGameMode)
	{
		GameInstanceID = InstanceID;
		CurrentGameMode->BecomeDedicatedInstance(HubGuid, InstanceID);
	}
}

bool AUTServerBeaconLobbyClient::Lobby_PrimeMapList_Validate(int32 InstanceID) { return true; }
void AUTServerBeaconLobbyClient::Lobby_PrimeMapList_Implementation(int32 InstanceID)
{
	UE_LOG(UT,Verbose, TEXT("[Hub] Lobby_PrimeMapList - Searching for maps"));
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		AUTLobbyMatchInfo* MatchInfo = NULL;
		
		for (int32 i=0; i < LobbyGameState->GameInstances.Num(); i++)
		{
			if ( LobbyGameState->GameInstances[i].MatchInfo->GameInstanceID == InstanceID )
			{
				MatchInfo = LobbyGameState->GameInstances[i].MatchInfo;
				break;
			}
		}

			if (MatchInfo && MatchInfo->CurrentRuleset.IsValid())
		{
			AllowedMaps.Empty();
			if (MatchInfo->CurrentRuleset->bCustomRuleset)
			{
				AUTGameMode* GameMode = MatchInfo->CurrentRuleset->GetDefaultGameModeObject();
				if (GameMode)
				{
					TArray<FString> Prefixes;
					Prefixes.Add( GameMode->GetMapPrefix() );
					UE_LOG(UT,Verbose, TEXT(" Getting Map List for %i prefixes"), Prefixes.Num());
					if (Prefixes.Num() > 0)
					{
						UE_LOG(UT,Verbose, TEXT("    -- First Prefiox: %s"), *Prefixes[0]);
					}

					LobbyGameState->GetMapList(Prefixes, AllowedMaps, true);
				}
				else
				{
					UE_LOG(UT,Verbose, TEXT("    Using Initial Map"));
					AllowedMaps.Add(MatchInfo->InitialMapInfo.Get());
				}
			}
			else
			{
				UE_LOG(UT,Verbose, TEXT("Using the Rulesets %i"), MatchInfo->CurrentRuleset->MapList.Num());
				AllowedMaps = MatchInfo->CurrentRuleset->MapList;
			}
		}

		
		// Grab the allowed maps
		

		UE_LOG(UT,Verbose, TEXT("[Hub] Lobby_PrimeMapList - GameState returned %i maps"),AllowedMaps.Num());

		if (AllowedMaps.Num() > 0)
		{
			Instance_ReceiveMap(AllowedMaps[0]->MapPackageName, AllowedMaps[0]->Title, AllowedMaps[0]->MapScreenshotReference,0);
		}
		else
		{
			// Immediately send the ban list
			Lobby_RequestFirstBanFromServer(InstanceID);
		}
	}

}

void AUTServerBeaconLobbyClient::Instance_ReceiveMap_Implementation(const FString& MapPackageName, const FString& MapTitle, const FString& MapScreenshotReference, int32 Index)
{
	UE_LOG(UT,Verbose,TEXT("[Instance] ReceiveMap %s [%s] %s %i"), *MapPackageName, *MapTitle, *MapScreenshotReference, Index);
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState)
	{
		UTGameState->CreateMapVoteInfo(MapPackageName, MapTitle, MapScreenshotReference);
	}

	Lobby_SendNextMap(GameInstanceID, Index);
}

bool AUTServerBeaconLobbyClient::Lobby_SendNextMap_Validate(int32 InstanceID, int32 LastIndex) { return true; }
void AUTServerBeaconLobbyClient::Lobby_SendNextMap_Implementation(int32 InstanceID, int32 LastIndex)
{
	UE_LOG(UT,Verbose,TEXT("[Hub] SendNextMap %i"), LastIndex);

	LastIndex++;
	if (AllowedMaps.Num() > LastIndex)
	{
		Instance_ReceiveMap(AllowedMaps[LastIndex]->MapPackageName, AllowedMaps[LastIndex]->Title, AllowedMaps[LastIndex]->MapScreenshotReference,LastIndex);
	}
	else
	{
		// Send the ban list
		Lobby_RequestFirstBanFromServer(InstanceID);
	}

}

void AUTServerBeaconLobbyClient::Instance_ReceiveHubID_Implementation(FGuid HubGuid)
{
	UE_LOG(UT,Verbose,TEXT("[Instance] Instance has received Hub GUID: %s"), *HubGuid.ToString());
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState)
	{
		UTGameState->HubGuid = HubGuid;
	}
}

void AUTServerBeaconLobbyClient::Instance_ReceieveRconMessage_Implementation(const FString& TargetUniqueId, const FString& AdminMessage)
{
	UE_LOG(UT,Verbose,TEXT("[Instance] ReceivedRconMessage: %s"), TargetUniqueId.IsEmpty() ? TEXT("<none>") : *TargetUniqueId, *AdminMessage);
	AUTBaseGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
	if (GameMode)
	{
		GameMode->SendRconMessage(TargetUniqueId, AdminMessage);
	}
}
void AUTServerBeaconLobbyClient::Instance_ReceiveUserMessage_Implementation(const FString& TargetUniqueId, const FString& Message)
{
	UE_LOG(UT,Verbose,TEXT("[Instance] ReceivedUserMessage: %s"), TargetUniqueId.IsEmpty() ? TEXT("<none>") : *TargetUniqueId, *Message);
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AUTBasePlayerController* UTPC = Cast<AUTBasePlayerController>(*Iterator);
		if (UTPC != nullptr)
		{
			if (UTPC->PlayerState && UTPC->PlayerState->UniqueId.ToString() == TargetUniqueId)
			{
				UTPC->ClientSay(NULL, Message, ChatDestinations::Lobby);
			}
		}
	}
}

void AUTServerBeaconLobbyClient::Instance_ForceShutdown_Implementation()
{
	UE_LOG(UT,Verbose,TEXT("[Instance] ForceShutdown"));
	AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GameMode)
	{
		GameMode->SendEveryoneBackToLobby();
		Empty();
	}
}

void AUTServerBeaconLobbyClient::Instance_Kick_Implementation(const FString& TargetUniqueId)
{
	UE_LOG(UT,Verbose,TEXT("[Instance] Kick: %s"), TargetUniqueId.IsEmpty() ? TEXT("<none>") : *TargetUniqueId);
	AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GameMode)
	{
		GameMode->RconKick(TargetUniqueId, false, TEXT(""));
	}
}

void AUTServerBeaconLobbyClient::Instance_AuthorizeAdmin_Implementation(const FString& AdminId, bool bIsAdmin)
{
	UE_LOG(UT,Verbose,TEXT("[Instance] AuthorizeAdmin: %s %i"), AdminId.IsEmpty() ? TEXT("<none>") : *AdminId, bIsAdmin);
	AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GameMode)
	{
		AUTGameSession* Session = Cast<AUTGameSession>(GameMode->GameSession);
		if (Session)
		{
			Session->AcknowledgeAdmin(AdminId, bIsAdmin);
		}
	}
}

void AUTServerBeaconLobbyClient::Lobby_RequestFirstBanFromServer(uint32 InstanceID)
{
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->GetMatchBans(InstanceID, BannedIds);
	}

	UE_LOG(UT,Verbose,TEXT("[Hub] REquestFirstBanFromServer: %i -- %i"), InstanceID, BannedIds.Num());
	if (BannedIds.Num() > 0)
	{
		Instance_ReceiveBan(BannedIds[0], 0, BannedIds.Num(), BannedIds.Num() <= 1);
	}
}

bool AUTServerBeaconLobbyClient::Lobby_RequestNextBanFromServer_Validate(uint32 InstanceID, int32 LastIndex) { return true; }
void AUTServerBeaconLobbyClient::Lobby_RequestNextBanFromServer_Implementation(uint32 InstanceID, int32 LastIndex)
{
	UE_LOG(UT,Verbose,TEXT("[Hub] RequestNextBanFromServer: %i -- %i"), InstanceID, LastIndex);
	LastIndex++;
	if (LastIndex < BannedIds.Num())
	{
		UE_LOG(UT,Verbose,TEXT("    Sending Next Ban: %s %i %i %i"),*BannedIds[LastIndex].ToString(), LastIndex, BannedIds.Num(), (LastIndex == BannedIds.Num()-1));
		Instance_ReceiveBan(BannedIds[LastIndex], LastIndex, BannedIds.Num(), LastIndex == BannedIds.Num()-1);
	}

}

void AUTServerBeaconLobbyClient::Instance_ReceiveBan_Implementation(FUniqueNetIdRepl BanId, int32 Index, int32 Total, bool bFinished)
{
	UE_LOG(UT,Verbose,TEXT("[Instance] AuthorizeAdmin: %s [Index=%i  Total=%i  bFinished=%i]"), *BanId.ToString(), Index, Total, bFinished);
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		GameState->TempBans.Add(BanId.GetUniqueNetId());
	}
	if (!bFinished)
	{
		Lobby_RequestNextBanFromServer(GameInstanceID, Index);
	}
}

bool AUTServerBeaconLobbyClient::Lobby_ReceiveUserMessage_Validate(const FString& Message, const FString&SenderName) { return true; }
void AUTServerBeaconLobbyClient::Lobby_ReceiveUserMessage_Implementation(const FString& Message, const FString&SenderName)
{
	UE_LOG(UT,Verbose,TEXT("[Lobby] ReceivedUserMessage: %s from %s"), Message.IsEmpty() ? TEXT("[none]") : *Message, SenderName.IsEmpty() ? TEXT("[none]") :*SenderName);

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AUTBasePlayerController* UTPC = Cast<AUTBasePlayerController>(*Iterator);
		if (UTPC != nullptr && !Message.IsEmpty() && !SenderName.IsEmpty())
		{
			FString FinalMessage = FString::Printf(TEXT("[%s] %s"), *SenderName, *Message);
			UTPC->ClientSay(nullptr, FinalMessage, ChatDestinations::Lobby);
		}
	}
}

