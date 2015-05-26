// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/PlayerState.h"
#include "UTLobbyGameMode.h"
#include "UTLobbyMatchInfo.h"
#include "Net/UnrealNetwork.h"
#include "UTGameEngine.h"
#include "UTLevelSummary.h"
#include "UTReplicatedGameRuleset.h"


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

	bSpectatable = true;
	bJoinAnytime = true;
	bMapListChanged = false;

	BotSkillLevel = -1;

}


void AUTLobbyMatchInfo::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AUTLobbyMatchInfo, OwnerId);
	DOREPLIFETIME(AUTLobbyMatchInfo, CurrentState);
	DOREPLIFETIME(AUTLobbyMatchInfo, bPrivateMatch);
	DOREPLIFETIME(AUTLobbyMatchInfo, MatchStats);
	DOREPLIFETIME(AUTLobbyMatchInfo, CurrentRuleset);
	DOREPLIFETIME(AUTLobbyMatchInfo, Players);
	DOREPLIFETIME(AUTLobbyMatchInfo, MatchBadge);
	DOREPLIFETIME(AUTLobbyMatchInfo, MapList);
	DOREPLIFETIME(AUTLobbyMatchInfo, PlayersInMatchInstance);
	DOREPLIFETIME(AUTLobbyMatchInfo, bJoinAnytime);
	DOREPLIFETIME(AUTLobbyMatchInfo, bSpectatable);
	DOREPLIFETIME(AUTLobbyMatchInfo, RankCeiling);
	DOREPLIFETIME(AUTLobbyMatchInfo, BotSkillLevel);
	DOREPLIFETIME_CONDITION(AUTLobbyMatchInfo, bDedicatedMatch, COND_InitialOnly);
}

void AUTLobbyMatchInfo::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	SetLobbyMatchState(ELobbyMatchState::Initializing);

	MatchBadge = TEXT("Loading...");
}

bool AUTLobbyMatchInfo::CheckLobbyGameState()
{
	LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	return LobbyGameState != NULL;
}


void AUTLobbyMatchInfo::SetLobbyMatchState(FName NewMatchState)
{
	if ((CurrentState != ELobbyMatchState::Recycling || NewMatchState == ELobbyMatchState::Dead) && CurrentState != ELobbyMatchState::Dead)
	{
		// When the client receives it's startup info, it will attempt to switch the match's state from Setup to waiting for players
		// but if we are Gamenching due to quickstart we don't want that.
		//if (NewMatchState != ELobbyMatchState::WaitingForPlayers || CurrentState != ELobbyMatchState::Launching)
		//{
			CurrentState = NewMatchState;
			if (CurrentState == ELobbyMatchState::Recycling)
			{
				FTimerHandle TempHandle; 
				GetWorldTimerManager().SetTimer(TempHandle, this, &AUTLobbyMatchInfo::RecycleMatchInfo, 120.0, false);
			}
		//}
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
			if (Players[PlayerIdx].IsValid() && Players[PlayerIdx] == PlayerToAdd)
			{
				return;
			}
		}

		if (IsBanned(PlayerToAdd->UniqueId))
		{
			PlayerToAdd->ClientMatchError(NSLOCTEXT("LobbyMessage","Banned","You do not have permission to enter this match."));
		}
		// Add code for private/invite only matches
	}
	
	Players.Add(PlayerToAdd);
	PlayerToAdd->AddedToMatch(this);
	PlayerToAdd->ChatDestination = ChatDestinations::Match;

	// Players default to ready
	PlayerToAdd->bReadyToPlay = true;
}

