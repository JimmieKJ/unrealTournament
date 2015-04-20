// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/GameState.h"
#include "UTGameState.h"
#include "UTLobbyMatchInfo.h"
#include "UTLobbyGameMode.h"
#include "Net/UnrealNetwork.h"
#include "UTEpicDefaultRulesets.h"

AUTLobbyGameState::AUTLobbyGameState(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void AUTLobbyGameState::BeginPlay()
{
	Super::BeginPlay();
	ServerMOTD = ServerMOTD.Replace(TEXT("\\n"), TEXT("\n"), ESearchCase::IgnoreCase);
}

void AUTLobbyGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTLobbyGameState, AvailableMatches);
	DOREPLIFETIME(AUTLobbyGameState, AvailableGameRulesets);
	DOREPLIFETIME(AUTLobbyGameState, AvailabelGameRulesetCount);
}

void AUTLobbyGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (Role == ROLE_Authority)
	{
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTLobbyGameState::CheckInstanceHealth, 60.0f, true);	
	}

	// Set the Epic official rules.  For now it's hard coded.  In time this will come from the MCP.

	UUTEpicDefaultRulesets::GetDefaultRules(this, AvailableGameRulesets);

	// Load custom server rule sets.  NOTE: Custom, rules can not have the same unique tag or title as a default rule

	for (int32 i=0; i < AllowedGameRulesets.Num(); i++)
	{
		if (!AllowedGameRulesets[i].IsEmpty())
		{
			UUTGameRuleset* NewRuleset = ConstructObject<UUTGameRuleset>(UUTGameRuleset::StaticClass(), GetTransientPackage(), *AllowedGameRulesets[i]);
			if (NewRuleset)
			{
				bool bExistsAlready = false;
				for (int32 j=0; j < AvailableGameRulesets.Num(); j++)
				{
					if ( AvailableGameRulesets[j]->UniqueTag.Equals(NewRuleset->UniqueTag, ESearchCase::IgnoreCase) || AvailableGameRulesets[j]->Title.ToLower() == NewRuleset->Title.ToLower() )
					{
						bExistsAlready = true;
						break;
					}
				}

				if (!bExistsAlready)
				{
					FActorSpawnParameters Params;
					Params.Name = FName(*AllowedGameRulesets[i]);
					Params.Owner = this;
					AUTReplicatedGameRuleset* NewReplicatedRuleset = GetWorld()->SpawnActor<AUTReplicatedGameRuleset>(Params);
					if (NewReplicatedRuleset)
					{
						NewReplicatedRuleset->SetRules(NewRuleset);
						AvailableGameRulesets.Add(NewReplicatedRuleset);
					}
				}
			}
		}
	}
	

	AvailabelGameRulesetCount = AvailableGameRulesets.Num();

}
	
void AUTLobbyGameState::CheckInstanceHealth()
{
	for (int32 i=0; i < AvailableMatches.Num(); i++)
	{
		AUTLobbyMatchInfo* MatchInfo = AvailableMatches[i];
		if (MatchInfo)
		{
			if (MatchInfo->GameInstanceProcessHandle.IsValid())
			{
				if ( MatchInfo->CurrentState == ELobbyMatchState::InProgress && !FPlatformProcess::IsProcRunning(MatchInfo->GameInstanceProcessHandle) )
				{
					UE_LOG(UT,Warning,TEXT("Terminating an instance that seems to be a zombie."));
					FPlatformProcess::TerminateProc(MatchInfo->GameInstanceProcessHandle);
					int32 ReturnCode = 0;
					FPlatformProcess::GetProcReturnCode(MatchInfo->GameInstanceProcessHandle, &ReturnCode);
					MatchInfo->SetLobbyMatchState(ELobbyMatchState::Recycling);
				}
			}
			else
			{
				if (MatchInfo->CurrentState == ELobbyMatchState::InProgress)
				{
					UE_LOG(UT,Warning,TEXT("Terminating an invalid match that is inprogress"));
					MatchInfo->SetLobbyMatchState(ELobbyMatchState::Recycling);
				}
			}
		}
	}

	for (int32 i=0; i<GameInstances.Num();i++)
	{
		if (GameInstances[i].MatchInfo && GameInstances[i].MatchInfo->CurrentState == ELobbyMatchState::Dead)
		{
			RemoveMatch(GameInstances[i].MatchInfo);
		}
	}

}


