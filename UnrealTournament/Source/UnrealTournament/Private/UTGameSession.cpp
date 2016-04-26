// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameSession.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTBaseGameMode.h"
#include "UTLobbyGameMode.h"
#include "UTAnalytics.h"
#include "UTGameInstance.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

/** True if this server was instructed to gracefully shutdown at the next convenience */
bool AUTGameSession::bGracefulShutdown = false;

/** The exit code to exit with when gracefully shutting down */
int32 AUTGameSession::GracefulExitCode = 0;

static void GracefulShutdown_Execute(const TArray<FString>& Args, UWorld* World)
{
	AUTGameSession::bGracefulShutdown = true;

	if (Args.Num() > 0)
	{
		AUTGameSession::GracefulExitCode = TCString<TCHAR>::Atoi(*Args[0]);
	}

	AGameMode* GameMode = World->GetAuthGameMode();
	if (GameMode)
	{
		AUTGameSession* GameSession = Cast<AUTGameSession>(GameMode->GameSession);
		if (GameSession)
		{
			GameSession->GracefulShutdown(AUTGameSession::GracefulExitCode);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs UTGracefulShutdown_ExeuteCmd(
	TEXT("GracefulShutdown"),
	TEXT("Gracefully shuts down the server with the given exit code the next time the server is available to do so"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(GracefulShutdown_Execute)
	);

AUTGameSession::AUTGameSession(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bSessionValid = false;
	bNoJoinInProgress = false;
	CantBindBeaconPortIsNotFatal = false;
}

void AUTGameSession::Destroyed()
{
	Super::Destroyed();
	CleanUpOnlineSubsystem();
}

void AUTGameSession::CleanUpOnlineSubsystem()
{
	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnSessionFailureDelegate_Handle(OnSessionFailuredDelegate);
		}
	}
}

void AUTGameSession::InitOptions( const FString& Options )
{
	bReregisterWhenDone = false;
	Super::InitOptions(Options);

	// Register a connection status change delegate so we can look for failures on the backend.

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			OnSessionFailuredDelegate = SessionInterface->AddOnSessionFailureDelegate_Handle(FOnSessionFailureDelegate::CreateUObject(this, &AUTGameSession::SessionFailure));
		}
	}

	// Cache the GameMode for later.
	UTBaseGameMode = Cast<AUTBaseGameMode>(GetWorld()->GetAuthGameMode());
	bNoJoinInProgress = UGameplayStatics::HasOption(Options, "NoJIP");
}

void AUTGameSession::ValidatePlayer(const FString& Address, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& ErrorMessage, bool bValidateAsSpectator)
{
	if ( !bValidateAsSpectator && (UniqueId.IsValid() && AllowedAdmins.Find(UniqueId->ToString()) == INDEX_NONE) && bNoJoinInProgress && UTBaseGameMode->HasMatchStarted() )
	{
		ErrorMessage = TEXT("CANTJOININPROGRESS");
		return;
	}
	
	if (!UniqueId.IsValid() && Address != TEXT("127.0.0.1") && !FParse::Param(FCommandLine::Get(), TEXT("AllowEveryone")) && Cast<AUTLobbyGameMode>(UTBaseGameMode) == NULL)
	{
		ErrorMessage = TEXT("NOTLOGGEDIN");
	}

	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		if (GameState->IsTempBanned(UniqueId))
		{
			UE_LOG(UT,Log,TEXT("Banned"));
			ErrorMessage = TEXT("BANNED");
			return;
		}
	
	}
}


