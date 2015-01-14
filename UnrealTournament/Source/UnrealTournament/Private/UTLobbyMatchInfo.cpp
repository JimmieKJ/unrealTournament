// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/PlayerState.h"
#include "UTLobbyGameMode.h"
#include "UTLobbyMatchInfo.h"
#include "Net/UnrealNetwork.h"

namespace ELobbyMatchState
{
	const FName Dead				= TEXT("Dead");
	const FName Initializing		= TEXT("Initializing");
	const FName Setup				= TEXT("Setup");
	const FName WaitingForPlayers	= TEXT("WaitingForPlayers");
	const FName Launching			= TEXT("Launching");
	const FName Aborting			= TEXT("Aborting");
	const FName InProgress			= TEXT("InProgress");
	const FName Completed			= TEXT("Completed");
	const FName Recycling			= TEXT("Recycling");
	const FName Returning			= TEXT("Returning");
}


AUTLobbyMatchInfo::AUTLobbyMatchInfo(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
	.DoNotCreateDefaultSubobject(TEXT("Sprite")))
{
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	bReplicateMovement = false;

	// Note: this is very important to set to false. Though all replication infos are spawned at run time, during seamless travel
	// they are held on to and brought over into the new world. In ULevel::InitializeNetworkActors, these PlayerStates may be treated as map/startup actors
	// and given static NetGUIDs. This also causes their deletions to be recorded and sent to new clients, which if unlucky due to name conflicts,
	// may end up deleting the new PlayerStates they had just spaned.
	bNetLoadOnClient = false;
}



void AUTLobbyMatchInfo::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AUTLobbyMatchInfo, OwnerId);
	DOREPLIFETIME(AUTLobbyMatchInfo, CurrentState);
	DOREPLIFETIME(AUTLobbyMatchInfo, bPrivateMatch);
	DOREPLIFETIME(AUTLobbyMatchInfo, MatchStats);
	DOREPLIFETIME(AUTLobbyMatchInfo, MatchGameMode);
	DOREPLIFETIME(AUTLobbyMatchInfo, MatchMap);
	DOREPLIFETIME(AUTLobbyMatchInfo, MatchOptions);
	DOREPLIFETIME(AUTLobbyMatchInfo, MaxPlayers);
	DOREPLIFETIME(AUTLobbyMatchInfo, Players);
	DOREPLIFETIME(AUTLobbyMatchInfo, PlayersInMatchInstance);
}

void AUTLobbyMatchInfo::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	SetLobbyMatchState(ELobbyMatchState::Initializing);
	LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	MinPlayers = 2; // TODO: This should be pulled from the game type at some point
}

void AUTLobbyMatchInfo::OnRep_MatchOptions()
{
	OnMatchOptionsChanged.ExecuteIfBound();	
}

void AUTLobbyMatchInfo::OnRep_MatchGameMode()
{
	OnMatchGameModeChanged.ExecuteIfBound();	
}


void AUTLobbyMatchInfo::OnRep_MatchMap()
{
	OnMatchMapChanged.ExecuteIfBound();	
}

void AUTLobbyMatchInfo::SetLobbyMatchState(FName NewMatchState)
{
	if ((CurrentState != ELobbyMatchState::Recycling || NewMatchState == ELobbyMatchState::Dead) && CurrentState != ELobbyMatchState::Dead)
	{
		CurrentState = NewMatchState;
		if (CurrentState == ELobbyMatchState::Recycling)
		{
			GetWorldTimerManager().SetTimer(this, &AUTLobbyMatchInfo::RecycleMatchInfo, 120.0, false);
		}
	}
}

void AUTLobbyMatchInfo::RecycleMatchInfo()
{
	if (LobbyGameState)
	{
		LobbyGameState->RemoveMatch(this);
	}
}

