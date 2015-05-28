// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GameMode.cpp: AGameMode c++ code.
=============================================================================*/

#include "EnginePrivate.h"
#include "Engine/LevelScriptActor.h"
#include "GameFramework/GameNetworkManager.h"
#include "Matinee/MatineeActor.h"
#include "OnlineSubsystemUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/PlayerStartPIE.h"
#include "GameFramework/EngineMessage.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/CheatManager.h"
#include "GameFramework/GameMode.h"
#include "Engine/ChildConnection.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameMode, Log, All);

namespace MatchState
{
	const FName EnteringMap = FName(TEXT("EnteringMap"));
	const FName WaitingToStart = FName(TEXT("WaitingToStart"));
	const FName InProgress = FName(TEXT("InProgress"));
	const FName WaitingPostMatch = FName(TEXT("WaitingPostMatch"));
	const FName LeavingMap = FName(TEXT("LeavingMap"));
	const FName Aborted = FName(TEXT("Aborted"));
}

AGameMode::AGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
		.DoNotCreateDefaultSubobject(TEXT("Sprite"))
	)
{
	bStartPlayersAsSpectators = false;
	bDelayedStart = false;
	bNetLoadOnClient = false;

	// One-time initialization
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	HUDClass = AHUD::StaticClass();
	MatchState = MatchState::EnteringMap;
	bPauseable = true;
	DefaultPawnClass = ADefaultPawn::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
	SpectatorClass = ASpectatorPawn::StaticClass();
	EngineMessageClass = UEngineMessage::StaticClass();
	GameStateClass = AGameState::StaticClass();
	CurrentID = 1;
	PlayerStateClass = APlayerState::StaticClass();
	MinRespawnDelay = 1.0f;
	InactivePlayerStateLifeSpan = 300.f;
}

FString AGameMode::GetNetworkNumber()
{
	UNetDriver* NetDriver = GetWorld()->GetNetDriver();
	return NetDriver ? NetDriver->LowLevelGetNetworkNumber() : FString(TEXT(""));
}

void AGameMode::SwapPlayerControllers(APlayerController* OldPC, APlayerController* NewPC)
{
	if (OldPC != NULL && !OldPC->IsPendingKill() && NewPC != NULL && !NewPC->IsPendingKill() && OldPC->Player != NULL)
	{
		// move the Player to the new PC
		UPlayer* Player = OldPC->Player;
		NewPC->NetPlayerIndex = OldPC->NetPlayerIndex; //@warning: critical that this is first as SetPlayer() may trigger RPCs
		NewPC->SetPlayer(Player);
		NewPC->NetConnection = OldPC->NetConnection;
		NewPC->CopyRemoteRoleFrom( OldPC );

		K2_OnSwapPlayerControllers(OldPC, NewPC);

		// send destroy event to old PC immediately if it's local
		if (Cast<ULocalPlayer>(Player))
		{
			GetWorld()->DestroyActor(OldPC);
		}
		else
		{
			OldPC->PendingSwapConnection = Cast<UNetConnection>(Player);
			//@note: at this point, any remaining RPCs sent by the client on the old PC will be discarded
			// this is consistent with general owned Actor destruction,
			// however in this particular case it could easily be changed
			// by modifying UActorChannel::ReceivedBunch() to account for PendingSwapConnection when it is setting bNetOwner
		}
	}
	else
	{
		UE_LOG(LogGameMode, Warning, TEXT("SwapPlayerControllers(): Invalid OldPC, invalid NewPC, or OldPC has no Player!"));
	}
}

void AGameMode::ForceClearUnpauseDelegates( AActor* PauseActor )
{
	if ( PauseActor != NULL )
	{
		bool bUpdatePausedState = false;
		for ( int32 PauserIdx = Pausers.Num() - 1; PauserIdx >= 0; PauserIdx-- )
		{
			FCanUnpause& CanUnpauseDelegate = Pausers[PauserIdx];
			if ( CanUnpauseDelegate.GetUObject() == PauseActor )
			{
				Pausers.RemoveAt(PauserIdx);
				bUpdatePausedState = true;
			}
		}

		// if we removed some CanUnpause delegates, we may be able to unpause the game now
		if ( bUpdatePausedState )
		{
			ClearPause();
		}

		APlayerController* PC = Cast<APlayerController>(PauseActor);
		AWorldSettings * WorldSettings = GetWorldSettings();
		if ( PC != NULL && PC->PlayerState != NULL && WorldSettings != NULL && WorldSettings->Pauser == PC->PlayerState )
		{
			// try to find another player to be the worldsettings's Pauser
			for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
			{
				APlayerController* Player = *Iterator;
				if (Player->PlayerState != NULL
					&&	Player->PlayerState != PC->PlayerState
					&&	!Player->IsPendingKillPending() && !Player->PlayerState->IsPendingKillPending())
				{
					WorldSettings->Pauser = Player->PlayerState;
					break;
				}
			}

			// if it's still pointing to the original player's PlayerState, clear it completely
			if ( WorldSettings->Pauser == PC->PlayerState )
			{
				WorldSettings->Pauser = NULL;
			}
		}
	}
}

void AGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	UWorld* World = GetWorld();

	// save Options for future use
	OptionsString = Options;

	UClass* const SessionClass = GetGameSessionClass();
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Instigator;
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save game sessions into a map
	GameSession = World->SpawnActor<AGameSession>(SessionClass, SpawnInfo);
	GameSession->InitOptions(Options);

	if (GetNetMode() != NM_Standalone)
	{
		FOnlineSessionSettings* SessionSettings = NULL;
		IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
		if (SessionInt.IsValid())
		{
			SessionSettings = SessionInt->GetSessionSettings(GameSessionName);
		}

		// Attempt to login, returning true means an async login is in flight
		if (!SessionSettings && !GameSession->ProcessAutoLogin())
		{
			GameSession->RegisterServer();
		}
	}

	SetMatchState(MatchState::EnteringMap);
}

bool AGameMode::SetPause(APlayerController* PC, FCanUnpause CanUnpauseDelegate)
{
	if ( AllowPausing(PC) )
	{
		// Add it for querying
		Pausers.Add(CanUnpauseDelegate);

		// Let the first one in "own" the pause state
		AWorldSettings * WorldSettings = GetWorldSettings();
		if (WorldSettings->Pauser == NULL)
		{
			WorldSettings->Pauser = PC->PlayerState;
		}
		return true;
	}
	return false;
}

void AGameMode::RestartGame()
{
	if ( GameSession->CanRestartGame() )
	{
		if (GetMatchState() == MatchState::LeavingMap)
		{
			return;
		}

		GetWorld()->ServerTravel("?Restart",GetTravelType());
	}
}

void AGameMode::ReturnToMainMenuHost()
{
	if (GameSession)
	{
		GameSession->ReturnToMainMenuHost();
	}
}

