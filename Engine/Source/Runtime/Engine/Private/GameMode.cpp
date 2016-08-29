// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GameMode.cpp: AGameMode c++ code.
=============================================================================*/

#include "EnginePrivate.h"
#include "Engine/LevelScriptActor.h"
#include "GameFramework/GameNetworkManager.h"
#include "Matinee/MatineeActor.h"
#include "Net/OnlineEngineInterface.h"
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
	
#if WITH_EDITOR
	#include "IMovieSceneCapture.h"
	#include "MovieSceneCaptureModule.h"
	#include "MovieSceneCaptureSettings.h"
#endif

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

// Statically declared events for plugins to use
FGameModeEvents::FGameModePostLoginEvent FGameModeEvents::GameModePostLoginEvent;
FGameModeEvents::FGameModeLogoutEvent FGameModeEvents::GameModeLogoutEvent;

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
	ReplaySpectatorPlayerControllerClass = APlayerController::StaticClass();
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
		NewPC->NetConnection = OldPC->NetConnection;
		NewPC->SetPlayer(Player);
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
		// Attempt to login, returning true means an async login is in flight
		if (!UOnlineEngineInterface::Get()->DoesSessionExist(World, GameSession->SessionName) &&
			!GameSession->ProcessAutoLogin())
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
	else
	{
		// If NewPlayer is not only a spectator and has a valid ID, add him as a user to the replay.
		if (NewPlayer->PlayerState->UniqueId.IsValid())
		{
			GetGameInstance()->AddUserToReplay(NewPlayer->PlayerState->UniqueId.ToString());
		}
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
	if(ShouldStartInCinematicMode(NewPlayer, HidePlayer, HideHUD, DisableMovement, DisableTurning))
	{
		NewPlayer->SetCinematicMode(true, HidePlayer, HideHUD, DisableMovement, DisableTurning);
	}

	// Tell the player to enable voice by default or use the push to talk method
	NewPlayer->ClientEnableNetworkVoice(!GameSession->RequiresPushToTalk());

	// Notify Blueprints that a new player has logged in.  Calling it here, because this is the first time that the PlayerController can take RPCs
	K2_PostLogin(NewPlayer);
	FGameModeEvents::GameModePostLoginEvent.Broadcast(this, NewPlayer);
}