void AUTLobbyMatchInfo::AddPlayer(AUTLobbyPlayerState* PlayerToAdd, bool bIsOwner)
{
	if (bIsOwner)
	{
		OwnerId = PlayerToAdd->UniqueId;
		SetLobbyMatchState(ELobbyMatchState::Setup);
	}
	else
	{
		if (Players.Num() >= 2)	// TODO: Make this a feature of the match itself
		{
			PlayerToAdd->ClientMatchError(NSLOCTEXT("LobbyMessage","MatchIsFull","The match you are trying to join is full."));
			return;
		}

		for (int32 i=0;i<BannedIDs.Num();i++)
		{
			if (PlayerToAdd->UniqueId == BannedIDs[i])
			{
				PlayerToAdd->ClientMatchError(NSLOCTEXT("LobbyMessage","Banned","You do not have permission to enter this match."));
				return;
			}
		}
		// Add code for private/invite only matches
	}
	
	Players.Add(PlayerToAdd);
	PlayerToAdd->AddedToMatch(this);

	PlayerToAdd->ChatDestination = ChatDestinations::Match;

}

bool AUTLobbyMatchInfo::RemovePlayer(AUTLobbyPlayerState* PlayerToRemove)
{
	// Owners remove everyone and kill the match
	if (OwnerId == PlayerToRemove->UniqueId)
	{
		// The host is removing this match, notify everyone.
		for (int32 i=0;i<Players.Num();i++)
		{
			Players[i]->RemovedFromMatch(this);
		}
		Players.Empty();

		// We are are not launching, kill this lobby otherwise keep it around
		if (!IsInProgress())
		{
			SetLobbyMatchState(ELobbyMatchState::Dead);
			return true;
		}

		return false;
	}

	// Else just remove this one player
	else
	{
		Players.Remove(PlayerToRemove);
		PlayerToRemove->RemovedFromMatch(this);
	}

	return false;

}

FText AUTLobbyMatchInfo::GetActionText()
{
	if (CurrentState == ELobbyMatchState::Dead)
	{
		return NSLOCTEXT("SUMatchPanel","Dead","!! DEAD - BUG !!");
	}
	else if (CurrentState == ELobbyMatchState::Setup)
	{
		return NSLOCTEXT("SUMatchPanel","Setup","Initializing...");
	}
	else if (CurrentState == ELobbyMatchState::WaitingForPlayers)
	{
		return NSLOCTEXT("SUMatchPanel","WaitingForPlayers","Click to Join");
	}
	else if (CurrentState == ELobbyMatchState::Launching)
	{
		return NSLOCTEXT("SUMatchPanel","Launching","Launching...");
	}
	else if (CurrentState == ELobbyMatchState::InProgress)
	{
		int32 Seconds;
		if ( FParse::Value(*MatchStats,TEXT("ElpasedTime="),Seconds) )
		{
			int32 Hours = Seconds / 3600;
			Seconds -= Hours * 3600;
			int32 Mins = Seconds / 60;
			Seconds -= Mins * 60;

			FFormatNamedArguments Args;
			FNumberFormattingOptions Options;

			Options.MinimumIntegralDigits = 2;
			Options.MaximumIntegralDigits = 2;

			Args.Add(TEXT("Hours"), FText::AsNumber(Hours, &Options));
			Args.Add(TEXT("Minutes"), FText::AsNumber(Mins, &Options));
			Args.Add(TEXT("Seconds"), FText::AsNumber(Seconds, &Options));

			return FText::Format( NSLOCTEXT("SUWindowsMidGame","ClockFormat", "{Hours}:{Minutes}:{Seconds}"),Args);
		}
		return NSLOCTEXT("SUMatchPanel","InProgress","In Progress...");
	}
	else if (CurrentState == ELobbyMatchState::Returning)
	{
		return NSLOCTEXT("SUMatchPanel","MatchOver","!! Match is over !!");
	}

	return FText::GetEmpty();
}

