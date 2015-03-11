// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/PlayerState.h"
#include "UTLobbyGameMode.h"
#include "UTLobbyMatchInfo.h"
#include "Net/UnrealNetwork.h"


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

	MaxPlayers = 20;
	bSpectatable = true;
	bJoinAnytime = false;
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
	DOREPLIFETIME(AUTLobbyMatchInfo, Players);
	DOREPLIFETIME(AUTLobbyMatchInfo, MatchBadge);
	DOREPLIFETIME(AUTLobbyMatchInfo, PlayersInMatchInstance);
	DOREPLIFETIME(AUTLobbyMatchInfo, MaxPlayers);
	DOREPLIFETIME(AUTLobbyMatchInfo, bJoinAnytime);
	DOREPLIFETIME(AUTLobbyMatchInfo, bSpectatable);
	DOREPLIFETIME(AUTLobbyMatchInfo, RankCeiling);

}

void AUTLobbyMatchInfo::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	SetLobbyMatchState(ELobbyMatchState::Initializing);
	MinPlayers = 2; // TODO: This should be pulled from the game type at some point

	MatchBadge = TEXT("Loading...");
	UpdateBadgeForNewGameMode();
}

bool AUTLobbyMatchInfo::CheckLobbyGameState()
{
	LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	return LobbyGameState != NULL;
}

void AUTLobbyMatchInfo::OnRep_MatchBadge()
{
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
	else
	{
		UpdateBadgeForNewGameMode();
	}
}

void AUTLobbyMatchInfo::OnRep_Players()
{
}

void AUTLobbyMatchInfo::OnRep_PlayersInMatch()
{
}


void AUTLobbyMatchInfo::OnRep_MatchStats()
{
	int32 Seconds;
	if (FParse::Value(*MatchStats, TEXT("ElpasedTime="), Seconds))
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

		MatchElapsedTime = FText::Format(NSLOCTEXT("SUWindowsMidGame", "ClockFormat", "{Hours}:{Minutes}:{Seconds}"), Args);
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
			FTimerHandle TempHandle; 
			GetWorldTimerManager().SetTimer(TempHandle, this, &AUTLobbyMatchInfo::RecycleMatchInfo, 120.0, false);
		}
	}
}

void AUTLobbyMatchInfo::RecycleMatchInfo()
{
	if (CheckLobbyGameState())
	{
		LobbyGameState->RemoveMatch(this);
	}
}