bool AGameMode::ShouldStartInCinematicMode(APlayerController* Player, bool& OutHidePlayer,bool& OutHideHUD,bool& OutDisableMovement,bool& OutDisableTurning)
{
	ULocalPlayer* LocPlayer = Player->GetLocalPlayer();
	if (!LocPlayer)
	{
		return false;
	}

#if WITH_EDITOR
	// If we have an active movie scene capture, we can take the settings from that
	if(LocPlayer->ViewportClient && LocPlayer->ViewportClient->Viewport)
	{
		if(auto* MovieSceneCapture = IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture())
		{
			const FMovieSceneCaptureSettings& Settings = MovieSceneCapture->GetSettings();
			if (Settings.bCinematicMode)
			{
				OutDisableMovement = !Settings.bAllowMovement;
				OutDisableTurning = !Settings.bAllowTurning;
				OutHidePlayer = !Settings.bShowPlayer;
				OutHideHUD = !Settings.bShowHUD;
				return true;
			}
		}
	}
#endif

	return false;
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
		FGameModeEvents::GameModeLogoutEvent.Broadcast(this, Exiting);
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
		const FName IncomingPlayerStartTag = FName(*IncomingName);
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			APlayerStart* Start = *It;
			if (Start && Start->PlayerStartTag == IncomingPlayerStartTag)
			{
				return Start;
			}
		}
	}

	// always pick StartSpot at start of match
	if ( ShouldSpawnAtStartSpot(Player) )
	{
		if (AActor* PlayerStartSpot = Player->StartSpot.Get())
		{
			return PlayerStartSpot;
		}
		else
		{
			UE_LOG(LogGameMode, Error, TEXT("Error - ShouldSpawnAtStartSpot returned true but the Player StartSpot was null."));
		}
	}

	AActor* BestStart = ChoosePlayerStart(Player);
	if (BestStart == nullptr)
	{
		// no player start found
		UE_LOG(LogGameMode, Log, TEXT("Warning - PATHS NOT DEFINED or NO PLAYERSTART with positive rating"));

		// This is a bit odd, but there was a complex chunk of code that in the end always resulted in this, so we may as well just 
		// short cut it down to this.  Basically we are saying spawn at 0,0,0 if we didn't find a proper player start
		BestStart = World->GetWorldSettings();
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
	const FString BugLocString = UGameplayStatics::ParseOption(OptionsString, TEXT("BugLoc"));
	const FString BugRotString = UGameplayStatics::ParseOption(OptionsString, TEXT("BugRot"));
	if( !BugLocString.IsEmpty() || !BugRotString.IsEmpty() )
	{
		for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
		{
			APlayerController* PlayerController = *Iterator;
			if( PlayerController->CheatManager != NULL )
			{
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

	UE_LOG(LogGameMode, Display, TEXT("Match State Changed from %s to %s"), *MatchState.ToString(), *NewState.ToString());

	MatchState = NewState;

	OnMatchStateSet();

	if (GameState)
	{
		GameState->SetMatchState(NewState);
	}

	K2_OnSetMatchState(NewState);
}

void AGameMode::OnMatchStateSet()
{
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
}

void AGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetMatchState() == MatchState::WaitingToStart)
	{
		// Check to see if we should start the match
		if (ReadyToStartMatch())
		{
			UE_LOG(LogGameMode, Log, TEXT("GameMode returned ReadyToStartMatch"));
			StartMatch();
		}
	}
	if (GetMatchState() == MatchState::InProgress)
	{
		// Check to see if we should start the match
		if (ReadyToEndMatch())
		{
			UE_LOG(LogGameMode, Log, TEXT("GameMode returned ReadyToEndMatch"));
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


void AGameMode::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	UWorld* World = GetWorld();

	// Get allocations for the elements we're going to add handled in one go
	const int32 ActorsToAddCount = World->GameState->PlayerArray.Num() + (bToTransition ?  3 : 0);
	ActorList.Reserve(ActorsToAddCount);

	// always keep PlayerStates, so that after we restart we can keep players on the same team, etc
	ActorList.Append(World->GameState->PlayerArray);

	if (bToTransition)
	{
		// keep ourselves until we transition to the final destination
		ActorList.Add(this);
		// keep general game state until we transition to the final destination
		ActorList.Add(World->GameState);
		// keep the game session state until we transition to the final destination
		ActorList.Add(GameSession);

		// If adding in this section best to increase the literal above for the ActorsToAddCount
	}
}

void AGameMode::SetBandwidthLimit(float AsyncIOBandwidthLimit)
{
	GAsyncIOBandwidthLimit = AsyncIOBandwidthLimit;
}

FString AGameMode::InitNewPlayer(APlayerController* NewPlayerController, const TSharedPtr<const FUniqueNetId>& UniqueId, const FString& Options, const FString& Portal)
{
	FUniqueNetIdRepl UniqueIdRepl(UniqueId);
	return InitNewPlayer(NewPlayerController, UniqueIdRepl, Options, Portal);
}

FString AGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	check(NewPlayerController);

	FString ErrorMessage;

	// Register the player with the session
	GameSession->RegisterPlayer(NewPlayerController, UniqueId.GetUniqueNetId(), UGameplayStatics::HasOption(Options, TEXT("bIsFromInvite")));

	// Init player's name
	FString InName = UGameplayStatics::ParseOption(Options, TEXT("Name")).Left(20);
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

APlayerController* AGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& ErrorMessage)
{
	FUniqueNetIdRepl UniqueIdRepl(UniqueId);
	return Login(NewPlayer, InRemoteRole, Portal, Options, UniqueIdRepl, ErrorMessage);
}

APlayerController* AGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	ErrorMessage = GameSession->ApproveLogin(Options);
	if (!ErrorMessage.IsEmpty())
	{
		return nullptr;
	}

	APlayerController* NewPlayerController = SpawnPlayerController(InRemoteRole, FVector::ZeroVector, FRotator::ZeroRotator);

	// Handle spawn failure.
	if (NewPlayerController == nullptr)
	{
		UE_LOG(LogGameMode, Log, TEXT("Couldn't spawn player controller of class %s"), PlayerControllerClass ? *PlayerControllerClass->GetName() : TEXT("NULL"));
		ErrorMessage = FString::Printf(TEXT("Failed to spawn player controller"));
		return nullptr;
	}

	// Customize incoming player based on URL options
	ErrorMessage = InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!ErrorMessage.IsEmpty())
	{
		return nullptr;
	}

	// Set up spectating
	bool bSpectator = FCString::Stricmp(*UGameplayStatics::ParseOption(Options, TEXT("SpectatorOnly")), TEXT("1")) == 0;
	if (bSpectator || MustSpectate(NewPlayerController))
	{
		NewPlayerController->StartSpectatingOnly();
		return NewPlayerController;
	}

	return NewPlayerController;
}


void AGameMode::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
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
	return UGameplayStatics::GrabOption(Options, Result);
}

void AGameMode::GetKeyValue( const FString& Pair, FString& Key, FString& Value )
{
	return UGameplayStatics::GetKeyValue(Pair, Key, Value);
}