FString AUTGameSession::ApproveLogin(const FString& Options)
{
	if (UTBaseGameMode)
	{
		if (!UGameplayStatics::HasOption(Options, TEXT("VersionCheck")) && (GetNetMode() != NM_Standalone) && !GetWorld()->IsPlayInEditor())
		{
			UE_LOG(UT, Warning, TEXT("********************************YOU MUST UPDATE TO A NEW VERSION %s"), *Options);
			return TEXT("You must update to a the latest version.  For more information, go to forums.unrealtournament.com");
		}
		// force allow split casting views
		// warning: relies on check in Login() that this is really a split view
		if (UGameplayStatics::HasOption(Options, TEXT("CastingView")))
		{
			return TEXT("");
		}

		bool bSpectator = FCString::Stricmp(*UGameplayStatics::ParseOption(Options, TEXT("SpectatorOnly")), TEXT("1")) == 0;

		// If this is a rank locked server, don't allow players in
		AUTGameMode* UTGameMode = Cast<AUTGameMode>(UTBaseGameMode);
		if (UTGameMode)
		{
			if (UTGameMode->bRankLocked && !bSpectator)
			{
				int32 IncomingRank = UGameplayStatics::GetIntOption(Options, TEXT("RankCheck"), 0);

				if (IncomingRank > UTGameMode->RankCheck)
				{
					return TEXT("TOOSTRONG");
				}
			}
		}

		if (GetNetMode() != NM_Standalone && !GetWorld()->IsPlayInEditor())
		{
			FString Password = UGameplayStatics::ParseOption(Options, TEXT("Password"));
			if (!bSpectator && !UTBaseGameMode->ServerPassword.IsEmpty())
			{
				UE_LOG(UT,Log,TEXT("Pass: %s v %s"), Password.IsEmpty() ? TEXT("") : *Password, *UTBaseGameMode->ServerPassword);
				if (Password.IsEmpty() || !UTBaseGameMode->ServerPassword.Equals(Password, ESearchCase::CaseSensitive))
				{
					return TEXT("NEEDPASS");
				}
			}
			FString SpecPassword = UGameplayStatics::ParseOption(Options, TEXT("SpecPassword"));
			if (bSpectator && !UTBaseGameMode->SpectatePassword.IsEmpty())
			{
				UE_LOG(UT,Log,TEXT("Spec: %s v %s"), SpecPassword.IsEmpty() ? TEXT("") : *SpecPassword, *UTBaseGameMode->SpectatePassword);
				if (SpecPassword.IsEmpty() || !UTBaseGameMode->SpectatePassword.Equals(SpecPassword, ESearchCase::CaseSensitive))
				{
					return TEXT("NEEDSPECPASS");
				}
			}
		}
	}
	return Super::ApproveLogin(Options);
}

bool AUTGameSession::ProcessAutoLogin()
{
	// UT Dedicated servers do not need to login.  
	if (GetWorld()->GetNetMode() == NM_DedicatedServer) 
	{
		// NOTE: Returning true here will effectively bypass the RegisterServer call in the base GameMode.  
		// UT servers will register elsewhere.
		return true;
	}
	return Super::ProcessAutoLogin();
}

// We want different behavior than the default engine implementation.  Our matches remain across level switches so don't allow the main game to mess with them.

void AUTGameSession::HandleMatchHasStarted() {}
void AUTGameSession::HandleMatchHasEnded() {}

void AUTGameSession::AcknowledgeAdmin(const FString& AdminId, bool bIsAdmin)
{
	if (bIsAdmin)
	{
		// Make sure he's not already in there.
		if (AllowedAdmins.Find(AdminId) == INDEX_NONE)
		{
			AllowedAdmins.Add(AdminId);
		}
	}
	else
	{
		if (AllowedAdmins.Find(AdminId) != INDEX_NONE)
		{
			AllowedAdmins.Remove(AdminId);
		}
	}
}

void AUTGameSession::SessionFailure(const FUniqueNetId& PlayerId, ESessionFailure::Type ErrorType)
{
	// Currently, SessionFailure never seems to be called.  Need to discuss it with Josh when he returns as it would
	// be better to handle this hear then in ConnectionStatusChanged
}

void AUTGameSession::CleanupServerSession()
{
	if (IsRunningDedicatedServer() == false)
	{
		return;
	}

	UUTGameInstance* const GameInstance = CastChecked<UUTGameInstance>(GetGameInstance());
	GameInstance->SafeSessionDelete(GameSessionName, FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete));
}

void AUTGameSession::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName == GameSessionName)
	{
		UE_LOG(LogOnlineGame, Log, TEXT("[AGameSessionCommon::OnDestroySessionComplete] Previous session destroyed. SessionName: %s, bWasSuccessful: %d"), *SessionName.ToString(), bWasSuccessful);

		if (!bWasSuccessful)
		{
			// It is ok if this fails, we will just create another session anyways
			UE_LOG(LogOnlineGame, Warning, TEXT("[AGameSessionCommon::OnDestroySessionComplete] Failed to destroy previous game session. SessionName: %s"), *SessionName.ToString());
		}

		// Wait for any other processes to finish/cleanup before we start advertising
		GetWorldTimerManager().SetTimer(StartServerTimerHandle, this, &ThisClass::StartServer, 0.1f);
	}
}