void AUTLobbyGameState::BroadcastChat(AUTLobbyPlayerState* SenderPS, FName Destination, const FString& Message)
{
	for (int32 i = 0; i < PlayerArray.Num(); i++)
	{
		AUTLobbyPlayerState* PS = Cast<AUTLobbyPlayerState>(PlayerArray[i]);
		if (PS)	
		{
			AUTBasePlayerController* BasePC = Cast<AUTBasePlayerController>(PS->GetOwner());
			if (BasePC != NULL)
			{
				if (Destination != ChatDestinations::Match || (SenderPS->CurrentMatch && PS->CurrentMatch == SenderPS->CurrentMatch))
				{
					BasePC->ClientSay(SenderPS, Message, Destination);
				}
			}
		}
	}
}

AUTLobbyMatchInfo* AUTLobbyGameState::FindMatchPlayerIsIn(FString PlayerID)
{
	for (int32 i=0;i<AvailableMatches.Num();i++)
	{
		if (AvailableMatches[i]->CurrentState == ELobbyMatchState::WaitingForPlayers ||
			AvailableMatches[i]->CurrentState != ELobbyMatchState::Launching ||
			AvailableMatches[i]->CurrentState != ELobbyMatchState::InProgress)	
		{
			for (int32 j=0;j<AvailableMatches[i]->Players.Num();j++)
			{
				if (AvailableMatches[i]->Players[j]->UniqueId.IsValid() && AvailableMatches[i]->Players[j]->UniqueId.ToString() == PlayerID)
				{
					return AvailableMatches[i];
				}
			}

			for (int32 j=0;j<AvailableMatches[i]->PlayersInMatchInstance.Num();j++)
			{
				if (AvailableMatches[i]->PlayersInMatchInstance[j].PlayerID.IsValid() && AvailableMatches[i]->PlayersInMatchInstance[j].PlayerID.ToString() == PlayerID)
				{
					return AvailableMatches[i];
				}
			}
		}
	}

	return NULL;
}

void AUTLobbyGameState::CheckForExistingMatch(AUTLobbyPlayerState* NewPlayer, bool bReturnedFromMatch)
{
	if (bReturnedFromMatch)
	{
		for (int32 i=0;i<AvailableMatches.Num();i++)
		{
			// Check to see if this player belongs in the match
			if (AvailableMatches[i] && AvailableMatches[i]->WasInMatchInstance(NewPlayer))
			{
				// Remove the player.
				AvailableMatches[i]->RemoveFromMatchInstance(NewPlayer);

				// Look to see if he is the owner of the match.
				if (AvailableMatches[i]->OwnerId == NewPlayer->UniqueId)
				{

					UE_LOG(UT,Log,TEXT("Found Host's Match -- Checking for kids"));

					// This was the owner, set the match to recycle (so it cleans itself up)
					AvailableMatches[i]->SetLobbyMatchState(ELobbyMatchState::Recycling);

					// Create a new match for this player to host using the old one as a template for current settings.
					AUTLobbyMatchInfo* NewMatch = AddNewMatch(NewPlayer, AvailableMatches[i]);
					NewMatch->ServerSetLobbyMatchState(ELobbyMatchState::WaitingForPlayers);

					// Look for any players who beat the host back.  If they are here, then see them

					for (int32 j = 0; j < PlayerArray.Num(); j++)
					{
						AUTLobbyPlayerState* PS = Cast<AUTLobbyPlayerState>(PlayerArray[j]);
						if (PS && PS != NewPlayer && PS->CurrentMatch == NULL && PS->PreviousMatch && PS->PreviousMatch == AvailableMatches[i])
						{
							JoinMatch(NewMatch, PS);
						}
					}
				}
				else
				{
					if (AvailableMatches[i]->CurrentState == ELobbyMatchState::Recycling)
					{
						// The host has a new match.  See if we can find it.
						for (int32 j=0; j<AvailableMatches.Num(); j++)
						{
							if (AvailableMatches[j] && AvailableMatches[i] != AvailableMatches[j] && AvailableMatches[i]->OwnerId == AvailableMatches[j]->OwnerId)					
							{
								JoinMatch(AvailableMatches[j], NewPlayer);
								break;
							}
						}
					}
					NewPlayer->PreviousMatch = AvailableMatches[i];
				}

				// We are done creating a new match
				return;
			}
		}
	}

	// We are looking to join a player's match.. see if we can find the player....
	if (NewPlayer->DesiredFriendToJoin != TEXT(""))
	{
		AUTLobbyMatchInfo* FriendsMatch = FindMatchPlayerIsIn(NewPlayer->DesiredFriendToJoin);
		if (FriendsMatch)
		{
			JoinMatch(FriendsMatch, NewPlayer);
		}
	}
}