void AUTLobbyMatchInfo::SetSettings(AUTLobbyGameState* GameState, AUTLobbyMatchInfo* MatchToCopy)
{
	CurrentState = ELobbyMatchState::Setup;

	if (MatchToCopy)
	{
		// Copy the options that were already built
		for (int32 i=0;i<MatchToCopy->HostMatchData.Num();i++)
		{
			HostMatchData.Add(MatchToCopy->HostMatchData[i]);
		}

		// Set this for later.  When the client is done replicating all of the match data, it will call
		// ServerSetDefaults.  Since this is a returning match we can assume this info is already valid on the 
		// client-host so we will be able to copy the default settings.
		PriorMatchToCopy = MatchToCopy;
	}
	else
	{
		for (int32 i=0;i<GameState->AllowedGameModeClasses.Num();i++)
		{
			FString Option = FString::Printf(TEXT("game=%s"), *GameState->AllowedGameModeClasses[i]);
			HostMatchData.Add(Option);
		}

		for (int32 i=0;i<GameState->AllowedMaps.Num();i++)
		{
			FString Option = FString::Printf(TEXT("map=%s"), *GameState->AllowedMaps[i]);
			HostMatchData.Add(Option);
		}
	}

	StartServerToClientDataPush();
}

void AUTLobbyMatchInfo::StartServerToClientDataPush()
{
	CurrentBulkID = 1;
	DataIndex = 0;
	SendNextBulkBlock();
}

void AUTLobbyMatchInfo::SendNextBulkBlock()
{
	uint8 SendCount = FMath::Clamp<int>( HostMatchData.Num() - DataIndex, 0, 10);

	if (SendCount > 0)
	{
		for (int i = 0; i < SendCount; i++)	// Send 10 maps at time.. make this configurable.
		{
			ClientReceiveMatchData(SendCount, CurrentBulkID, HostMatchData[DataIndex++]);
		}
	}
	else
	{
		ClientReceivedAllData();
	}

}

void AUTLobbyMatchInfo::ClientReceivedAllData_Implementation()
{
	// TODO: Add code that detects the host has no usuable gamemodes or maps installed and both warns the host and
	// tells the server to clean up this broken match.

	// Set the hosts defaults.

	MatchGameMode = HostAvailbleGameModes[0]->DisplayName;
	MatchOptions = HostAvailbleGameModes[0]->DefaultObject->GetDefaultLobbyOptions();
	MatchMap = HostAvailableMaps[0]->MapName;

	// Ok, we have all of the default data set now.  Tell the server.

	if (OwnerId.IsValid())
	{
		ServerSetDefaults(MatchGameMode, MatchOptions, MatchMap);
	}
	else
	{
		bWaitingForOwnerId = true;
	}
}

void AUTLobbyMatchInfo::OnRep_OwnerId()
{
	if (bWaitingForOwnerId && OwnerId.IsValid())
	{
		ServerSetDefaults(MatchGameMode, MatchOptions, MatchMap);
	}
}

AUTGameMode* AUTLobbyMatchInfo::GetGameModeDefaultObject(FString ClassName)
{
	// Try to load the native class

	UClass* GameModeClass = LoadClass<AUTGameMode>(NULL, *ClassName, NULL, LOAD_None, NULL);

	if (GameModeClass == NULL)
	{
		// Try the blueprinted class
		UBlueprint* Blueprint = LoadObject<UBlueprint>(NULL, *ClassName, NULL, LOAD_None, NULL);
		if (Blueprint != NULL)
		{
			GameModeClass = Blueprint->GeneratedClass;
		}
	}

	if (GameModeClass != NULL)
	{
		return GameModeClass->GetDefaultObject<AUTGameMode>();
	}

	return NULL;
}