bool AUTGameSession::ShouldStopServer()
{
	if (ServerSecondsToLive > 0)
	{
		double AbsoluteServerSeconds = FApp::GetCurrentTime() - GStartTime;
		if (AbsoluteServerSeconds >= ServerSecondsToLive)
		{
			UE_LOG(LogOnlineGame, Log, TEXT("[AGameSessionCommon::ShouldStopServer] Server time limit hit (lives for %f seconds, configured to live %f seconds)"),
				AbsoluteServerSeconds, ServerSecondsToLive);
			return true;
		}
	}

	return false;
}

void AUTGameSession::CheckForPossibleRestart()
{
	UWorld* const World = GetWorld();
	AGameMode* const GameMode = World->GetAuthGameMode();
	check(GameMode);

	UE_LOG(LogOnlineGame, Log, TEXT("[AUTGameSession::CheckForPossibleRestart] Checking for possible restart..."));
	
	/*
	if (IsDevelopment())
	{
		UE_LOG(LogOnlineGame, Log, TEXT("[AUTGameSession::CheckForPossibleRestart] Ignoring restart for development build"));
		return;
	}
	*/

	// check if an unattended match is in progress
	if (FParse::Param(FCommandLine::Get(), TEXT("Unattended")))
	{
		UE_LOG(LogOnlineGame, Log, TEXT("[AUTGameSession::CheckForPossibleRestart] Ignoring restart due to -unattended switch, assuming performance test."));
		return;
	}

	// If there are no players, but there is a reservation, check back later
	// reservations will clear out and it will be caught in a future call
	if (GameMode->GetNumPlayers() == 0)
	{
		UE_LOG(LogOnlineGame, Log, TEXT("[AUTGameSession::CheckForPossibleRestart] Game is empty with no reservations, restarting"));
		Restart();
	}
}

void AUTGameSession::Restart()
{
	//LockPlayersToSession(false);

	if (bGracefulShutdown)
	{
		ShutdownServer(TEXT("Graceful shutdown requested"), GracefulExitCode);
	}
	else
	{
		// check for shutdown/exit logic first
		if (ShouldStopServer())
		{
			UE_LOG(LogOnlineGame, Log, TEXT("[AUTGameSession::Restart] Configured server stop criteria hit, shutting down gracefully."));
			ShutdownServer(TEXT("Controlled shutdown due to stop criteria."), EXIT_CODE_GRACEFUL);
		}
		else
		{
			UE_LOG(LogOnlineGame, Log, TEXT("[AUTGameSession::Restart] Restarting dedicated server"));
			/*
			GetWorldTimerManager().ClearTimer(StartServerTimerHandle);
			StartServerTimerHandle.Invalidate();

			// Deny future beacon traffic while we restart
			//PauseReservationRequests(true);

			// Don't preserve beacon state, doing a full restart
			DestroyHostBeacon(false);

			// Server travel back to the empty map
			UUTGameInstance* const GameInstance = CastChecked<UUTGameInstance>(GetGameInstance());
			const FString& TravelURL = UGameMapsSettings::GetGameDefaultMap();
			GameInstance->ServerTravel(TravelURL, true, true);*/
		}
	}
}

void AUTGameSession::GracefulShutdown(int32 ExitCode)
{
	bGracefulShutdown = true;
	GracefulExitCode = ExitCode;

	UE_LOG(LogOnlineGame, Log, TEXT("[AUTGameSession::GracefulShutdown] Flagged for graceful shutdown with ExitCode: %i"), ExitCode);

	// Will shutdown the server right now if possible
	CheckForPossibleRestart();
}

void AUTGameSession::ShutdownServer(const FString& Reason, int32 ExitCode)
{
	if (IsRunningDedicatedServer() == false)
	{
		return;
	}

	UE_LOG(LogOnlineGame, Warning, TEXT("[AUTGameSession::ShutdownServer] Shutting down dedicated server, Reason: %s, ExitCode: %d"), *Reason, ExitCode);
	
	//UnregisterServerDelegates();
	DestroyHostBeacon(false);

	GetWorldTimerManager().ClearTimer(StartServerTimerHandle);
	StartServerTimerHandle.Invalidate();

	FPlatformMisc::RequestExitWithStatus(false, ExitCode);
}