TWeakObjectPtr<AUTReplicatedGameRuleset> AUTLobbyGameState::FindRuleset(FString TagToFind)
{
	for (int32 i=0; i < AvailableGameRulesets.Num(); i++)
	{
		if ( AvailableGameRulesets[i].IsValid() && AvailableGameRulesets[i]->UniqueTag.Equals(TagToFind, ESearchCase::IgnoreCase) )
		{
			return AvailableGameRulesets[i];
		}
	}

	return NULL;
}

AUTLobbyMatchInfo* AUTLobbyGameState::QuickStartMatch(AUTLobbyPlayerState* Host, bool bIsCTFMatch)
{
	// Create a match and replicate all of the relevant information

	UE_LOG(UT,Log,TEXT("Starting a QuickMatch for %s (%s)"), *Host->PlayerName, (bIsCTFMatch ? TEXT("True") : TEXT("False")))

	AUTLobbyMatchInfo* NewMatchInfo = GetWorld()->SpawnActor<AUTLobbyMatchInfo>();
	if (NewMatchInfo)
	{	
		AvailableMatches.Add(NewMatchInfo);
		NewMatchInfo->SetOwner(Host);
		NewMatchInfo->AddPlayer(Host, true);

		UE_LOG(UT,Log,TEXT("   Added player to Quickmatch.. "))

		TWeakObjectPtr<AUTReplicatedGameRuleset> NewRuleset = FindRuleset( bIsCTFMatch ? FQuickMatchTypeRulesetTag::CTF : FQuickMatchTypeRulesetTag::DM);

		if (NewRuleset.IsValid())
		{
			NewMatchInfo->SetRules(NewRuleset, NewRuleset->MapPlaylist);
		}

		NewMatchInfo->bJoinAnytime = true;
		NewMatchInfo->bSpectatable = true;
		NewMatchInfo->LaunchMatch();		
	}

	return NewMatchInfo;
}

AUTLobbyMatchInfo* AUTLobbyGameState::AddNewMatch(AUTLobbyPlayerState* MatchOwner, AUTLobbyMatchInfo* MatchToCopy)
{
	// Create a match and replicate all of the relevant information

	AUTLobbyMatchInfo* NewMatchInfo = GetWorld()->SpawnActor<AUTLobbyMatchInfo>();
	if (NewMatchInfo)
	{	
		AvailableMatches.Add(NewMatchInfo);
		HostMatch(NewMatchInfo, MatchOwner, MatchToCopy);
		return NewMatchInfo;
	}
		
	return NULL;
}

void AUTLobbyGameState::HostMatch(AUTLobbyMatchInfo* MatchInfo, AUTLobbyPlayerState* MatchOwner, AUTLobbyMatchInfo* MatchToCopy)
{
	MatchInfo->SetOwner(MatchOwner->GetOwner());
	MatchInfo->AddPlayer(MatchOwner, true);
	MatchInfo->SetSettings(this, MatchToCopy);
}

void AUTLobbyGameState::JoinMatch(AUTLobbyMatchInfo* MatchInfo, AUTLobbyPlayerState* NewPlayer)
{
	if (MatchInfo->CurrentState == ELobbyMatchState::Launching)
	{
		NewPlayer->ClientMatchError(NSLOCTEXT("LobbyMessage","MatchIsStarting","The match you are trying to join is starting.  Please wait for it to begin before trying to spectate it."));	
		return;
	}
	else if (MatchInfo->CurrentState == ELobbyMatchState::InProgress)
	{
		AUTLobbyGameMode* GM = GetWorld()->GetAuthGameMode<AUTLobbyGameMode>();
		if (GM)
		{
			if (MatchInfo->bJoinAnytime)
			{
				 if ( MatchInfo->MatchHasRoom() )
				 {
					MatchInfo->AddPlayer(NewPlayer);
					NewPlayer->ClientConnectToInstance(MatchInfo->GameInstanceGUID, GM->ServerInstanceGUID.ToString(), false);
					return;
				 }
				 else
				 {
					NewPlayer->ClientMatchError(NSLOCTEXT("LobbyMessage","MatchIsFull","The match you are trying to join is full."));	
					return;
				 }
			}

			if (MatchInfo->bSpectatable)
			{
				MatchInfo->AddPlayer(NewPlayer);
				NewPlayer->ClientConnectToInstance(MatchInfo->GameInstanceGUID, GM->ServerInstanceGUID.ToString(), true);
				return;
			}

			NewPlayer->ClientMatchError(NSLOCTEXT("LobbyMessage","MatchNoSpectate","This match doesn't allow spectating."));	
			return;
		}
	}

	if (!MatchInfo->SkillTest(NewPlayer->AverageRank)) // MAKE THIS CONFIG
	{
		NewPlayer->ClientMatchError(NSLOCTEXT("LobbyMessage","MatchTooGood","Your skill rating is too high for this match."));	
		return;
	}

	MatchInfo->AddPlayer(NewPlayer);
}