FString AGameMode::ParseOption( FString Options, const FString& InKey )
{
	return UGameplayStatics::ParseOption(Options, InKey);
}

bool AGameMode::HasOption( FString Options, const FString& InKey )
{
	return UGameplayStatics::HasOption(Options, InKey);
}

int32 AGameMode::GetIntOption( const FString& Options, const FString& ParseString, int32 CurrentValue)
{
	return UGameplayStatics::GetIntOption(Options, ParseString, CurrentValue);
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

void AGameMode::PreLogin(const FString& Options, const FString& Address, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& ErrorMessage)
{
	FUniqueNetIdRepl UniqueIdRepl(UniqueId);
	PreLogin(Options, Address, UniqueIdRepl, ErrorMessage);
}

void AGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	ErrorMessage = GameSession->ApproveLogin(Options);
}

APlayerController* AGameMode::SpawnPlayerController(ENetRole InRemoteRole, FVector const& SpawnLocation, FRotator const& SpawnRotation)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Instigator;	
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save player controllers into a map
	SpawnInfo.bDeferConstruction = true;
	APlayerController* NewPC = GetWorld()->SpawnActor<APlayerController>(PlayerControllerClass, SpawnLocation, SpawnRotation, SpawnInfo);
	if (NewPC)
	{
		if (InRemoteRole == ROLE_SimulatedProxy)
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
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	APawn* ResultPawn = GetWorld()->SpawnActor<APawn>(PawnClass, StartLocation, StartRotation, SpawnInfo );
	if ( ResultPawn == NULL )
	{
		UE_LOG(LogGameMode, Warning, TEXT("Couldn't spawn Pawn of type %s at %s"), *GetNameSafe(PawnClass), *StartSpot->GetName());
	}
	return ResultPawn;
}

