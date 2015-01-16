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

void AUTLobbyMatchInfo::UpdateGameMode()
{
 	if (MatchGameMode == TEXT(""))
	{
		CurrentGameModeData.Reset();
	}
}

void AUTLobbyMatchInfo::OnRep_MatchGameMode()
{
	UpdateGameMode();
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

bool AUTLobbyMatchInfo::MatchIsReadyToJoin(AUTLobbyPlayerState* Joiner)
{
	if (Joiner && LobbyGameState)
	{
		if (CurrentState == ELobbyMatchState::WaitingForPlayers || (CurrentState == ELobbyMatchState::Setup && OwnerId == Joiner->UniqueId))
		{
			return (MatchGameMode != TEXT("") && MatchMap != TEXT("") && MatchOptions != TEXT("") && OwnerId.IsValid() && LobbyGameState->GetGameModeDefaultObject(MatchGameMode) );
		}
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

	// If we have a match to copy then copy it's values here
	if (MatchToCopy)
	{
		MatchGameMode = MatchToCopy->MatchGameMode;
		MatchOptions = MatchToCopy->MatchOptions;
		MatchMap = MatchToCopy->MatchMap;
	}
	else
	{
		// Set the defaults for this match...

		MatchGameMode = GameState->AllowedGameModeClasses[0];
		AUTGameMode* StartingGameMode = AUTLobbyGameState::GetGameModeDefaultObject(MatchGameMode);
		if (StartingGameMode)
		{
			 MatchMap = StartingGameMode->DefaultLobbyMap;
			 MatchOptions = StartingGameMode->DefaultLobbyOptions;
		}
	}
}

bool AUTLobbyMatchInfo::ServerMatchGameModeChanged_Validate(const FString& NewMatchGameMode) { return true; }
void AUTLobbyMatchInfo::ServerMatchGameModeChanged_Implementation(const FString& NewMatchGameMode)
{
	MatchGameMode = NewMatchGameMode;
}


bool AUTLobbyMatchInfo::ServerMatchMapChanged_Validate(const FString& NewMatchMap) { return true; }
void AUTLobbyMatchInfo::ServerMatchMapChanged_Implementation(const FString& NewMatchMap)
{
	MatchMap = NewMatchMap;
}

bool AUTLobbyMatchInfo::ServerMatchOptionsChanged_Validate(const FString& NewMatchOptions) { return true; }
void AUTLobbyMatchInfo::ServerMatchOptionsChanged_Implementation(const FString& NewMatchOptions)
{
	MatchOptions = NewMatchOptions;
}

bool AUTLobbyMatchInfo::ServerMatchIsReadyForPlayers_Validate() { return true; }
void AUTLobbyMatchInfo::ServerMatchIsReadyForPlayers_Implementation()
{
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

		// Remove out the [Min/Max] info...

		FString FinalOptions = MatchOptions;

		int32 LeftBracket = FinalOptions.Find(TEXT("["));
		while (LeftBracket >=0)
		{
			FString Left = LeftBracket > 0 ? FinalOptions.Left(LeftBracket) : TEXT("");
			int32 RightBracket = FinalOptions.Find(TEXT("]"));
			FString Right = RightBracket >= 0 ? FinalOptions.Right(FinalOptions.Len() - RightBracket -1) : FinalOptions.Right(FinalOptions.Len() - LeftBracket);
			FinalOptions = Left + Right;
			LeftBracket = FinalOptions.Find(TEXT("["));
		}

		AUTGameMode* DefaultGame = AUTLobbyGameState::GetGameModeDefaultObject(MatchGameMode);
		if (DefaultGame)
		{
			if (!DefaultGame->ForcedInstanceGameOptions.IsEmpty())
			{
				FinalOptions += DefaultGame->ForcedInstanceGameOptions;
			}
		}

		LobbyGameState->LaunchGameInstance(this, FinalOptions);
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


void AUTLobbyMatchInfo::ClientGetDefaultGameOptions()
{
	if (CurrentGameModeData.IsValid())
	{
		MatchOptions = CurrentGameModeData->DefaultObject->GetDefaultLobbyOptions();
		ServerMatchOptionsChanged(MatchOptions);
	}
}

void AUTLobbyMatchInfo::BuildAllowedMapsList()
{
	if (LobbyGameState)
	{
		AvailableMaps.Empty();
		for (int32 i=0; i<LobbyGameState->ClientAvailableMaps.Num();i++)
		{
			if (CurrentGameModeData->DefaultObject->SupportsMap(LobbyGameState->ClientAvailableMaps[i]->MapName))
			{
				AvailableMaps.Add(LobbyGameState->ClientAvailableMaps[i]);
			}
		}
	}
}