void AUTLobbyGameState::RemoveFromAMatch(AUTLobbyPlayerState* PlayerOwner)
{
	AUTLobbyMatchInfo* Match;
	Match = PlayerOwner->CurrentMatch;
	if (Match)
	{
		// Trigger all players being removed
		if (Match->RemovePlayer(PlayerOwner))
		{
			RemoveMatch(Match);
		}
	}
}

void AUTLobbyGameState::RemoveMatch(AUTLobbyMatchInfo* MatchToRemove)
{
	// Kill any game instances.
	TerminateGameInstance(MatchToRemove);

	// Match is dead....
	AvailableMatches.Remove(MatchToRemove);
}

void AUTLobbyGameState::SortPRIArray()
{
}

void AUTLobbyGameState::SetupLobbyBeacons()
{
	UWorld* const World = GetWorld();

	// Always create a new beacon host
	LobbyBeacon_Listener = World->SpawnActor<AUTServerBeaconLobbyHostListener>(AUTServerBeaconLobbyHostListener::StaticClass());

	if (LobbyBeacon_Listener && LobbyBeacon_Listener->InitHost())
	{
		LobbyBeacon_Object = World->SpawnActor<AUTServerBeaconLobbyHostObject>(AUTServerBeaconLobbyHostObject::StaticClass());

		if (LobbyBeacon_Object)
		{
			LobbyBeacon_Listener->RegisterHost(LobbyBeacon_Object);
			GameInstanceListenPort = LobbyBeacon_Listener->ListenPort;
			return;
		}
	}

	UE_LOG(UT,Log,TEXT("Could not create Lobby Beacons"));
}

void AUTLobbyGameState::CreateAutoMatch(FString MatchGameMode, FString MatchOptions, FString MatchMap)
{
/*
	// Create the MatchInfo for this match
	AUTLobbyMatchInfo* NewMatchInfo = GetWorld()->SpawnActor<AUTLobbyMatchInfo>();
	if (NewMatchInfo)
	{	
		AvailableMatches.Add(NewMatchInfo);
		NewMatchInfo->MatchGameMode = MatchGameMode;
		NewMatchInfo->MatchOptions = MatchOptions;
		NewMatchInfo->MatchMap = MatchMap;
		NewMatchInfo->bJoinAnytime = true;
		NewMatchInfo->bSpectatable = true;
		NewMatchInfo->MaxPlayers = 20;
		NewMatchInfo->bDedicatedMatch = true;

		MatchOptions = MatchOptions + TEXT("?DedI=TRUE");

		LaunchGameInstance(NewMatchInfo, MatchOptions, 10, -1);
	}
*/
}