void AGameMode::PostLogin( APlayerController* NewPlayer )
{
	UWorld* World = GetWorld();

	// update player count
	if (NewPlayer->PlayerState->bOnlySpectator)
	{
		NumSpectators++;
	}
	else if (World->IsInSeamlessTravel() || NewPlayer->HasClientLoadedCurrentWorld())
	{
		NumPlayers++;
	}
	else
	{
		NumTravellingPlayers++;
	}

	// save network address for re-associating with reconnecting player, after stripping out port number
	FString Address = NewPlayer->GetPlayerNetworkAddress();
	int32 pos = Address.Find(TEXT(":"), ESearchCase::CaseSensitive);
	NewPlayer->PlayerState->SavedNetworkAddress = (pos > 0) ? Address.Left(pos) : Address;

	// check if this player is reconnecting and already has PlayerState
	FindInactivePlayer(NewPlayer);

	StartNewPlayer(NewPlayer);
	NewPlayer->ClientCapBandwidth(NewPlayer->Player->CurrentNetSpeed);
	if(World->NetworkManager)
	{
		World->NetworkManager->UpdateNetSpeeds(true);	 // @TODO ONLINE - Revisit LAN netspeeds
	}

	GenericPlayerInitialization(NewPlayer);

	if (NewPlayer->PlayerState->bOnlySpectator)
	{
		NewPlayer->ClientGotoState(NAME_Spectating);
	}

	if (GameSession)
	{
		GameSession->PostLogin(NewPlayer);
	}

	// add the player to any matinees running so that it gets in on any cinematics already running, etc
	TArray<AMatineeActor*> AllMatineeActors;
	World->GetMatineeActors(AllMatineeActors);
	for (int32 i = 0; i < AllMatineeActors.Num(); i++)
	{
		AllMatineeActors[i]->AddPlayerToDirectorTracks(NewPlayer);
	}

	bool HidePlayer=false, HideHUD=false, DisableMovement=false, DisableTurning=false;

	//Check to see if we should start in cinematic mode (matinee movie capture)
	if(ShouldStartInCinematicMode(HidePlayer, HideHUD, DisableMovement, DisableTurning))
	{
		NewPlayer->SetCinematicMode(true, HidePlayer, HideHUD, DisableMovement, DisableTurning);
	}

	// Tell the player to enable voice by default or use the push to talk method
	NewPlayer->ClientEnableNetworkVoice(!GameSession->RequiresPushToTalk());

	// Notify Blueprints that a new player has logged in.  Calling it here, because this is the first time that the PlayerController can take RPCs
	K2_PostLogin(NewPlayer);
}

bool AGameMode::ShouldStartInCinematicMode(bool& OutHidePlayer,bool& OutHideHUD,bool& OutDisableMovement,bool& OutDisableTurning)
{
	bool StartInCinematicMode = false;
	if(GEngine->MatineeScreenshotOptions.bStartWithMatineeCapture)
	{
		GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("CinematicMode"), StartInCinematicMode, GEditorPerProjectIni );
		if(StartInCinematicMode)
		{
			GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableMovement"), OutDisableMovement, GEditorPerProjectIni );
			GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("DisableTurning"), OutDisableTurning, GEditorPerProjectIni );
			GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("HidePlayer"), OutHidePlayer, GEditorPerProjectIni );
			GConfig->GetBool( TEXT("MatineeCreateMovieOptions"), TEXT("HideHUD"), OutHideHUD, GEditorPerProjectIni );
			return StartInCinematicMode;
		}
	}
	return StartInCinematicMode;
}


void AGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	PlayerPawn->SetPlayerDefaults();
}

void AGameMode::Logout( AController* Exiting )
{
	APlayerController* PC = Cast<APlayerController>(Exiting);
	if ( PC != NULL )
	{
		K2_OnLogout(Exiting);

		RemovePlayerControllerFromPlayerCount(PC);

		if (GameSession)
		{
			GameSession->NotifyLogout(PC);
		}

		// @TODO ONLINE - Revisit LAN netspeeds
		UWorld* World = GetWorld();
		if (World && World->NetworkManager)
		{
			World->NetworkManager->UpdateNetSpeeds(true);
		}
	}
}

void AGameMode::InitGameState()
{
	GameState->GameModeClass = GetClass();
	GameState->ReceivedGameModeClass();
	GameState->SpectatorClass = SpectatorClass;
	GameState->ReceivedSpectatorClass();
}


AActor* AGameMode::FindPlayerStart_Implementation( AController* Player, const FString& IncomingName )
{
	UWorld* World = GetWorld();

	// if incoming start is specified, then just use it
	if( !IncomingName.IsEmpty() )
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			APlayerStart* Start = *It;
			if (Start && Start->PlayerStartTag == FName(*IncomingName))
			{
				return Start;
			}
		}
	}

	// always pick StartSpot at start of match
	if ( ShouldSpawnAtStartSpot(Player) )
	{
		return Player->StartSpot.Get();
	}

	AActor* BestStart = ChoosePlayerStart(Player);
	if (BestStart == NULL)
	{
		// no player start found
		UE_LOG(LogGameMode, Log, TEXT("Warning - PATHS NOT DEFINED or NO PLAYERSTART with positive rating"));

		// Search all loaded levels for possible player start object
		for ( int32 LevelIndex = 0; LevelIndex < World->GetNumLevels(); ++LevelIndex )
		{
			ULevel* Level = World->GetLevel( LevelIndex );
			for ( int32 ActorIndex = 0; ActorIndex < Level->Actors.Num(); ++ActorIndex )
			{
				AActor* NavObject = Cast<AActor>(Level->Actors[ActorIndex]);
				if ( NavObject )
				{
					BestStart = NavObject;
					break;
				}
			}

			if (BestStart != NULL)
			{
				break;
			}
		}
	}

	return BestStart;
}

AActor* AGameMode::K2_FindPlayerStart( AController* Player )
{
	return FindPlayerStart(Player);
}

void AGameMode::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Instigator;
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save game states or network managers into a map

	// Fallback to default GameState if none was specified.
	if (GameStateClass == NULL)
	{
		UE_LOG(LogGameMode, Warning, TEXT("No GameStateClass was specified in %s (%s)"), *GetName(), *GetClass()->GetName());
		GameStateClass = AGameState::StaticClass();
	}

	GameState = GetWorld()->SpawnActor<AGameState>(GameStateClass, SpawnInfo);
	GetWorld()->GameState = GameState;
	if (GameState)
	{
		GameState->AuthorityGameMode = this;
	}

	// Only need NetworkManager for servers in net games
	GetWorld()->NetworkManager = GetWorldSettings()->GameNetworkManagerClass ? GetWorld()->SpawnActor<AGameNetworkManager>(GetWorldSettings()->GameNetworkManagerClass, SpawnInfo ) : NULL;

	InitGameState();
}