void AUTLobbyMatchInfo::AddPlayer(AUTLobbyPlayerState* PlayerToAdd, bool bIsOwner)
{
	if (bIsOwner && !OwnerId.IsValid())
	{
		OwnerId = PlayerToAdd->UniqueId;
		SetLobbyMatchState(ELobbyMatchState::Setup);
	}
	else
	{
		// Look to see if this player is already in the match

		for (int32 PlayerIdx = 0; PlayerIdx < Players.Num(); PlayerIdx++)
		{
			if (Players[PlayerIdx] == PlayerToAdd) return;	// Quick out if already in the match
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

bool AUTLobbyMatchInfo::MatchIsReadyToJoin(AUTLobbyPlayerState* Joiner)
{
	if (Joiner && CheckLobbyGameState())
	{
		if (CurrentState == ELobbyMatchState::WaitingForPlayers || 
				(CurrentState == ELobbyMatchState::Setup && OwnerId == Joiner->UniqueId) ||
				(CurrentState == ELobbyMatchState::Launching && (bJoinAnytime || OwnerId == Joiner->UniqueId) )
			)
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
		if (Players.Num() < MaxPlayers)
		{
			return NSLOCTEXT("SUMatchPanel","ClickToJoin","Click to Join");
		}
		else
		{
			return NSLOCTEXT("SUMatchPanel","Full","Match is Full");
		}
	}
	else if (CurrentState == ELobbyMatchState::Launching)
	{
		return NSLOCTEXT("SUMatchPanel","Launching","Launching...");
	}
	else if (CurrentState == ELobbyMatchState::InProgress)
	{
		if (bJoinAnytime)
		{
			return NSLOCTEXT("SUMatchPanel","ClickToJoin","Click to Join");
		}
		else if (bSpectatable)
		{
			return NSLOCTEXT("SUMatchPanel","Spectate","Click to Spectate");
		}
		else if (MatchElapsedTime.IsEmpty())
		{
			return NSLOCTEXT("SUMatchPanel","InProgress","In Progress...");
		}
		else
		{
			return MatchElapsedTime;
		}
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
		bJoinAnytime = MatchToCopy->bJoinAnytime;
		bSpectatable = MatchToCopy->bSpectatable;
		MaxPlayers = MatchToCopy->MaxPlayers;
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
	AUTGameMode* StartingGameMode = AUTLobbyGameState::GetGameModeDefaultObject(MatchGameMode);
	if (StartingGameMode)
	{
			MatchMap = StartingGameMode->DefaultLobbyMap;
			MatchOptions = StartingGameMode->DefaultLobbyOptions;
	}
}


bool AUTLobbyMatchInfo::ServerMatchMapChanged_Validate(const FString& NewMatchMap) { return true; }
void AUTLobbyMatchInfo::ServerMatchMapChanged_Implementation(const FString& NewMatchMap)
{
	MatchMap = NewMatchMap;
}

bool AUTLobbyMatchInfo::ServerMatchOptionsChanged_Validate(const FString& NewMatchOptions) { return true; }
void AUTLobbyMatchInfo::ServerMatchOptionsChanged_Implementation(const FString& NewMatchOptions)
{
	// recreate the match options string using the URL code with the old options as a base
	// this a) prevents options the server admin set that are not available in the UI from getting clobbered, and
	// b) prevents the client from removing the admin required server options with bad options strings
	FURL OldURL(NULL, *FString::Printf(TEXT("?%s"), *MatchOptions), TRAVEL_Absolute);
	FURL NewURL(&OldURL, *FString::Printf(TEXT("?%s"), *NewMatchOptions), TRAVEL_Relative);

	NewURL.ToString().Split(TEXT("?"), NULL, &MatchOptions);
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

	if (Players.Num() > MaxPlayers)
	{
		GetOwnerPlayerState()->ClientMatchError(NSLOCTEXT("LobbyMessage", "TooManyPlayers","There are too many players in this match to start."));
		return;
	}

	if (!CheckLobbyGameState() || !LobbyGameState->CanLaunch(this))
	{
		GetOwnerPlayerState()->ClientMatchError(NSLOCTEXT("LobbyMessage", "TooManyInstances","All available game instances are taken.  Please wait a bit and try starting again."));
		return;
	}

	if (CurrentState == ELobbyMatchState::WaitingForPlayers)
	{
		LaunchMatch();
	}
}

void AUTLobbyMatchInfo::LaunchMatch()
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

	FinalOptions += FString::Printf(TEXT("?MaxPlayers=%i"), MaxPlayers);

	AUTGameMode* DefaultGame = AUTLobbyGameState::GetGameModeDefaultObject(MatchGameMode);
	if (DefaultGame)
	{
		if (!DefaultGame->ForcedInstanceGameOptions.IsEmpty())
		{
			FinalOptions += DefaultGame->ForcedInstanceGameOptions;
		}
	}


	if (CheckLobbyGameState())
	{
		LobbyGameState->LaunchGameInstance(this, FinalOptions);
	}

	

}

bool AUTLobbyMatchInfo::ServerAbortMatch_Validate() { return true; }
void AUTLobbyMatchInfo::ServerAbortMatch_Implementation()
{
	if (CheckLobbyGameState())
	{
		LobbyGameState->TerminateGameInstance(this);
	}

	SetLobbyMatchState(ELobbyMatchState::WaitingForPlayers);
	TWeakObjectPtr<AUTLobbyPlayerState> OwnerPS = GetOwnerPlayerState();
	if (OwnerPS.IsValid()) OwnerPS->bReadyToPlay = false;
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
	return OwnerId.IsValid() && (Players.Num() > 0 || PlayersInMatchInstance.Num() > 0) && (CurrentState == ELobbyMatchState::InProgress || CurrentState == ELobbyMatchState::Launching || CurrentState == ELobbyMatchState::WaitingForPlayers);
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
	if (CheckLobbyGameState())
	{
		AvailableMaps.Empty();
		for (int32 i=0; i<LobbyGameState->ClientAvailableMaps.Num();i++)
		{
			if (CurrentGameModeData->DefaultObject->SupportsMap(LobbyGameState->ClientAvailableMaps[i]->MapName))
			{
				// TODO: we should modify the server to not send unentitled things in the first place
				if (LocallyHasEntitlement(GetRequiredEntitlementFromPackageName(FName(*LobbyGameState->ClientAvailableMaps[i]->MapName))))
				{
					AvailableMaps.Add(LobbyGameState->ClientAvailableMaps[i]);
				}
			}
		}
	}
}

bool AUTLobbyMatchInfo::ServerSetMaxPlayers_Validate(int32 NewMaxPlayers) { return true; }
void AUTLobbyMatchInfo::ServerSetMaxPlayers_Implementation(int32 NewMaxPlayers)
{
	MaxPlayers = FMath::Clamp<int32>(NewMaxPlayers, 2, 20);
}


bool AUTLobbyMatchInfo::ServerSetAllowJoinInProgress_Validate(bool bAllow) { return true; }
void AUTLobbyMatchInfo::ServerSetAllowJoinInProgress_Implementation(bool bAllow)
{
	bJoinAnytime = bAllow;
}

bool AUTLobbyMatchInfo::ServerSetAllowSpectating_Validate(bool bAllow) { return true; }
void AUTLobbyMatchInfo::ServerSetAllowSpectating_Implementation(bool bAllow)
{
	bSpectatable = bAllow;
}

bool AUTLobbyMatchInfo::ServerSetRankCeiling_Validate(int32 NewRankCeiling) { return true; }
void AUTLobbyMatchInfo::ServerSetRankCeiling_Implementation(int32 NewRankCeiling)
{
	RankCeiling = NewRankCeiling;
}



FText AUTLobbyMatchInfo::GetDebugInfo()
{

	FText Owner = NSLOCTEXT("UTLobbyMatchInfo","NoOwner","NONE");
	if (OwnerId.IsValid())
	{
		if (Players.Num() > 0 && Players[0].IsValid()) Owner = FText::FromString(Players[0]->PlayerName);
		else Owner = FText::FromString(OwnerId.ToString());
	}


	FFormatNamedArguments Args;
	Args.Add(TEXT("OwnerName"), Owner);
	Args.Add(TEXT("CurrentState"), FText::FromName(CurrentState));
	Args.Add(TEXT("ShouldShowInDock"), FText::AsNumber(ShouldShowInDock()));
	Args.Add(TEXT("InProgress"), FText::AsNumber(IsInProgress()));
	Args.Add(TEXT("MatchStats"), FText::FromString(MatchStats));


	return FText::Format(NSLOCTEXT("UTLobbyMatchInfo","DebugFormat","Owner [{OwnerName}] State [{CurrentState}] Flags [{ShouldShowInDock}, {InProgress}]  Stats: {MatchStats}"), Args);
}


void AUTLobbyMatchInfo::UpdateBadgeForNewGameMode()
{
#if !UE_SERVER
	if (GetNetMode() != NM_DedicatedServer)
	{
		AUTGameMode* DefaultGame = AUTLobbyGameState::GetGameModeDefaultObject(MatchGameMode);
		if (DefaultGame && (CurrentState == ELobbyMatchState::Initializing || CurrentState == ELobbyMatchState::Setup || CurrentState == ELobbyMatchState::WaitingForPlayers))
		{
			MatchBadge = DefaultGame->GetHUBPregameFormatString();
		}
	}
#endif
}