bool AUTLobbyMatchInfo::RemovePlayer(AUTLobbyPlayerState* PlayerToRemove)
{
	// Owners remove everyone and kill the match
	if (OwnerId == PlayerToRemove->UniqueId)
	{
		// The host is removing this match, notify everyone.
		for (int32 i=0;i<Players.Num();i++)
		{
			if (Players[i].IsValid())
			{
				Players[i]->RemovedFromMatch(this);
			}
		}
		Players.Empty();

		// We are are not launching, kill this lobby otherwise keep it around
		if ( CurrentState == ELobbyMatchState::Launching || !IsInProgress() )
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
		if (	CurrentState == ELobbyMatchState::WaitingForPlayers || 
		   (    CurrentState == ELobbyMatchState::Setup && OwnerId == Joiner->UniqueId) ||
		   (	CurrentState == ELobbyMatchState::Launching && (bJoinAnytime || OwnerId == Joiner->UniqueId) )
		   )
		{
			return ( OwnerId.IsValid() );
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
		if (MatchHasRoom())
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
		else 
		{
			return NSLOCTEXT("SUMatchPanel","InProgress","In Progress...");
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
	if (MatchToCopy)
	{
		SetRules(MatchToCopy->CurrentRuleset, MatchToCopy->MapList);
		BotSkillLevel = MatchToCopy->BotSkillLevel;
	}

	SetLobbyMatchState(ELobbyMatchState::Setup);
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
	if (Players.Num() < CurrentRuleset->MinPlayersToStart)
	{
		GetOwnerPlayerState()->ClientMatchError(NSLOCTEXT("LobbyMessage", "NotEnoughPlayers","There are not enough players in the match to start."));
		return;
	}

	if (Players.Num() > CurrentRuleset->MaxPlayers)
	{
		GetOwnerPlayerState()->ClientMatchError(NSLOCTEXT("LobbyMessage", "TooManyPlayers","There are too many players in this match to start."));
		return;
	}

	// TODO: need to check for ready ups on server side

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

	if (CheckLobbyGameState() && CurrentRuleset.IsValid())
	{
		if (MapList.Num() > 0)
		{
			// build all of the data needed to launch the map.

			FString GameURL = FString::Printf(TEXT("%s?Game=%s?MaxPlayers=%i"),*MapList[0], *CurrentRuleset->GameMode, CurrentRuleset->MaxPlayers);
			GameURL += CurrentRuleset->GameOptions;

			if (!CurrentRuleset->bCustomRuleset)
			{
				// Custom rules already have their bot info set
				
				// Load the level summary of this map.
				UUTLevelSummary* Summary = UUTGameEngine::LoadLevelSummary(MapList[0]);
				int32 OptimalPlayerCount = CurrentRuleset->bTeamGame ? Summary->OptimalTeamPlayerCount : Summary->OptimalPlayerCount;

				if (BotSkillLevel >= 0)
				{
					GameURL += FString::Printf(TEXT("?BotFill=%i?Difficulty=%i"), OptimalPlayerCount, FMath::Clamp<int32>(BotSkillLevel,0,7));			
				}
			}

			LobbyGameState->LaunchGameInstance(this, GameURL);
		}
		else
		{
			GetOwnerPlayerState()->ClientMatchError(NSLOCTEXT("LobbyMessage", "NoMaps","Please Select a map to play."));		
		}
	}
}

bool AUTLobbyMatchInfo::ServerAbortMatch_Validate() { return true; }
void AUTLobbyMatchInfo::ServerAbortMatch_Implementation()
{
	if (CheckLobbyGameState())
	{
		LobbyGameState->TerminateGameInstance(this, true);
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

			if (Players[i].IsValid())
			{
				// Add this player to the players in Match
				PlayersInMatchInstance.Add(FPlayerListInfo(Players[i]));

				// Tell the client to connect to the instance
				Players[i]->ClientConnectToInstance(GameInstanceGUID, GM->ServerInstanceGUID.ToString(),false);
			}
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
	if (bDedicatedMatch)
	{
		// dedicated instances always show unless they are dead
		return (CurrentState != ELobbyMatchState::Aborting && CurrentState != ELobbyMatchState::Dead && CurrentState != ELobbyMatchState::Recycling);
	}
	else
	{
		return OwnerId.IsValid() && (Players.Num() > 0 || PlayersInMatchInstance.Num() > 0) && 
				CurrentRuleset.IsValid() && 
				(CurrentState == ELobbyMatchState::InProgress || CurrentState == ELobbyMatchState::Launching || CurrentState == ELobbyMatchState::WaitingForPlayers);
	}
}

bool AUTLobbyMatchInfo::ServerSetLobbyMatchState_Validate(FName NewMatchState) { return true; }
void AUTLobbyMatchInfo::ServerSetLobbyMatchState_Implementation(FName NewMatchState)
{
	SetLobbyMatchState(NewMatchState);
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
	Args.Add(TEXT("CurrentRuleSet"), FText::FromString(CurrentRuleset.IsValid() ? CurrentRuleset->Title : TEXT("None")));
	Args.Add(TEXT("ShouldShowInDock"), FText::AsNumber(ShouldShowInDock()));
	Args.Add(TEXT("InProgress"), FText::AsNumber(IsInProgress()));
	Args.Add(TEXT("MatchStats"), FText::FromString(MatchStats));


	return FText::Format(NSLOCTEXT("UTLobbyMatchInfo","DebugFormat","Owner [{OwnerName}] State [{CurrentState}] RuleSet [{CurrentRuleSet}] Flags [{ShouldShowInDock}, {InProgress}]  Stats: {MatchStats}"), Args);
}

void AUTLobbyMatchInfo::OnRep_CurrentRuleset()
{
	OnRep_Update();
	OnRulesetUpdatedDelegate.ExecuteIfBound();
}


void AUTLobbyMatchInfo::OnRep_Update()
{
	// Let the UI know
	OnMatchInfoUpdatedDelegate.ExecuteIfBound();
}

void AUTLobbyMatchInfo::OnRep_MapList()
{
	bMapListChanged = true;
}

FString AUTLobbyMatchInfo::GetMapList()
{
	FString Maps = TEXT("");
	if (CurrentRuleset.IsValid() && CurrentRuleset->MapPlaylist.Num() >0)
	{
		for (int32 i=0; i<CurrentRuleset->MapPlaylist.Num(); i++)
		{
			Maps += i > 0 ? TEXT(", ") + CurrentRuleset->MapPlaylist[i] : CurrentRuleset->MapPlaylist[i];
		}
	}

	return Maps;
}

void AUTLobbyMatchInfo::SetRules(TWeakObjectPtr<AUTReplicatedGameRuleset> NewRuleset, const TArray<FString>& NewMapList)
{
	CurrentRuleset = NewRuleset;
	MapList.Empty();
	for (int32 i=0; i < NewMapList.Num(); i++)
	{
		MapList.Add(NewMapList[i]);
	}

	bMapListChanged = true;
}

bool AUTLobbyMatchInfo::ServerSetRules_Validate(const FString& RulesetTag, const TArray<FString>& NewMapList,int32 NewBotSkillLevel) { return true; }
void AUTLobbyMatchInfo::ServerSetRules_Implementation(const FString&RulesetTag, const TArray<FString>& NewMapList,int32 NewBotSkillLevel)
{
	if ( CheckLobbyGameState() )
	{
		TWeakObjectPtr<AUTReplicatedGameRuleset> NewRuleSet = LobbyGameState->FindRuleset(RulesetTag);

		if (NewRuleSet.IsValid())
		{
			SetRules(NewRuleSet, NewMapList);
		}

		BotSkillLevel = NewBotSkillLevel;

	}
}

void AUTLobbyMatchInfo::SetMatchStats(FString Update)
{
	MatchStats = Update;
	OnRep_MatchStats();
}

void AUTLobbyMatchInfo::OnRep_MatchStats()
{
	int32 GameTime;
	if ( FParse::Value(*MatchStats, TEXT("GameTime="), GameTime) )
	{
		MatchGameTime = GameTime;
	}
}

// Unmounts and/or deletes stale packages per the current ruleset
bool AUTLobbyMatchInfo::GetNeededPackagesForCurrentRuleset(TArray<FString>& NeededPackageURLs)
{
	NeededPackageURLs.Empty();
	for (int32 i = 0; i < CurrentRuleset->RequiredPackages.Num(); i++)
	{
		FString Path = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("DownloadedPaks"), *CurrentRuleset->RequiredPackages[i].PackageName) + TEXT(".pak");
		UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
		if (UTEngine)
		{
			if (UTEngine->LocalContentChecksums.Contains(CurrentRuleset->RequiredPackages[i].PackageName))
			{
				if (UTEngine->LocalContentChecksums[CurrentRuleset->RequiredPackages[i].PackageName] == CurrentRuleset->RequiredPackages[i].PackageChecksum)
				{
					continue;
				}
				else
				{
					// Local content has a non-matching md5, not sure if we should try to unmount/delete it
					return false;
				}
			}

			if (UTEngine->MountedDownloadedContentChecksums.Contains(CurrentRuleset->RequiredPackages[i].PackageName))
			{
				if (UTEngine->MountedDownloadedContentChecksums[CurrentRuleset->RequiredPackages[i].PackageName] == CurrentRuleset->RequiredPackages[i].PackageChecksum)
				{
					// We've already mounted the content needed and the checksum matches
					continue;
				}
				else
				{
					// Unmount the pak
					if (FCoreDelegates::OnUnmountPak.IsBound())
					{
						FCoreDelegates::OnUnmountPak.Execute(Path);
					}

					// Remove the CRC entry
					UTEngine->MountedDownloadedContentChecksums.Remove(CurrentRuleset->RequiredPackages[i].PackageName);
					UTEngine->DownloadedContentChecksums.Remove(CurrentRuleset->RequiredPackages[i].PackageName);

					// Delete the original file
					FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
				}
			}

			if (UTEngine->DownloadedContentChecksums.Contains(CurrentRuleset->RequiredPackages[i].PackageName))
			{
				if (UTEngine->DownloadedContentChecksums[CurrentRuleset->RequiredPackages[i].PackageName] == CurrentRuleset->RequiredPackages[i].PackageChecksum)
				{
					// Mount the pak
					if (FCoreDelegates::OnMountPak.IsBound())
					{
						FCoreDelegates::OnMountPak.Execute(Path, 0);
						UTEngine->MountedDownloadedContentChecksums.Add(CurrentRuleset->RequiredPackages[i].PackageName, CurrentRuleset->RequiredPackages[i].PackageChecksum);
					}

					continue;
				}
				else
				{
					UTEngine->DownloadedContentChecksums.Remove(CurrentRuleset->RequiredPackages[i].PackageName);

					// Delete the original file
					FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
				}
			}

			NeededPackageURLs.Add(CurrentRuleset->RequiredPackages[i].PackageURL);
		}
	}

	return true;
}

bool AUTLobbyMatchInfo::ServerCreateCustomRule_Validate(const FString& GameMode, const FString& StartingMap, const TArray<FString>& GameOptions, int32 DesiredSkillLevel, int32 DesiredPlayerCount) { return true; }
void AUTLobbyMatchInfo::ServerCreateCustomRule_Implementation(const FString& GameMode, const FString& StartingMap, const TArray<FString>& GameOptions, int32 DesiredSkillLevel, int32 DesiredPlayerCount)
{
	// We need to build a one off custom replicated ruleset just for this hub.  :)

	AUTLobbyGameState* GameState = GetWorld()->GetGameState<AUTLobbyGameState>();

	FActorSpawnParameters Params;
	Params.Owner = GameState;
	AUTReplicatedGameRuleset* NewReplicatedRuleset = GetWorld()->SpawnActor<AUTReplicatedGameRuleset>(Params);
	if (NewReplicatedRuleset)
	{

		// Look up the game mode in the AllowedGameData array and get the text description.

		FString Desc = TEXT("A custom match.");
		for (int32 i=0;i<GameState->AllowedGameData.Num();i++)
		{
			if (GameState->AllowedGameData[i].Package.Equals(GameMode, ESearchCase::IgnoreCase))
			{
				Desc = FString::Printf(TEXT("A custom %s match!"), *GameState->AllowedGameData[i].MenuDescription);
				break;
			}
		
		}

		NewReplicatedRuleset->Title = TEXT("Custom Rule");
		NewReplicatedRuleset->Tooltip = TEXT("");
		NewReplicatedRuleset->Description = Desc + TEXT("\nBe warned, anything goes in here, so enter at your own risk.\n");

		int32 PlayerCount = 20;

		for (int32 i=0; i < GameOptions.Num(); i++)
		{
			NewReplicatedRuleset->Description += GameOptions[i] + TEXT("\n");
		}

		NewReplicatedRuleset->MaxPlayers = DesiredPlayerCount;
		if (DesiredSkillLevel >= 0)
		{
			//NewReplicatedRuleset->
		}

		NewReplicatedRuleset->GameMode = GameMode;
		NewReplicatedRuleset->GameModeClass = GameMode;

		FString FinalGameOptions = TEXT("");
		for (int32 i=0; i<GameOptions.Num();i++)
		{
			FinalGameOptions += TEXT("?") + GameOptions[i];
		}

		if (DesiredSkillLevel >= 0)
		{
			// Load the level summary of this map.
			UUTLevelSummary* Summary = UUTGameEngine::LoadLevelSummary(StartingMap);

			// This match wants bots.  
			int32 OptimalPlayerCount = NewReplicatedRuleset->GetDefaultGameModeObject()->bTeamGame ? Summary->OptimalTeamPlayerCount : Summary->OptimalPlayerCount;
			FinalGameOptions += FString::Printf(TEXT("?BotFill=%i?Difficulty=%i"), OptimalPlayerCount, FMath::Clamp<int32>(DesiredSkillLevel,0,7));				
		}

		NewReplicatedRuleset->GameOptions = FinalGameOptions;


		NewReplicatedRuleset->MapPlaylistSize = 1;
		NewReplicatedRuleset->MapPlaylist.Add(StartingMap);
		NewReplicatedRuleset->MinPlayersToStart = 2;
		NewReplicatedRuleset->MaxPlayers = 16;
		NewReplicatedRuleset->DisplayTexture = "Texture2D'/Game/RestrictedAssets/UI/GameModeBadges/GB_Custom.GB_Custom'";
		NewReplicatedRuleset->bCustomRuleset = true;

		MapList.Add(StartingMap);

		// Add code to setup the required packages array
		CurrentRuleset = NewReplicatedRuleset;
	}

}

bool AUTLobbyMatchInfo::IsBanned(FUniqueNetIdRepl Who)
{
	for (int32 i=0;i<BannedIDs.Num();i++)
	{
		if (Who == BannedIDs[i])
		{
			return true;
		}
	}

	return false;
}