void AGameMode::RestartPlayer(AController* NewPlayer)
{
	if ( NewPlayer == NULL || NewPlayer->IsPendingKillPending() )
	{
		return;
	}

	UE_LOG(LogGameMode, Verbose, TEXT("RestartPlayer %s"), (NewPlayer && NewPlayer->PlayerState) ? *NewPlayer->PlayerState->PlayerName : TEXT("Unknown"));

	if (NewPlayer->PlayerState && NewPlayer->PlayerState->bOnlySpectator)
	{
		UE_LOG(LogGameMode, Verbose, TEXT("RestartPlayer tried to restart a spectator-only player!"));
		return;
	}

	AActor* StartSpot = FindPlayerStart(NewPlayer);

	// if a start spot wasn't found,
	if (StartSpot == NULL)
	{
		// check for a previously assigned spot
		if (NewPlayer->StartSpot != NULL)
		{
			StartSpot = NewPlayer->StartSpot.Get();
			UE_LOG(LogGameMode, Warning, TEXT("Player start not found, using last start spot"));
		}
		else
		{
			// otherwise abort
			UE_LOG(LogGameMode, Warning, TEXT("Player start not found, failed to restart player"));
			return;
		}
	}
	// try to create a pawn to use of the default class for this player
	if (NewPlayer->GetPawn() == NULL && GetDefaultPawnClassForController(NewPlayer) != NULL)
	{
		NewPlayer->SetPawn(SpawnDefaultPawnFor(NewPlayer, StartSpot));
	}

	if (NewPlayer->GetPawn() == NULL)
	{
		NewPlayer->FailedToSpawnPawn();
	}
	else
	{
		// initialize and start it up
		InitStartSpot(StartSpot, NewPlayer);

		// @todo: this was related to speedhack code, which is disabled.
		/*
		if ( NewPlayer->GetAPlayerController() )
		{
			NewPlayer->GetAPlayerController()->TimeMargin = -0.1f;
		}
		*/
		NewPlayer->Possess(NewPlayer->GetPawn());

		// If the Pawn is destroyed as part of possession we have to abort
		if (NewPlayer->GetPawn() == nullptr)
		{
			NewPlayer->FailedToSpawnPawn();
		}
		else
		{
			// set initial control rotation to player start's rotation
			NewPlayer->ClientSetRotation(NewPlayer->GetPawn()->GetActorRotation(), true);

			FRotator NewControllerRot = StartSpot->GetActorRotation();
			NewControllerRot.Roll = 0.f;
			NewPlayer->SetControlRotation( NewControllerRot );

			SetPlayerDefaults(NewPlayer->GetPawn());

			K2_OnRestartPlayer(NewPlayer);
		}
	}

#if !UE_WITH_PHYSICS
	if (NewPlayer->GetPawn() != NULL)
	{
		UCharacterMovementComponent* CharacterMovement = Cast<UCharacterMovementComponent>(NewPlayer->GetPawn()->GetMovementComponent());
		if (CharacterMovement)
		{
			CharacterMovement->bCheatFlying = true;
			CharacterMovement->SetMovementMode(MOVE_Flying);
		}
	}
#endif	//!UE_WITH_PHYSICS
}

void AGameMode::InitStartSpot_Implementation(AActor* StartSpot, AController* NewPlayer)
{
}

void AGameMode::StartPlay()
{
	if (MatchState == MatchState::EnteringMap)
	{
		SetMatchState(MatchState::WaitingToStart);
	}

	// Check to see if we should immediately transfer to match start
	if (MatchState == MatchState::WaitingToStart && ReadyToStartMatch())
	{
		StartMatch();
	}
}

void AGameMode::HandleMatchIsWaitingToStart()
{
	if (GameSession != NULL)
	{
		GameSession->HandleMatchIsWaitingToStart();
	}

	// Calls begin play on actors, unless we're about to transition to match start

	if (!ReadyToStartMatch())
	{
		GetWorldSettings()->NotifyBeginPlay();
	}
}

bool AGameMode::ReadyToStartMatch_Implementation()
{
	// If bDelayed Start is set, wait for a manual match start
	if (bDelayedStart)
	{
		return false;
	}

	// By default start when we have > 0 players
	if (GetMatchState() == MatchState::WaitingToStart)
	{
		if (NumPlayers + NumBots > 0)
		{
			return true;
		}
	}
	return false;
}

void AGameMode::StartMatch()
{
	if (HasMatchStarted())
	{
		// Already started
		return;
	}

	//Let the game session override the StartMatch function, in case it wants to wait for arbitration
	if (GameSession->HandleStartMatchRequest())
	{
		return;
	}

	SetMatchState(MatchState::InProgress);
}

void AGameMode::HandleMatchHasStarted()
{
	GameSession->HandleMatchHasStarted();

	// start human players first
	for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = *Iterator;
		if ((PlayerController->GetPawn() == NULL) && PlayerCanRestart(PlayerController))
		{
			RestartPlayer(PlayerController);
		}
	}

	// Make sure level streaming is up to date before triggering NotifyMatchStarted
	GEngine->BlockTillLevelStreamingCompleted(GetWorld());

	// First fire BeginPlay, if we haven't already in waiting to start match
	GetWorldSettings()->NotifyBeginPlay();

	// Then fire off match started
	GetWorldSettings()->NotifyMatchStarted();

	// if passed in bug info, send player to right location
	FString BugLocString = ParseOption(OptionsString, TEXT("BugLoc"));
	FString BugRotString = ParseOption(OptionsString, TEXT("BugRot"));
	if( !BugLocString.IsEmpty() || !BugRotString.IsEmpty() )
	{
		for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
		{
			APlayerController* PlayerController = *Iterator;
			if( PlayerController->CheatManager != NULL )
			{
				//`log( "BugLocString:" @ BugLocString );
				//`log( "BugRotString:" @ BugRotString );

				PlayerController->CheatManager->BugItGoString( BugLocString, BugRotString );
			}
		}
	}

	if (IsHandlingReplays() && GetGameInstance() != nullptr)
	{
		GetGameInstance()->StartRecordingReplay(GetWorld()->GetMapName(), GetWorld()->GetMapName());
	}
}

bool AGameMode::ReadyToEndMatch_Implementation()
{
	// By default don't explicitly end match
	return false;
}

void AGameMode::EndMatch()
{
	if (!IsMatchInProgress())
	{
		return;
	}

	SetMatchState(MatchState::WaitingPostMatch);
}

void AGameMode::HandleMatchHasEnded()
{
	GameSession->HandleMatchHasEnded();

	if (IsHandlingReplays() && GetGameInstance() != nullptr)
	{
		GetGameInstance()->StopRecordingReplay();
	}
}