void AGameMode::ReplicateStreamingStatus(APlayerController* PC)
{
	UWorld* MyWorld = GetWorld();

	if (MyWorld->GetWorldSettings()->bUseClientSideLevelStreamingVolumes)
	{
		// Client will itself decide what to stream
		return;
	}

	// don't do this for local players or players after the first on a splitscreen client
	if (Cast<ULocalPlayer>(PC->Player) == nullptr && Cast<UChildConnection>(PC->Player) == nullptr)
	{
		// if we've loaded levels via CommitMapChange() that aren't normally in the StreamingLevels array, tell the client about that
		if (MyWorld->CommittedPersistentLevelName != NAME_None)
		{
			PC->ClientPrepareMapChange(MyWorld->CommittedPersistentLevelName, true, true);
			// tell the client to commit the level immediately
			PC->ClientCommitMapChange();
		}

		if (MyWorld->StreamingLevels.Num() > 0)
		{
			// Tell the player controller the current streaming level status
			for (int32 LevelIndex = 0; LevelIndex < MyWorld->StreamingLevels.Num(); LevelIndex++)
			{
				// streamingServer
				ULevelStreaming* TheLevel = MyWorld->StreamingLevels[LevelIndex];

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
		if (MyWorld->PreparingLevelNames.Num() > 0)
		{
			for (int32 LevelIndex = 0; LevelIndex < MyWorld->PreparingLevelNames.Num(); LevelIndex++)
			{
				PC->ClientPrepareMapChange(MyWorld->PreparingLevelNames[LevelIndex], LevelIndex == 0, LevelIndex == MyWorld->PreparingLevelNames.Num() - 1);
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
	if( Other && !S.IsEmpty() )
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
	check(PlayerState)
	UWorld* LocalWorld = GetWorld();
	// don't store if it's an old PlayerState from the previous level or if it's a spectator... or if we are shutting down
	if (!PlayerState->bFromPreviousLevel && !PlayerState->bOnlySpectator && !LocalWorld->bIsTearingDown)
	{
		APlayerState* const NewPlayerState = PlayerState->Duplicate();
		if (NewPlayerState)
		{
			// Side effect of Duplicate() adding PlayerState to PlayerArray (see APlayerState::PostInitializeComponents)
			LocalWorld->GameState->RemovePlayerState(NewPlayerState);

			// make PlayerState inactive
			NewPlayerState->SetReplicates(false);

			// delete after some time
			NewPlayerState->SetLifeSpan(InactivePlayerStateLifeSpan);

			// On console, we have to check the unique net id as network address isn't valid
			const bool bIsConsole = GEngine->IsConsoleBuild();
			// Assume valid unique ids means comparison should be via this method
			const bool bHasValidUniqueId = NewPlayerState->UniqueId.IsValid();
			// Don't accidentally compare empty network addresses (already issue with two clients on same machine during development)
			const bool bHasValidNetworkAddress = !NewPlayerState->SavedNetworkAddress.IsEmpty();
			const bool bUseUniqueIdCheck = bIsConsole || bHasValidUniqueId;
			
			// make sure no duplicates
			for (int32 Idx = 0; Idx < InactivePlayerArray.Num(); ++Idx)
			{
				APlayerState* const CurrentPlayerState = InactivePlayerArray[Idx];
				if ((CurrentPlayerState == nullptr) || CurrentPlayerState->IsPendingKill())
				{
					// already destroyed, just remove it
					InactivePlayerArray.RemoveAt(Idx, 1);
					Idx--;
				}
				else if ( (!bUseUniqueIdCheck && bHasValidNetworkAddress && (CurrentPlayerState->SavedNetworkAddress == NewPlayerState->SavedNetworkAddress))
						|| (bUseUniqueIdCheck && (CurrentPlayerState->UniqueId == NewPlayerState->UniqueId)) )
				{
					// destroy the playerstate, then remove it from the tracking
					CurrentPlayerState->Destroy();
					InactivePlayerArray.RemoveAt(Idx, 1);
					Idx--;
				}
			}
			InactivePlayerArray.Add(NewPlayerState);

			// cap at 16 saved PlayerStates
			if ( InactivePlayerArray.Num() > 16 )
			{
				int32 const NumToRemove = InactivePlayerArray.Num() - 16;

				// destroy the extra inactive players
				for (int Idx = 0; Idx < NumToRemove; ++Idx)
				{
					APlayerState* const PS = InactivePlayerArray[Idx];
					if (PS != nullptr)
					{
						PS->Destroy();
					}
				}

				// and then remove them from the tracking array
				InactivePlayerArray.RemoveAt(0, NumToRemove);
			}
		}
	}

	PlayerState->OnDeactivated();
}


bool AGameMode::FindInactivePlayer(APlayerController* PC)
{
	check(PC && PC->PlayerState);
	// don't bother for spectators
	if (PC->PlayerState->bOnlySpectator)
	{
		return false;
	}

	// On console, we have to check the unique net id as network address isn't valid
	const bool bIsConsole = GEngine->IsConsoleBuild();
	// Assume valid unique ids means comparison should be via this method
	const bool bHasValidUniqueId = PC->PlayerState->UniqueId.IsValid();
	// Don't accidentally compare empty network addresses (already issue with two clients on same machine during development)
	const bool bHasValidNetworkAddress = !PC->PlayerState->SavedNetworkAddress.IsEmpty();
	const bool bUseUniqueIdCheck = bIsConsole || bHasValidUniqueId;

	const FString NewNetworkAddress = PC->PlayerState->SavedNetworkAddress;
	const FString NewName = PC->PlayerState->PlayerName;
	for (int32 i=0; i < InactivePlayerArray.Num(); i++)
	{
		APlayerState* CurrentPlayerState = InactivePlayerArray[i];
		if ( (CurrentPlayerState == NULL) || CurrentPlayerState->IsPendingKill() )
		{
			InactivePlayerArray.RemoveAt(i,1);
			i--;
		}
		else if ((bUseUniqueIdCheck && (CurrentPlayerState->UniqueId == PC->PlayerState->UniqueId)) ||
				 (!bUseUniqueIdCheck && bHasValidNetworkAddress && (FCString::Stricmp(*CurrentPlayerState->SavedNetworkAddress, *NewNetworkAddress) == 0) && (FCString::Stricmp(*CurrentPlayerState->PlayerName, *NewName) == 0)))
		{
			// found it!
			APlayerState* OldPlayerState = PC->PlayerState;
			PC->PlayerState = CurrentPlayerState;
			PC->PlayerState->SetOwner(PC);
			PC->PlayerState->SetReplicates(true);
			PC->PlayerState->SetLifeSpan(0.0f);
			OverridePlayerState(PC, OldPlayerState);
			GetWorld()->GameState->AddPlayerState(PC->PlayerState);
			InactivePlayerArray.RemoveAt(i, 1);
			OldPlayerState->bIsInactive = true;
			// Set the uniqueId to NULL so it will not kill the player's registration 
			// in UnregisterPlayerWithSession()
			OldPlayerState->SetUniqueId(NULL);
			OldPlayerState->Destroy();
			PC->PlayerState->OnReactivated();
			return true;
		}
		
	}
	return false;
}

void AGameMode::OverridePlayerState(APlayerController* PC, APlayerState* OldPlayerState)
{
	PC->PlayerState->DispatchOverrideWith(OldPlayerState);
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
void AGameMode::GameWelcomePlayer(UNetConnection* Connection, FString& RedirectURL)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RedirectURL = GetRedirectURL(Connection->ClientWorldPackageName.ToString());
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