void AUTLobbyGameState::LaunchGameInstance(AUTLobbyMatchInfo* MatchOwner, const FString& GameMode, const FString& Map, const FString& GameOptions, int32 MaxPlayers, int32 BotSkillLevel)
{
	AUTLobbyGameMode* LobbyGame = GetWorld()->GetAuthGameMode<AUTLobbyGameMode>();
	if (LobbyGame)
	{
		GameInstanceID++;
		if (GameInstanceID == 0) GameInstanceID = 1;	// Always skip 0.

		// Append the InstanceID so that we know who to talk about to.  TODO: We have to add the server's accessible address (or the iC's address) here once we support
		// instances on a machines.  NOTE: All communication that comes from the instance server to the lobby will need this id.

		FString FinalOptions = GameOptions;
		
		// Set the Max players
		FinalOptions += FString::Printf(TEXT("?MaxPlayers=%i"), MaxPlayers);

		// Set the Bot Skill level and if they are needed.
		if (BotSkillLevel >= 0)
		{
			FinalOptions += FString::Printf(TEXT("?BotFill=%i?Difficulty=%i"), MaxPlayers, FMath::Clamp<int32>(BotSkillLevel,0,7));
		}

		// Apply additional options.
		if (!ForcedInstanceGameOptions.IsEmpty()) FinalOptions += ForcedInstanceGameOptions;

		FinalOptions = FString::Printf(TEXT("%s?Game=%s?%s?InstanceID=%i?HostPort=%i"), *Map, *GameMode, *FinalOptions, GameInstanceID, GameInstanceListenPort);

		int32 InstancePort = LobbyGame->StartingInstancePort + (LobbyGame->InstancePortStep * GameInstances.Num());

		FString ExecPath = FPlatformProcess::GenerateApplicationPath(FApp::GetName(), FApp::GetBuildConfiguration());
		FString Options = FString::Printf(TEXT("UnrealTournament %s -server -port=%i -log"), *FinalOptions, InstancePort);
		
		// Add in additional command line params
		if (!AdditionalInstanceCommandLine.IsEmpty()) Options += TEXT(" ") + AdditionalInstanceCommandLine;

		FString ConnectionString = FString::Printf(TEXT("%s:%i"), *GetWorld()->GetNetDriver()->LowLevelGetNetworkNumber(), InstancePort);

		UE_LOG(UT,Verbose,TEXT("Launching %s with Params %s"), *ExecPath, *Options);

		MatchOwner->GameInstanceProcessHandle = FPlatformProcess::CreateProc(*ExecPath, *Options, true, false, false, NULL, 0, NULL, NULL);

		if (MatchOwner->GameInstanceProcessHandle.IsValid())
		{
			GameInstances.Add(FGameInstanceData(MatchOwner, InstancePort, ConnectionString));
			MatchOwner->SetLobbyMatchState(ELobbyMatchState::Launching);
			MatchOwner->GameInstanceID = GameInstanceID;
		}
		else
		{
			UE_LOG(UT,Warning,TEXT("Could not start an instance (the ProcHandle is Invalid)"));
			if (MatchOwner)
			{
				TWeakObjectPtr<AUTLobbyPlayerState> OwnerPS = MatchOwner->GetOwnerPlayerState();
				if (OwnerPS.IsValid())
				{
					OwnerPS->ClientMatchError(NSLOCTEXT("LobbyMessage", "FailedToStart", "Unfortunately, the server had trouble starting a new instance.  Please try again in a few moments."));
				}
			}
		}
	}

	AUTLobbyGameMode* GM = GetWorld()->GetAuthGameMode<AUTLobbyGameMode>();
	if (GM)
	{
		GM->UpdateLobbySession();
	}
}

void AUTLobbyGameState::TerminateGameInstance(AUTLobbyMatchInfo* MatchOwner)
{
	if (MatchOwner->GameInstanceProcessHandle.IsValid())
	{
		// if we have an active game instance that is coming up but we have not started the travel to it yet, Kill the instance.
		if (FPlatformProcess::IsProcRunning(MatchOwner->GameInstanceProcessHandle))
		{
			FPlatformProcess::TerminateProc(MatchOwner->GameInstanceProcessHandle);
		}
		int32 ReturnCode = 0;
		FPlatformProcess::GetProcReturnCode(MatchOwner->GameInstanceProcessHandle, &ReturnCode);
		UE_LOG(UT, Verbose, TEXT("Terminating an Instance with return code %i"), ReturnCode);

		MatchOwner->GameInstanceProcessHandle.Reset();
		MatchOwner->GameInstanceID = 0;
	}

	int32 InstanceIndex = -1;
	for (int32 i = 0; i < GameInstances.Num(); i++)
	{
		if (GameInstances[i].MatchInfo == MatchOwner)
		{
			GameInstances.RemoveAt(i);
			break;
		}
	}

	AUTLobbyGameMode* GM = GetWorld()->GetAuthGameMode<AUTLobbyGameMode>();
	if (GM)
	{
		GM->UpdateLobbySession();
	}
}


void AUTLobbyGameState::GameInstance_Ready(uint32 GameInstanceID, FGuid GameInstanceGUID)
{
	for (int32 i=0; i < GameInstances.Num(); i++)
	{
		if (GameInstances[i].MatchInfo->GameInstanceID == GameInstanceID)
		{
			GameInstances[i].MatchInfo->GameInstanceReady(GameInstanceGUID);
			break;
		}
	}
}

void AUTLobbyGameState::GameInstance_MatchUpdate(uint32 GameInstanceID, const FString& Update)
{
	for (int32 i = 0; i < GameInstances.Num(); i++)
	{
		if (GameInstances[i].MatchInfo->GameInstanceID == GameInstanceID)
		{
			GameInstances[i].MatchInfo->SetMatchStats(Update);
			break;
		}
	}
}