void AGameMode::StartToLeaveMap()
{
	SetMatchState(MatchState::LeavingMap);
}

void AGameMode::HandleLeavingMap()
{

}

void AGameMode::AbortMatch()
{
	SetMatchState(MatchState::Aborted);
}

void AGameMode::HandleMatchAborted()
{

}

bool AGameMode::HasMatchStarted() const
{
	if (GetMatchState() == MatchState::EnteringMap || GetMatchState() == MatchState::WaitingToStart)
	{
		return false;
	}

	return true;
}

bool AGameMode::IsMatchInProgress() const
{
	if (GetMatchState() == MatchState::InProgress)
	{
		return true;
	}

	return false;
}

bool AGameMode::HasMatchEnded() const
{
	if (GetMatchState() == MatchState::WaitingPostMatch || GetMatchState() == MatchState::LeavingMap)
	{
		return true;
	}

	return false;
}

void AGameMode::SetMatchState(FName NewState)
{
	if (MatchState == NewState)
	{
		return;
	}

	MatchState = NewState;

	// Call change callbacks

	if (MatchState == MatchState::WaitingToStart)
	{
		HandleMatchIsWaitingToStart();
	}
	else if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::WaitingPostMatch)
	{
		HandleMatchHasEnded();
	}
	else if (MatchState == MatchState::LeavingMap)
	{
		HandleLeavingMap();
	}
	else if (MatchState == MatchState::Aborted)
	{
		HandleMatchAborted();
	}

	if (GameState)
	{
		GameState->SetMatchState(NewState);
	}

	K2_OnSetMatchState(NewState);
}

void AGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetMatchState() == MatchState::WaitingToStart)
	{
		// Check to see if we should start the match
		if (ReadyToStartMatch())
		{
			StartMatch();
		}
	}
	if (GetMatchState() == MatchState::InProgress)
	{
		// Check to see if we should start the match
		if (ReadyToEndMatch())
		{
			EndMatch();
		}
	}
}

void AGameMode::ResetLevel()
{
	UE_LOG(LogGameMode, Log, TEXT("Reset %s"), *GetName());

	// Reset ALL controllers first
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		APlayerController* PlayerController = Cast<APlayerController>(Controller);
		if ( PlayerController )
		{
			PlayerController->ClientReset();
		}
		Controller->Reset();
	}

	// Reset all actors (except controllers, the GameMode, and any other actors specified by ShouldReset())
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if(	A && !A->IsPendingKill() && A != this && !A->IsA<AController>() && ShouldReset(A))
		{
			A->Reset();
		}
	}

	// reset the GameMode
	Reset();

	// Notify the level script that the level has been reset
	ALevelScriptActor* LevelScript = GetWorld()->GetLevelScriptActor();
	if( LevelScript )
	{
		LevelScript->LevelReset();
	}
}


void AGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	UE_LOG(LogGameMode, Log, TEXT(">> GameMode::HandleSeamlessTravelPlayer: %s "), *C->GetName());

	APlayerController* PC = Cast<APlayerController>(C);
	if (PC != NULL && PC->GetClass() != PlayerControllerClass)
	{
		if (PC->Player != NULL)
		{
			// we need to spawn a new PlayerController to replace the old one
			APlayerController* NewPC = SpawnPlayerController(PC->IsLocalPlayerController() ? ROLE_SimulatedProxy : ROLE_AutonomousProxy, PC->GetFocalLocation(), PC->GetControlRotation());
			if (NewPC == NULL)
			{
				UE_LOG(LogGameMode, Warning, TEXT("Failed to spawn new PlayerController for %s (old class %s)"), *PC->GetHumanReadableName(), *PC->GetClass()->GetName());
				PC->Destroy();
				return;
			}
			else
			{
				PC->CleanUpAudioComponents();
				PC->SeamlessTravelTo(NewPC);
				NewPC->SeamlessTravelFrom(PC);
				SwapPlayerControllers(PC, NewPC);
				PC = NewPC;
				C = NewPC;
			}
		}
		else
		{
			PC->Destroy();
		}
	}
	else
	{
		// clear out data that was only for the previous game
		C->PlayerState->Reset();
		// create a new PlayerState and copy over info; this is necessary because the old GameMode may have used a different PlayerState class
		APlayerState* OldPlayerState = C->PlayerState;
		C->InitPlayerState();
		OldPlayerState->SeamlessTravelTo(C->PlayerState);
		// we don"t need the old PlayerState anymore
		//@fixme: need a way to replace PlayerStates that doesn't cause incorrect "player left the game"/"player entered the game" messages
		OldPlayerState->Destroy();
	}

	// Find a start spot
	AActor* StartSpot = FindPlayerStart(C);

	if (StartSpot == NULL)
	{
		UE_LOG(LogGameMode, Warning, TEXT("Could not find a starting spot"));
	}
	else
	{
		FRotator StartRotation(0, StartSpot->GetActorRotation().Yaw, 0);
		C->SetInitialLocationAndRotation(StartSpot->GetActorLocation(), StartRotation);
	}

	C->StartSpot = StartSpot;

	if (PC != NULL)
	{
		// Track the last completed seamless travel for the player
		PC->LastCompletedSeamlessTravelCount = PC->SeamlessTravelCount;

		PC->CleanUpAudioComponents();

		if (PC->PlayerCameraManager == NULL)
		{
			PC->SpawnPlayerCameraManager();
		}

		SetSeamlessTravelViewTarget(PC);
		if (PC->PlayerState->bOnlySpectator)
		{
			PC->StartSpectatingOnly();

			// NumSpectators is incremented in PostSeamlessTravel
		}
		else
		{
			NumPlayers++;
			NumTravellingPlayers--;
			PC->bPlayerIsWaiting = true;
			PC->ChangeState(NAME_Spectating);
			PC->ClientGotoState(NAME_Spectating);
		}
	}
	else
	{
		NumBots++;
	}

	if (PC)
	{
		// This handles setting hud, and may spawn the player pawn if the game is in progress
		StartNewPlayer(PC);
	}

	GenericPlayerInitialization(C);

	UE_LOG(LogGameMode, Log, TEXT("<< GameMode::HandleSeamlessTravelPlayer: %s"), *C->GetName());
}

void AGameMode::SetSeamlessTravelViewTarget(APlayerController* PC)
{
	PC->SetViewTarget(PC);
}