void AUTLobbyMatchInfo::ClientReceiveMatchData_Implementation(uint8 BulkSendCount, uint16 BulkSendID, const FString& MatchData)
{
	// Look to see if this is a new block.
	if (BulkSendID != CurrentBulkID)
	{
		if (CurrentBlockCount != ExpectedBlockCount)
		{
			UE_LOG(UT,Log,TEXT("ERROR: Didn't receive everything in the block %i %i %i"), BulkSendID, CurrentBlockCount, ExpectedBlockCount);
		}

		CurrentBulkID = BulkSendID;
		CurrentBlockCount = 0;
		ExpectedBlockCount = BulkSendCount;
	}

	// Parse the Match data.

	FString DataType;
	FString Data;
	if ( MatchData.Split(TEXT("="), &DataType, &Data) )
	{
		if (DataType.ToLower() == TEXT("game"))	
		{
			AUTGameMode* DefaultGame = GetGameModeDefaultObject(Data);
			if (DefaultGame)
			{
				HostAvailbleGameModes.Add( FAllowedGameModeData::Make(Data, DefaultGame->DisplayName.ToString(), DefaultGame));
			}
		}
		else if (DataType.ToLower() == TEXT("map"))
		{
			// Add code to check if the map exists..
			HostAvailableMaps.Add( FAllowedMapData::Make(Data));
		}
	}

	CurrentBlockCount++;
	if (CurrentBlockCount == ExpectedBlockCount)
	{
		ServerACKBulkCompletion(CurrentBulkID);
	}
}

bool AUTLobbyMatchInfo::ServerACKBulkCompletion_Validate(uint16 BuildSendID) { return true;}
void AUTLobbyMatchInfo::ServerACKBulkCompletion_Implementation(uint16 BuildSendID) 
{
	// Pause a slight bit before the next block is sent
	GetWorldTimerManager().SetTimer(this, &AUTLobbyMatchInfo::SendNextBulkBlock, 0.1);
}

void AUTLobbyMatchInfo::SetMatchGameMode(const FString NewGameMode)
{
	ServerMatchGameModeChanged(NewGameMode);
}


bool AUTLobbyMatchInfo::ServerMatchGameModeChanged_Validate(const FString& NewMatchGameMode) { return true; }
void AUTLobbyMatchInfo::ServerMatchGameModeChanged_Implementation(const FString& NewMatchGameMode)
{
	MatchGameMode = NewMatchGameMode;
}

void AUTLobbyMatchInfo::SetMatchMap(const FString NewMatchMap)
{
	ServerMatchMapChanged(NewMatchMap);
}

bool AUTLobbyMatchInfo::ServerMatchMapChanged_Validate(const FString& NewMatchMap) { return true; }
void AUTLobbyMatchInfo::ServerMatchMapChanged_Implementation(const FString& NewMatchMap)
{
	MatchMap = NewMatchMap;
}

void AUTLobbyMatchInfo::SetMatchOptions(const FString NewMatchOptions)
{
	ServerMatchOptionsChanged(NewMatchOptions);
}


bool AUTLobbyMatchInfo::ServerMatchOptionsChanged_Validate(const FString& NewMatchOptions) { return true; }
void AUTLobbyMatchInfo::ServerMatchOptionsChanged_Implementation(const FString& NewMatchOptions)
{
	MatchOptions = NewMatchOptions;
}

bool AUTLobbyMatchInfo::ServerSetDefaults_Validate(const FString& NewMatchGameMode, const FString& NewMatchOptions, const FString& NewMatchMap) { return true; }
void AUTLobbyMatchInfo::ServerSetDefaults_Implementation(const FString& NewMatchGameMode, const FString& NewMatchOptions, const FString& NewMatchMap)
{

	// If we are the settings from a prior match then copy them instead of using the client's defaults.
	if (PriorMatchToCopy)
	{
		MatchGameMode = PriorMatchToCopy->MatchGameMode;
		MatchOptions = PriorMatchToCopy->MatchOptions;
		MatchMap = PriorMatchToCopy->MatchMap;
		PriorMatchToCopy = NULL;
	}
	else
	{
		MatchGameMode = NewMatchGameMode;
		MatchOptions = NewMatchOptions;
		MatchMap = NewMatchMap;
	}

	SetLobbyMatchState(ELobbyMatchState::WaitingForPlayers);
}