void AUTLobbyGameState::GameInstance_MatchBadgeUpdate(uint32 GameInstanceID, const FString& Update)
{
	for (int32 i = 0; i < GameInstances.Num(); i++)
	{
		if (GameInstances[i].MatchInfo->GameInstanceID == GameInstanceID)
		{
			GameInstances[i].MatchInfo->MatchBadge = Update;
			break;
		}
	}
}


void AUTLobbyGameState::GameInstance_PlayerUpdate(uint32 GameInstanceID, FUniqueNetIdRepl PlayerID, const FString& PlayerName, int32 PlayerScore)
{
	// Find the match
	for (int32 i = 0; i < GameInstances.Num(); i++)
	{
		if (GameInstances[i].MatchInfo->GameInstanceID == GameInstanceID)
		{
			// Look through the players in the match
			AUTLobbyMatchInfo* Match = GameInstances[i].MatchInfo;
			if (Match)
			{
				for (int32 j=0; j < Match->PlayersInMatchInstance.Num(); j++)
				{
					if (Match->PlayersInMatchInstance[j].PlayerID == PlayerID)
					{
						Match->PlayersInMatchInstance[j].PlayerName = PlayerName;
						Match->PlayersInMatchInstance[j].PlayerScore = PlayerScore;
						break;
					}
				}
			}
		}
	}
}


void AUTLobbyGameState::GameInstance_EndGame(uint32 GameInstanceID, const FString& FinalUpdate)
{
	AUTLobbyGameMode* GM = GetWorld()->GetAuthGameMode<AUTLobbyGameMode>();
	if (GM)
	{
		GM->UpdateLobbySession();

		for (int32 i = 0; i < GameInstances.Num(); i++)
		{
			if (GameInstances[i].MatchInfo->GameInstanceID == GameInstanceID)
			{
				GameInstances[i].MatchInfo->MatchStats= FinalUpdate;
				break;
			}
		}
	}
}

void AUTLobbyGameState::GameInstance_Empty(uint32 GameInstanceID)
{
	for (int32 i = 0; i < GameInstances.Num(); i++)
	{
		if (GameInstances[i].MatchInfo->GameInstanceID == GameInstanceID && !GameInstances[i].MatchInfo->bDedicatedMatch)
		{
			// Set the match info's state to recycling so all returning players will be directed properly.
			GameInstances[i].MatchInfo->SetLobbyMatchState(ELobbyMatchState::Recycling);

			// Terminate the game instance
			TerminateGameInstance(GameInstances[i].MatchInfo);
			break;
		}
	}
}

bool AUTLobbyGameState::IsMatchStillValid(AUTLobbyMatchInfo* TestMatch)
{
	for (int32 i=0;i<AvailableMatches.Num();i++)
	{
		if (AvailableMatches[i] && AvailableMatches[i] == TestMatch) return true;
	}

	return false;
}

// A New Client has joined.. Send them all of the server side settings
void AUTLobbyGameState::InitializeNewPlayer(AUTLobbyPlayerState* NewPlayer)
{
	CheckForExistingMatch(NewPlayer, NewPlayer->bReturnedFromMatch);
}


bool AUTLobbyGameState::CanLaunch(AUTLobbyMatchInfo* MatchToLaunch)
{
	AUTLobbyGameMode* GM = GetWorld()->GetAuthGameMode<AUTLobbyGameMode>();
	return (GM && GM->GetNumMatches() < GM->MaxInstances);
}

void AUTLobbyGameState::GameInstance_RequestNextMap(AUTServerBeaconLobbyClient* ClientBeacon, uint32 GameInstanceID, const FString& CurrentMap)
{

	for (int32 i = 0; i < GameInstances.Num(); i++)
	{
		if (GameInstances[i].MatchInfo->GameInstanceID == GameInstanceID)
		{
			for (int32 j=0; j < GameInstances[i].MatchInfo->MapList.Num(); j++)
			{
				if (GameInstances[i].MatchInfo->MapList[j].ToLower() == CurrentMap.ToLower())
				{
					if (j < GameInstances[i].MatchInfo->MapList.Num()-1)
					{
						ClientBeacon->InstanceNextMap(GameInstances[i].MatchInfo->MapList[j+1]);
						return;
					}

					break;
				}
			}

			GameInstances[i].MatchInfo->SetLobbyMatchState(ELobbyMatchState::Completed);
			ClientBeacon->InstanceNextMap(TEXT(""));
		}
	}
}