void AGameMode::ProcessServerTravel(const FString& URL, bool bAbsolute)
{
#if WITH_SERVER_CODE
	StartToLeaveMap();

	// force an old style load screen if the server has been up for a long time so that TimeSeconds doesn't overflow and break everything
	bool bSeamless = (bUseSeamlessTravel && GetWorld()->TimeSeconds < 172800.0f); // 172800 seconds == 48 hours

	FString NextMap;
	if (URL.ToUpper().Contains(TEXT("?RESTART")))
	{
		NextMap = UWorld::RemovePIEPrefix(GetOutermost()->GetName());
	}
	else
	{
		int32 OptionStart = URL.Find(TEXT("?"));
		if (OptionStart == INDEX_NONE)
		{
			NextMap = URL;
		}
		else
		{
			NextMap = URL.Left(OptionStart);
		}
	}
	
	// Handle short package names (convenience code so that short map names
	// can still be specified in the console).
	if (FPackageName::IsShortPackageName(NextMap))
	{
		UE_LOG(LogGameMode, Fatal, TEXT("ProcessServerTravel: %s: Short package names are not supported."), *URL);
		return;
	}
	
	FGuid NextMapGuid = UEngine::GetPackageGuid(FName(*NextMap), GetWorld()->IsPlayInEditor());

	// Notify clients we're switching level and give them time to receive.
	FString URLMod = URL;
	APlayerController* LocalPlayer = ProcessClientTravel(URLMod, NextMapGuid, bSeamless, bAbsolute);

	UE_LOG(LogGameMode, Log, TEXT("ProcessServerTravel: %s"), *URL);
	UWorld* World = GetWorld(); 
	check(World);
	World->NextURL = URL;
	ENetMode NetMode = GetNetMode();

	if (bSeamless)
	{
		World->SeamlessTravel(World->NextURL, bAbsolute);
		World->NextURL = TEXT("");
	}
	// Switch immediately if not networking.
	else if (NetMode != NM_DedicatedServer && NetMode != NM_ListenServer)
	{
		World->NextSwitchCountdown = 0.0f;
	}
#endif // WITH_SERVER_CODE
}


void AGameMode::GetSeamlessTravelActorList(bool bToEntry, TArray<AActor*>& ActorList)
{
	UWorld* World = GetWorld();
	// always keep PlayerStates, so that after we restart we can keep players on the same team, etc
	for (int32 i = 0; i < World->GameState->PlayerArray.Num(); i++)
	{
		ActorList.Add(World->GameState->PlayerArray[i]);
	}

	if (bToEntry)
	{
		// keep general game state until we transition to the final destination
		ActorList.Add(World->GameState);
		// keep the game session state until we transition to the final destination
		ActorList.Add(GameSession);
	}
}

void AGameMode::SetBandwidthLimit(float AsyncIOBandwidthLimit)
{
	GAsyncIOBandwidthLimit = AsyncIOBandwidthLimit;
}

FString AGameMode::InitNewPlayer(APlayerController* NewPlayerController, const TSharedPtr<FUniqueNetId>& UniqueId, const FString& Options, const FString& Portal)
{
	check(NewPlayerController);

	FString ErrorMessage;

	// Register the player with the session
	GameSession->RegisterPlayer(NewPlayerController, UniqueId, HasOption(Options, TEXT("bIsFromInvite")));

	// Init player's name
	FString InName = ParseOption(Options, TEXT("Name")).Left(20);
	if (InName.IsEmpty())
	{
		InName = FString::Printf(TEXT("%s%i"), *DefaultPlayerName.ToString(), NewPlayerController->PlayerState->PlayerId);
	}

	ChangeName(NewPlayerController, InName, false);

	// Find a starting spot
	AActor* const StartSpot = FindPlayerStart(NewPlayerController, Portal);
	if (StartSpot != NULL)
	{
		// Set the player controller / camera in this new location
		FRotator InitialControllerRot = StartSpot->GetActorRotation();
		InitialControllerRot.Roll = 0.f;
		NewPlayerController->SetInitialLocationAndRotation(StartSpot->GetActorLocation(), InitialControllerRot);
		NewPlayerController->StartSpot = StartSpot;
	}
	else
	{
		ErrorMessage = FString::Printf(TEXT("Failed to find PlayerStart"));
	}

	return ErrorMessage;
}

bool AGameMode::MustSpectate_Implementation(APlayerController* NewPlayerController) const
{
	return NewPlayerController->PlayerState->bOnlySpectator;
}

APlayerController* AGameMode::Login(UPlayer* NewPlayer, ENetRole RemoteRole, const FString& Portal, const FString& Options, const TSharedPtr<FUniqueNetId>& UniqueId, FString& ErrorMessage)
{
	ErrorMessage = GameSession->ApproveLogin(Options);
	if (!ErrorMessage.IsEmpty())
	{
		return NULL;
	}

	APlayerController* NewPlayerController = SpawnPlayerController(RemoteRole, FVector::ZeroVector, FRotator::ZeroRotator);

	// Handle spawn failure.
	if (NewPlayerController == NULL)
	{
		UE_LOG(LogGameMode, Log, TEXT("Couldn't spawn player controller of class %s"), PlayerControllerClass ? *PlayerControllerClass->GetName() : TEXT("NULL"));
		ErrorMessage = FString::Printf(TEXT("Failed to spawn player controller"));
		return NULL;
	}

	// Customize incoming player based on URL options
	ErrorMessage = InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!ErrorMessage.IsEmpty())
	{
		return NULL;
	}

	// Set up spectating
	bool bSpectator = FCString::Stricmp(*ParseOption(Options, TEXT("SpectatorOnly")), TEXT("1")) == 0;
	if (bSpectator || MustSpectate(NewPlayerController))
	{
		NewPlayerController->StartSpectatingOnly();
		return NewPlayerController;
	}

	return NewPlayerController;
}


void AGameMode::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Canvas->SetDrawColor(255,255,255);
}


void AGameMode::Reset()
{
	Super::Reset();
	InitGameState();
}


bool AGameMode::ShouldReset_Implementation(AActor* ActorToReset)
{
	return true;
}

void AGameMode::PlayerSwitchedToSpectatorOnly(APlayerController* PC)
{
	RemovePlayerControllerFromPlayerCount(PC);
	NumSpectators++;
}

void AGameMode::RemovePlayerControllerFromPlayerCount(APlayerController* PC)
{
	if ( PC )
	{
		if ( PC->PlayerState->bOnlySpectator )
		{
			NumSpectators--;
		}
		else
		{
			if ( GetWorld()->IsInSeamlessTravel() || PC->HasClientLoadedCurrentWorld() )
			{
				NumPlayers--;
			}
			else
			{
				NumTravellingPlayers--;
			}
		}
	}
}

int32 AGameMode::GetNumPlayers()
{
	return NumPlayers + NumTravellingPlayers;
}