bool AUTLobbyMatchInfo::ServerManageUser_Validate(int32 CommandID, AUTLobbyPlayerState* Target){ return true; }
void AUTLobbyMatchInfo::ServerManageUser_Implementation(int32 CommandID, AUTLobbyPlayerState* Target)
{

	for (int32 i=0; i < Players.Num(); i++)
	{
		if (Target == Players[i])
		{
			// Right now we only have kicks and bans.
			RemovePlayer(Target);
			if (CommandID == 1)
			{
				BannedIDs.Add(Target->UniqueId);
			}
		}
	}

}

bool AUTLobbyMatchInfo::ServerStartMatch_Validate() { return true; }
void AUTLobbyMatchInfo::ServerStartMatch_Implementation()
{

	if (Players.Num() < MinPlayers)
	{
		GetOwnerPlayerState()->ClientMatchError(NSLOCTEXT("LobbyMessage", "NotEnoughPlayers","There are not enough players in the match to start."));
		return;
	}

	if (CurrentState == ELobbyMatchState::WaitingForPlayers)
	{
		for (int32 i=0;i<Players.Num();i++)
		{
			Players[i]->bReadyToPlay = true;
		}

		LobbyGameState->LaunchGameInstance(this, MatchOptions);
	}
}

bool AUTLobbyMatchInfo::ServerAbortMatch_Validate() { return true; }
void AUTLobbyMatchInfo::ServerAbortMatch_Implementation()
{
	LobbyGameState->TerminateGameInstance(this);
	SetLobbyMatchState(ELobbyMatchState::WaitingForPlayers);
	AUTLobbyPlayerState* OwnerPS = GetOwnerPlayerState();
	if (OwnerPS) OwnerPS->bReadyToPlay = false;
}

void AUTLobbyMatchInfo::GameInstanceReady(FGuid inGameInstanceGUID)
{
	GameInstanceGUID = inGameInstanceGUID.ToString();
	AUTLobbyGameMode* GM = GetWorld()->GetAuthGameMode<AUTLobbyGameMode>();
	if (GM)
	{
		for (int32 i=0;i<Players.Num();i++)
		{
			// Add this player to the players in Match
			PlayersInMatchInstance.Add(FPlayerListInfo(Players[i]));

			// Tell the client to connect to the instance
			Players[i]->ClientConnectToInstance(GameInstanceGUID, GM->ServerInstanceGUID.ToString(),false);
		}
	}
	SetLobbyMatchState(ELobbyMatchState::InProgress);
}

bool AUTLobbyMatchInfo::WasInMatchInstance(AUTLobbyPlayerState* PlayerState)
{
	for (int32 i=0; i<PlayersInMatchInstance.Num();i++)
	{
		if (PlayersInMatchInstance[i].PlayerID == PlayerState->UniqueId)
		{
			return true;
		}
	}
	return false;
}

void AUTLobbyMatchInfo::RemoveFromMatchInstance(AUTLobbyPlayerState* PlayerState)
{
	for (int32 i=0; i<PlayersInMatchInstance.Num();i++)
	{
		if (PlayersInMatchInstance[i].PlayerID == PlayerState->UniqueId)
		{
			PlayersInMatchInstance.RemoveAt(i);
			break;
		}
	}
}


bool AUTLobbyMatchInfo::IsInProgress()
{
	return CurrentState == ELobbyMatchState::InProgress || CurrentState == ELobbyMatchState::Launching || CurrentState == ELobbyMatchState::Completed;
}

bool AUTLobbyMatchInfo::ShouldShowInDock()
{
	return CurrentState == ELobbyMatchState::InProgress || CurrentState == ELobbyMatchState::Launching || CurrentState == ELobbyMatchState::WaitingForPlayers;
}

bool AUTLobbyMatchInfo::ServerSetLobbyMatchState_Validate(FName NewMatchState) { return true; }
void AUTLobbyMatchInfo::ServerSetLobbyMatchState_Implementation(FName NewMatchState)
{
	SetLobbyMatchState(NewMatchState);
}
