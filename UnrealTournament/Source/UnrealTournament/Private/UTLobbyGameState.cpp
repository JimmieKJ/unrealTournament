// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/GameState.h"
#include "UTGameState.h"
#include "UTLobbyMatchInfo.h"
#include "UTLobbyGameMode.h"
#include "Net/UnrealNetwork.h"

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
}

void AUTLobbyGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (Role == ROLE_Authority)
	{
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTLobbyGameState::CheckInstanceHealth, 60.0f, true);	
	}
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
					FPlatformProcess::CloseProc(MatchInfo->GameInstanceProcessHandle);
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

void AUTLobbyGameState::CheckForExistingMatch(AUTLobbyPlayerState* NewPlayer)
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
				// This was the owner, set the match to recycle (so it cleans itself up)
				AvailableMatches[i]->SetLobbyMatchState(ELobbyMatchState::Recycling);

				// Create a new match for this player to host using the old one as a template for current settings.
				AUTLobbyMatchInfo* NewMatch = AddNewMatch(NewPlayer, AvailableMatches[i]);

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
				else if (AvailableMatches[i]->IsInProgress())
				{
					NewPlayer->PreviousMatch = AvailableMatches[i];
				}
			}
			break;
		}
	}
}

AUTLobbyMatchInfo* AUTLobbyGameState::QuickStartMatch(AUTLobbyPlayerState* Host, bool bIsCTFMatch)
{
	// Create a match and replicate all of the relevant information

	AUTLobbyMatchInfo* NewMatchInfo = GetWorld()->SpawnActor<AUTLobbyMatchInfo>();
	if (NewMatchInfo)
	{	
		AvailableMatches.Add(NewMatchInfo);
		NewMatchInfo->SetOwner(Host);
		NewMatchInfo->AddPlayer(Host, true);

		if (bIsCTFMatch)
		{
			NewMatchInfo->MatchGameMode = TEXT("/Script/UnrealTournament.UTCTFGameMode");
			NewMatchInfo->MatchMap = TEXT("CTF-Outside");
			NewMatchInfo->MatchOptions = "?MinPlayers=6";
		}
		else
		{
			NewMatchInfo->MatchGameMode = TEXT("/Script/UnrealTournament.UTDMGameMode");
			NewMatchInfo->MatchMap = TEXT("DM-Tuba");		// MOVE THIS TO CIRCUIT 
			NewMatchInfo->MatchOptions = "?MinPlayers=4";
		}

		NewMatchInfo->bJoinAnytime = true;
		NewMatchInfo->bSpectatable = true;
		NewMatchInfo->MaxPlayers = 10;

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
				 if (MatchInfo->PlayersInMatchInstance.Num() < MatchInfo->MaxPlayers)
				 {
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

		LaunchGameInstance(NewMatchInfo, MatchOptions);
	}
}

void AUTLobbyGameState::LaunchGameInstance(AUTLobbyMatchInfo* MatchOwner, FString ServerURLOptions)
{
	AUTLobbyGameMode* LobbyGame = GetWorld()->GetAuthGameMode<AUTLobbyGameMode>();
	if (LobbyGame)
	{
		GameInstanceID++;
		if (GameInstanceID == 0) GameInstanceID = 1;	// Always skip 0.

		// Append the InstanceID so that we know who to talk about to.  TODO: We have to add the server's accessible address (or the iC's address) here once we support
		// instances on a machines.  NOTE: All communication that comes from the instance server to the lobby will need this id.

		// Apply additional options.
		if (!ForcedInstanceGameOptions.IsEmpty()) ServerURLOptions += ForcedInstanceGameOptions;

		FString GameOptions = FString::Printf(TEXT("%s?Game=%s?%s?InstanceID=%i?HostPort=%i"), *MatchOwner->MatchMap, *MatchOwner->MatchGameMode, *ServerURLOptions, GameInstanceID, GameInstanceListenPort);

		int32 InstancePort = LobbyGame->StartingInstancePort + (LobbyGame->InstancePortStep * GameInstances.Num());

		FString ExecPath = FPlatformProcess::GenerateApplicationPath(FApp::GetName(), FApp::GetBuildConfiguration());
		FString Options = FString::Printf(TEXT("UnrealTournament %s -server -port=%i -log"), *GameOptions, InstancePort);
		
		// Add in additional command line params
		if (!AdditionalInstanceCommandLine.IsEmpty()) Options += TEXT(" ") + AdditionalInstanceCommandLine;

		FString ConnectionString = FString::Printf(TEXT("%s:%i"), *GetWorld()->GetNetDriver()->LowLevelGetNetworkNumber(), InstancePort);

		UE_LOG(UT,Log,TEXT("Launching %s with Params %s"), *ExecPath, *Options);

		MatchOwner->GameInstanceProcessHandle = FPlatformProcess::CreateProc(*ExecPath, *Options, true, false, false, NULL, 0, NULL, NULL);

		if (MatchOwner->GameInstanceProcessHandle.IsValid())
		{
			GameInstances.Add(FGameInstanceData(MatchOwner, InstancePort, ConnectionString));
			MatchOwner->SetLobbyMatchState(ELobbyMatchState::Launching);
			MatchOwner->GameInstanceID = GameInstanceID;
		}
		else
		{
			// NOTIFY the Match Owner that the server failed to start an instance
		}
	}
}

void AUTLobbyGameState::TerminateGameInstance(AUTLobbyMatchInfo* MatchOwner)
{
	if (MatchOwner->GameInstanceProcessHandle.IsValid())
	{

		// if we have an active game instance that is coming up but we have not started the travel to it yet, Kill the instance.
		if (MatchOwner->GameInstanceProcessHandle.IsValid())
		{
			if (FPlatformProcess::IsProcRunning(MatchOwner->GameInstanceProcessHandle))
			{
				FPlatformProcess::TerminateProc(MatchOwner->GameInstanceProcessHandle);
			}
		}

		int32 InstanceIndex = -1;
		for (int32 i=0; i < GameInstances.Num(); i++)
		{
			if (GameInstances[i].MatchInfo == MatchOwner)
			{
				GameInstances.RemoveAt(i);
				break;
			}
		}

		MatchOwner->GameInstanceProcessHandle.Reset();
		MatchOwner->GameInstanceID = 0;
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
			GameInstances[i].MatchInfo->MatchStats = Update;
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
	for (int32 i = 0; i < GameInstances.Num(); i++)
	{
		if (GameInstances[i].MatchInfo->GameInstanceID == GameInstanceID)
		{
			GameInstances[i].MatchInfo->MatchStats= FinalUpdate;
			GameInstances[i].MatchInfo->SetLobbyMatchState(ELobbyMatchState::Completed);
			break;
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
	for (int32 i = 0; i < AllowedGameModeClasses.Num(); i++)
	{
		FString Option = FString::Printf(TEXT("game=%s"), *AllowedGameModeClasses[i]);
		NewPlayer->AddHostData(Option);
	}

	// determine GUID for installed maps if we haven't already
	if (AllowedMaps.Num() == 0)
	{
		// if server .ini didn't define allowed maps, assume all maps are acceptable
		if (AllowedMapNames.Num() == 0)
		{
			TArray<FString> Paths;
			FPackageName::QueryRootContentPaths(Paths);
			int32 MapExtLen = FPackageName::GetMapPackageExtension().Len();
			for (int32 i = 0; i < Paths.Num(); i++)
			{
				// ignore /Engine/ as those aren't real gameplay maps
				if (!Paths[i].StartsWith(TEXT("/Engine/")))
				{
					TArray<FString> List;
					FPackageName::FindPackagesInDirectory(List, *FPackageName::LongPackageNameToFilename(Paths[i]));
					for (int32 j = 0; j < List.Num(); j++)
					{
						if (List[j].Right(MapExtLen) == FPackageName::GetMapPackageExtension())
						{
							AllowedMapNames.Add(FPaths::GetBaseFilename(List[j]));
						}
					}
				}
			}
		}
		for (const FString& MapName : AllowedMapNames)
		{
			FString MapFullName;
			FPackageName::SearchForPackageOnDisk(FString(MapName) + FPackageName::GetMapPackageExtension(), &MapFullName);
			new(AllowedMaps) FAllowedMapData(MapName, GEngine->GetPackageGuid(FName(*MapFullName)));
		}
	}
	for (int32 i = 0; i < AllowedMaps.Num(); i++)
	{
		FString Option = FString::Printf(TEXT("map=%s:%s"), *AllowedMaps[i].MapName, *AllowedMaps[i].MapGuid.ToString());
		NewPlayer->AddHostData(Option);
	}


	NewPlayer->StartServerToClientDataPush();
}

AUTGameMode* AUTLobbyGameState::GetGameModeDefaultObject(FString ClassName)
{
	// Try to load the native class
	UClass* GameModeClass = NULL;
	if (!ClassName.IsEmpty()) // not replicated yet?
	{
		GameModeClass = LoadClass<AUTGameMode>(NULL, *ClassName, NULL, LOAD_None, NULL);
		if (GameModeClass == NULL)
		{
			FString BlueprintName = FString::Printf(TEXT("%s_C"), *ClassName);
			GameModeClass = LoadClass<UUTHUDWidget>(NULL, *BlueprintName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
		}
	}

	return (GameModeClass != NULL) ? GameModeClass->GetDefaultObject<AUTGameMode>() : NULL;
}

TSharedPtr<FAllowedGameModeData> AUTLobbyGameState::ResolveGameMode(FString GameModeClass)
{
	for (int32 i=0; i<ClientAvailbleGameModes.Num();i++)
	{
		if (ClientAvailbleGameModes[i]->ClassName == GameModeClass)
		{
			return ClientAvailbleGameModes[i];
		}
	}

	// try to fall back to DM
	for (int32 i = 0; i<ClientAvailbleGameModes.Num(); i++)
	{
		if (ClientAvailbleGameModes[i]->ClassName == TEXT("/Script/UnrealTournament.UTDMGameMode"))
		{
			return ClientAvailbleGameModes[i];
		}
	}

	FString GameMode = TEXT("/Script/UnrealTournament.UTDMGameMode");
	AUTGameMode* DefaultGame = AUTLobbyGameState::GetGameModeDefaultObject(GameMode);
	if (DefaultGame)
	{
		return FAllowedGameModeData::Make(GameMode, DefaultGame->DisplayName.ToString(), DefaultGame);	
	}

	return NULL;
}

bool AUTLobbyGameState::CanLaunch(AUTLobbyMatchInfo* MatchToLaunch)
{
	AUTLobbyGameMode* GM = GetWorld()->GetAuthGameMode<AUTLobbyGameMode>();
	return (GM && GameInstances.Num() < GM->MaxInstances);
}