void AGameMode::ClearPause()
{
	if ( !AllowPausing() && Pausers.Num() > 0 )
	{
		UE_LOG(LogGameMode, Log, TEXT("Clearing list of UnPause delegates for %s because game type is not pauseable"), *GetFName().ToString());
		Pausers.Empty();
	}

	for (int32 Index = 0; Index < Pausers.Num(); Index++)
	{
		FCanUnpause CanUnpauseCriteriaMet = Pausers[Index];
		if( CanUnpauseCriteriaMet.IsBound() )
		{
			bool Result = CanUnpauseCriteriaMet.Execute();
			if (Result)
			{
				Pausers.RemoveAt(Index--,1);
			}
		}
		else
		{
			Pausers.RemoveAt(Index--,1);
		}
	}

	// Clear the pause state if the list is empty
	if ( Pausers.Num() == 0 )
	{
		GetWorldSettings()->Pauser = NULL;
	}
}

bool AGameMode::GrabOption( FString& Options, FString& Result )
{
	if( Options.Left(1)==TEXT("?") )
	{
		// Get result.
		Result = Options.Mid(1, MAX_int32);
		if (Result.Contains(TEXT("?"), ESearchCase::CaseSensitive))
		{
			Result = Result.Left(Result.Find(TEXT("?"), ESearchCase::CaseSensitive));
		}

		// Update options.
		Options = Options.Mid(1, MAX_int32);
		if (Options.Contains(TEXT("?"), ESearchCase::CaseSensitive))
		{
			Options = Options.Mid(Options.Find(TEXT("?"), ESearchCase::CaseSensitive), MAX_int32);
		}
		else
		{
			Options = TEXT("");
		}

		return true;
	}
	else return false;
}

void AGameMode::GetKeyValue( const FString& Pair, FString& Key, FString& Value )
{
	const int32 EqualSignIndex = Pair.Find(TEXT("="), ESearchCase::CaseSensitive);
	if( EqualSignIndex != INDEX_NONE )
	{
		Key = Pair.Left(EqualSignIndex);
		Value = Pair.Mid(EqualSignIndex + 1, MAX_int32);
	}
	else
	{
		Key = Pair;
		Value = TEXT("");
	}
}

FString AGameMode::ParseOption( const FString& Options, const FString& InKey )
{
	FString OptionsMod = Options;
	FString Pair, Key, Value;
	while( GrabOption( OptionsMod, Pair ) )
	{
		GetKeyValue( Pair, Key, Value );
		if( FCString::Stricmp(*Key, *InKey) == 0 )
			return Value;
	}
	return TEXT("");
}

bool AGameMode::HasOption( const FString& Options, const FString& InKey )
{
	FString OptionsMod = Options;
	FString Pair, Key, Value;
	while( GrabOption( OptionsMod, Pair ) )
	{
		GetKeyValue( Pair, Key, Value );
		if( FCString::Stricmp(*Key, *InKey) == 0 )
		{
			return true;
		}
	}
	return false;
}

int32 AGameMode::GetIntOption( const FString& Options, const FString& ParseString, int32 CurrentValue)
{
	FString InOpt = ParseOption( Options, ParseString );
	if ( !InOpt.IsEmpty() )
	{
		return FCString::Atoi(*InOpt);
	}
	return CurrentValue;
}

FString AGameMode::GetDefaultGameClassPath(const FString& MapName, const FString& Options, const FString& Portal) const
{
	return GetClass()->GetPathName();
}

TSubclassOf<AGameMode> AGameMode::GetGameModeClass(const FString& MapName, const FString& Options, const FString& Portal) const
{
	return GetClass();
}

TSubclassOf<AGameMode> AGameMode::SetGameMode(const FString& MapName, const FString& Options, const FString& Portal)
{
	return GetGameModeClass(MapName, Options, Portal);
}

TSubclassOf<AGameSession> AGameMode::GetGameSessionClass() const
{
	return AGameSession::StaticClass();
}

void AGameMode::NotifyPendingConnectionLost() {}

APlayerController* AGameMode::ProcessClientTravel( FString& FURL, FGuid NextMapGuid, bool bSeamless, bool bAbsolute )
{
	// We call PreClientTravel directly on any local PlayerPawns (ie listen server)
	APlayerController* LocalPlayerController = NULL;
	for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = *Iterator;
		if ( Cast<UNetConnection>(PlayerController->Player) != NULL )
		{
			// remote player
			PlayerController->ClientTravel(FURL, TRAVEL_Relative, bSeamless, NextMapGuid);
		}
		else
		{
			// local player
			LocalPlayerController = PlayerController;
			PlayerController->PreClientTravel(FURL, bAbsolute ? TRAVEL_Absolute : TRAVEL_Relative, bSeamless);
		}
	}

	return LocalPlayerController;
}

void AGameMode::PreLogin(const FString& Options, const FString& Address, const TSharedPtr<FUniqueNetId>& UniqueId, FString& ErrorMessage)
{
	ErrorMessage = GameSession->ApproveLogin(Options);
}

APlayerController* AGameMode::SpawnPlayerController(ENetRole RemoteRole, FVector const& SpawnLocation, FRotator const& SpawnRotation)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Instigator;	
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save player controllers into a map
	SpawnInfo.bDeferConstruction = true;
	APlayerController* NewPC = GetWorld()->SpawnActor<APlayerController>(PlayerControllerClass, SpawnLocation, SpawnRotation, SpawnInfo);
	if (NewPC)
	{
		if (RemoteRole == ROLE_SimulatedProxy)
		{
			// This is a local player because it has no authority/autonomous remote role
			NewPC->SetAsLocalPlayerController();
		}
		
		UGameplayStatics::FinishSpawningActor(NewPC, FTransform(SpawnRotation, SpawnLocation));
	}

	return NewPC;
}

UClass* AGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	return DefaultPawnClass;
}

APawn* AGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// don't allow pawn to be spawned with any pitch or roll
	FRotator StartRotation(ForceInit);
	StartRotation.Yaw = StartSpot->GetActorRotation().Yaw;
	FVector StartLocation = StartSpot->GetActorLocation();

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Instigator;	
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save default player pawns into a map
	APawn* ResultPawn = GetWorld()->SpawnActor<APawn>(GetDefaultPawnClassForController(NewPlayer), StartLocation, StartRotation, SpawnInfo );
	if ( ResultPawn == NULL )
	{
		UE_LOG(LogGameMode, Warning, TEXT("Couldn't spawn Pawn of type %s at %s"), *GetNameSafe(DefaultPawnClass), *StartSpot->GetName());
	}
	return ResultPawn;
}

void AGameMode::ReplicateStreamingStatus(APlayerController* PC)
{
	// don't do this for local players or players after the first on a splitscreen client
	if (Cast<ULocalPlayer>(PC->Player) == NULL && Cast<UChildConnection>(PC->Player) == NULL)
	{
		// if we've loaded levels via CommitMapChange() that aren't normally in the StreamingLevels array, tell the client about that
		if (GetWorld()->CommittedPersistentLevelName != NAME_None)
		{
			PC->ClientPrepareMapChange(GetWorld()->CommittedPersistentLevelName, true, true);
			// tell the client to commit the level immediately
			PC->ClientCommitMapChange();
		}

		if (GetWorld()->StreamingLevels.Num() > 0)
		{
			// Tell the player controller the current streaming level status
			for (int32 LevelIndex = 0; LevelIndex < GetWorld()->StreamingLevels.Num(); LevelIndex++)
			{
				// streamingServer
				ULevelStreaming* TheLevel = GetWorld()->StreamingLevels[LevelIndex];

				if( TheLevel != NULL )
				{
					const ULevel* LoadedLevel = TheLevel->GetLoadedLevel();
					
					UE_LOG(LogGameMode, Log, TEXT("levelStatus: %s %i %i %i %s %i"),
						*TheLevel->GetWorldAssetPackageName(),
						TheLevel->bShouldBeVisible,
						LoadedLevel && LoadedLevel->bIsVisible,
						TheLevel->bShouldBeLoaded,
						*GetNameSafe(LoadedLevel),
						TheLevel->bHasLoadRequestPending);

					PC->ClientUpdateLevelStreamingStatus(
						TheLevel->GetWorldAssetPackageFName(),
						TheLevel->bShouldBeLoaded,
						TheLevel->bShouldBeVisible,
						TheLevel->bShouldBlockOnLoad,
						TheLevel->LevelLODIndex);
				}
			}
			PC->ClientFlushLevelStreaming();
		}

		// if we're preparing to load different levels using PrepareMapChange() inform the client about that now
		if (GetWorld()->PreparingLevelNames.Num() > 0)
		{
			for (int32 LevelIndex = 0; LevelIndex < GetWorld()->PreparingLevelNames.Num(); LevelIndex++)
			{
				PC->ClientPrepareMapChange(GetWorld()->PreparingLevelNames[LevelIndex], LevelIndex == 0, LevelIndex == GetWorld()->PreparingLevelNames.Num() - 1);
			}
			// DO NOT commit these changes yet - we'll send that when we're done preparing them
		}
	}
}

void AGameMode::GenericPlayerInitialization(AController* C)
{
	APlayerController* PC = Cast<APlayerController>(C);
	if (PC != NULL)
	{
		// Notify the game that we can now be muted and mute others
		UpdateGameplayMuteList(PC);

		ReplicateStreamingStatus(PC);
	}
}

void AGameMode::StartNewPlayer(APlayerController* NewPlayer)
{
	// tell client what hud class to use
	NewPlayer->ClientSetHUD(HUDClass);

	// If players should start as spectators, leave them in the spectator state
	if (!bStartPlayersAsSpectators && !NewPlayer->PlayerState->bOnlySpectator)
	{
		// If match is in progress, start the player
		if (IsMatchInProgress())
		{
			RestartPlayer(NewPlayer);

			if (NewPlayer->GetPawn() != NULL)
			{
				NewPlayer->GetPawn()->ClientSetRotation(NewPlayer->GetPawn()->GetActorRotation());
			}
		}
		// Check to see if we should start right away, avoids a one frame lag in single player games
		else if (GetMatchState() == MatchState::WaitingToStart)
		{
			// Check to see if we should start the match
			if (ReadyToStartMatch())
			{
				StartMatch();
			}
		}
	}
}

bool AGameMode::CanSpectate_Implementation( APlayerController* Viewer, APlayerState* ViewTarget )
{
	return true;
}

void AGameMode::ChangeName( AController* Other, const FString& S, bool bNameChange )
{
	if( !S.IsEmpty() )
	{
		Other->PlayerState->SetPlayerName(S);
	}
}

void AGameMode::SendPlayer( APlayerController* aPlayer, const FString& FURL )
{
	aPlayer->ClientTravel( FURL, TRAVEL_Relative );
}

bool AGameMode::GetTravelType()
{
	return false;
}

void AGameMode::Broadcast( AActor* Sender, const FString& Msg, FName Type )
{
	APlayerState* SenderPlayerState = NULL;
	if ( Cast<APawn>(Sender) != NULL )
	{
		SenderPlayerState = Cast<APawn>(Sender)->PlayerState;
	}
	else if ( Cast<AController>(Sender) != NULL )
	{
		SenderPlayerState = Cast<AController>(Sender)->PlayerState;
	}

	for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		(*Iterator)->ClientTeamMessage( SenderPlayerState, Msg, Type );
	}
}


void AGameMode::BroadcastLocalized( AActor* Sender, TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject )
{
	for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		(*Iterator)->ClientReceiveLocalizedMessage( Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject );
	}
}

bool AGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	return ( Player != NULL && Player->StartSpot != NULL );
}

AActor* AGameMode::ChoosePlayerStart_Implementation( AController* Player )
{
	// Choose a player start
	APlayerStart* FoundPlayerStart = NULL;
	UClass* PawnClass = GetDefaultPawnClassForController(Player);
	APawn* PawnToFit = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;
	TArray<APlayerStart*> UnOccupiedStartPoints;
	TArray<APlayerStart*> OccupiedStartPoints;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* PlayerStart = *It;

		if (PlayerStart->IsA<APlayerStartPIE>())
		{
			// Always prefer the first "Play from Here" PlayerStart, if we find one while in PIE mode
			FoundPlayerStart = PlayerStart;
			break;
		}
		else
		{
			FVector ActorLocation = PlayerStart->GetActorLocation();
			const FRotator ActorRotation = PlayerStart->GetActorRotation();
			if (!GetWorld()->EncroachingBlockingGeometry(PawnToFit, ActorLocation, ActorRotation))
			{
				UnOccupiedStartPoints.Add(PlayerStart);
			}
			else if (GetWorld()->FindTeleportSpot(PawnToFit, ActorLocation, ActorRotation))
			{
				OccupiedStartPoints.Add(PlayerStart);
			}
		}
	}
	if (FoundPlayerStart == NULL)
	{
		if (UnOccupiedStartPoints.Num() > 0)
		{
			FoundPlayerStart = UnOccupiedStartPoints[FMath::RandRange(0, UnOccupiedStartPoints.Num() - 1)];
		}
		else if (OccupiedStartPoints.Num() > 0)
		{
			FoundPlayerStart = OccupiedStartPoints[FMath::RandRange(0, OccupiedStartPoints.Num() - 1)];
		}
	}
	return FoundPlayerStart;
}

bool AGameMode::PlayerCanRestart_Implementation( APlayerController* Player )
{
	if (!IsMatchInProgress() || Player == NULL || Player->IsPendingKillPending())
	{
		return false;
	}

	// Ask the player controller if it's ready to restart as well
	return Player->CanRestartPlayer();
}

void AGameMode::UpdateGameplayMuteList( APlayerController* aPlayer )
{
	if (aPlayer)
	{
		aPlayer->MuteList.bHasVoiceHandshakeCompleted = true;
		aPlayer->ClientVoiceHandshakeComplete();
	}
}

bool AGameMode::AllowCheats(APlayerController* P)
{
	return ( GetNetMode() == NM_Standalone || GIsEditor ); // Always allow cheats in editor (PIE now supports networking)
}

bool AGameMode::AllowPausing( APlayerController* PC )
{
	return bPauseable || GetNetMode() == NM_Standalone;
}

void AGameMode::PreCommitMapChange(const FString& PreviousMapName, const FString& NextMapName) {}

void AGameMode::PostCommitMapChange() {}

void AGameMode::AddInactivePlayer(APlayerState* PlayerState, APlayerController* PC)
{
	// don't store if it's an old PlayerState from the previous level or if it's a spectator
	if (!PlayerState->bFromPreviousLevel && !PlayerState->bOnlySpectator)
	{
		APlayerState* NewPlayerState = PlayerState->Duplicate();
		if (NewPlayerState)
		{
			GetWorld()->GameState->RemovePlayerState(NewPlayerState);

			// make PlayerState inactive
			NewPlayerState->SetReplicates(false);

			// delete after some time
			NewPlayerState->SetLifeSpan(InactivePlayerStateLifeSpan);

			// On console, we have to check the unique net id as network address isn't valid
			bool bIsConsole = GEngine->IsConsoleBuild();

			// make sure no duplicates
			for (int32 i=0; i<InactivePlayerArray.Num(); i++)
			{
				APlayerState* CurrentPlayerState = InactivePlayerArray[i];
				if ( (CurrentPlayerState == NULL) || CurrentPlayerState->IsPendingKill() ||
					(!bIsConsole && (CurrentPlayerState->SavedNetworkAddress == NewPlayerState->SavedNetworkAddress)))
				{
					InactivePlayerArray.RemoveAt(i,1);
					i--;
				}
			}
			InactivePlayerArray.Add(NewPlayerState);

			// cap at 16 saved PlayerStates
			if ( InactivePlayerArray.Num() > 16 )
			{
				InactivePlayerArray.RemoveAt(0, InactivePlayerArray.Num() - 16);
			}
		}
	}

	PlayerState->Destroy();
}


bool AGameMode::FindInactivePlayer(APlayerController* PC)
{
	// don't bother for spectators
	if (PC->PlayerState->bOnlySpectator)
	{
		return false;
	}

	// On console, we have to check the unique net id as network address isn't valid
	bool bIsConsole = GEngine->IsConsoleBuild();

	FString NewNetworkAddress = PC->PlayerState->SavedNetworkAddress;
	FString NewName = PC->PlayerState->PlayerName;
	for (int32 i=0; i<InactivePlayerArray.Num(); i++)
	{
		APlayerState* CurrentPlayerState = InactivePlayerArray[i];
		if ( (CurrentPlayerState == NULL) || CurrentPlayerState->IsPendingKill() )
		{
			InactivePlayerArray.RemoveAt(i,1);
			i--;
		}
		else if ( (bIsConsole && ( CurrentPlayerState->UniqueId == PC->PlayerState->UniqueId ) ) ||
			(!bIsConsole && (FCString::Stricmp(*CurrentPlayerState->SavedNetworkAddress, *NewNetworkAddress) == 0) && (FCString::Stricmp(*CurrentPlayerState->PlayerName, *NewName) == 0)) )
		{
			// found it!
			APlayerState* OldPlayerState = PC->PlayerState;
			PC->PlayerState = CurrentPlayerState;
			PC->PlayerState->SetOwner(PC);
			PC->PlayerState->SetReplicates(true);
			PC->PlayerState->SetLifeSpan( 0.0f );
			OverridePlayerState(PC, OldPlayerState);
			GetWorld()->GameState->AddPlayerState(PC->PlayerState);
			InactivePlayerArray.RemoveAt(i,1);
			OldPlayerState->bIsInactive = true;
			// Set the uniqueId to NULL so it will not kill the player's registration 
			// in UnregisterPlayerWithSession()
			OldPlayerState->SetUniqueId(NULL);
			OldPlayerState->Destroy();
			return true;
		}
	}
	return false;
}

void AGameMode::OverridePlayerState(APlayerController* PC, APlayerState* OldPlayerState)
{
	PC->PlayerState->OverrideWith(OldPlayerState);
}

void AGameMode::PostSeamlessTravel()
{
	if ( GameSession != NULL )
	{
		GameSession->PostSeamlessTravel();
	}

	// We have to make a copy of the controller list, since the code after this will destroy
	// and create new controllers in the world's list
	TArray<TAutoWeakObjectPtr<class AController> >	OldControllerList;
	for (auto It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		OldControllerList.Add(*It);
	}

	// handle players that are already loaded
	for( FConstControllerIterator Iterator = OldControllerList.CreateConstIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState)
		{
			APlayerController* PlayerController = Cast<APlayerController>(Controller);
			if (PlayerController == NULL)
			{
				HandleSeamlessTravelPlayer(Controller);
			}
			else
			{
				if (Controller->PlayerState->bOnlySpectator)
				{
					// The spectator count must be incremented here, instead of in HandleSeamlessTravelPlayer,
					// as otherwise spectators can 'hide' from player counters, by making HasClientLoadedCurrentWorld return false
					NumSpectators++;
				}
				else
				{
					NumTravellingPlayers++;
				}
				if (PlayerController->HasClientLoadedCurrentWorld())
				{
					HandleSeamlessTravelPlayer(Controller);
				}
			}
		}
	}
}

void AGameMode::MatineeCancelled()
{
}

FString AGameMode::StaticGetFullGameClassName(FString const& Str)
{
	// look to see if this should be remapped from a shortname to full class name
	const AGameMode* const DefaultGameMode = GetDefault<AGameMode>();
	if (DefaultGameMode)
	{
		int32 const NumAliases = DefaultGameMode->GameModeClassAliases.Num();
		for (int32 Idx=0; Idx<NumAliases; ++Idx)
		{
			const FGameClassShortName& Alias = DefaultGameMode->GameModeClassAliases[Idx];
			if ( Str == Alias.ShortName )
			{
				// switch GameClassName to the full name
				return Alias.GameClassName;
			}
		}
	}

	return Str;
}

FString AGameMode::GetRedirectURL(const FString& MapName) const
{
	return FString();
}

bool AGameMode::IsHandlingReplays()
{
	// If we're running in PIE, don't record demos
	if (GetWorld() != nullptr && GetWorld()->IsPlayInEditor())
	{
		return false;
	}

	return bHandleDedicatedServerReplays && GetNetMode() == ENetMode::NM_DedicatedServer;
}
