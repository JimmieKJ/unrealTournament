// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "UTGameSessionNonRanked.h"

void AUTGameSessionNonRanked::CleanUpOnlineSubsystem()
{
	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		OnlineSub->ClearOnConnectionStatusChangedDelegate_Handle(OnConnectionStatusDelegate);

		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);
			SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegate);
			SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(OnEndSessionCompleteDelegate);
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegate);
			SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(OnUpdateSessionCompleteDelegate);
		}
	}

	Super::CleanUpOnlineSubsystem();
}

void AUTGameSessionNonRanked::InitHostBeacon(FOnlineSessionSettings* SessionSettings)
{
	UWorld* const World = GetWorld();

	// If the beacon host already exists, just exit this loop.
	if (BeaconHost)
	{
		UE_LOG(UT, Verbose, TEXT("InitHostBeacon called with an existing beacon."));
		return;
	}

	// Always create a new beacon host
	BeaconHostListener = World->SpawnActor<AOnlineBeaconHost>(AOnlineBeaconHost::StaticClass());
	if (BeaconHostListener == nullptr)
	{
		UE_LOG(UT, Warning, TEXT("Attempt to spawn the BeaconHostListener Failed!  This will cause your online session to not work."));
		return;
	}

	BeaconHost = World->SpawnActor<AUTServerBeaconHost>(AUTServerBeaconHost::StaticClass());
	if (BeaconHost == nullptr)
	{
		UE_LOG(UT, Warning, TEXT("Attempt to spawn the BeaconHost Failed!  This will cause your online session to not work."));
		return;
	}

	// Initialize beacon state, either new or from a seamless travel
	bool bBeaconInit = false;
	if (BeaconHostListener && BeaconHostListener->InitHost())
	{
		BeaconHostListener->RegisterHost(BeaconHost);
		UE_LOG(UT, Log, TEXT("---------------------------------------"));
		UE_LOG(UT, Log, TEXT("Server Active Port (ignore ip): %s"), *GetNetDriver()->LowLevelGetNetworkNumber());
		UE_LOG(UT, Log, TEXT("Server Beacon Port: %i"), BeaconHostListener->GetListenPort());
		UE_LOG(UT, Log, TEXT("---------------------------------------"));

		int32 DesiredPort = AOnlineBeaconHost::StaticClass()->GetDefaultObject<AOnlineBeaconHost>()->ListenPort;
		int32 PortOverride;
		if (FParse::Value(FCommandLine::Get(), TEXT("BeaconPort="), PortOverride) && PortOverride != 0)
		{
			DesiredPort = PortOverride;
		}

		if (UTBaseGameMode && !UTBaseGameMode->IsGameInstanceServer() && BeaconHostListener->GetListenPort() != DesiredPort)
		{
			if (CantBindBeaconPortIsNotFatal)
			{
				UE_LOG(UT, Warning, TEXT("Could not bind to expected beacon port.  Your server will be unresponsive. [%i vs %i]"), BeaconHostListener->GetListenPort(), DesiredPort);
			}
			else
			{
				UE_LOG(UT, Warning, TEXT("Could not bind to expected beacon port.  Restarting server!!! [%i vs %i]"), BeaconHostListener->GetListenPort(), DesiredPort);
				FPlatformMisc::RequestExit(true);
			}
		}

		BeaconHostListener->PauseBeaconRequests(false);
	}

	// Update the beacon port
	if (SessionSettings)
	{
		SessionSettings->Set(SETTING_BEACONPORT, BeaconHostListener->GetListenPort(), EOnlineDataAdvertisementType::ViaOnlineService);
	}
}

void AUTGameSessionNonRanked::DestroyHostBeacon(bool bPreserveReservations)
{
	if (BeaconHost)
	{
		BeaconHostListener->UnregisterHost(BeaconHost->GetBeaconType());
		BeaconHost->Destroy();
		BeaconHost = NULL;

		BeaconHostListener->DestroyBeacon();
		BeaconHostListener = NULL;
	}
}

void AUTGameSessionNonRanked::RegisterServer()
{
	UE_LOG(UT, Verbose, TEXT("--------------[REGISTER SERVER]----------------"));

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			EOnlineSessionState::Type State = SessionInterface->GetSessionState(GameSessionName);
			if (State == EOnlineSessionState::NoSession)
			{
				bSessionValid = false;
				if (!OnConnectionStatusDelegate.IsValid())
				{
					OnConnectionStatusDelegate = OnlineSub->AddOnConnectionStatusChangedDelegate_Handle(FOnConnectionStatusChangedDelegate::CreateUObject(this, &ThisClass::OnConnectionStatusChanged));
				}
				if (!OnCreateSessionCompleteDelegate.IsValid())
				{
					OnCreateSessionCompleteDelegate = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete));
				}

				bool bLanGame = FParse::Param(FCommandLine::Get(), TEXT("lan"));
				TSharedPtr<class FUTOnlineGameSettingsBase> OnlineGameSettings = MakeShareable(new FUTOnlineGameSettingsBase(bLanGame, false, false, 10000));

				if (OnlineGameSettings.IsValid() && UTBaseGameMode)
				{
					InitHostBeacon(OnlineGameSettings.Get());
					OnlineGameSettings->ApplyGameSettings(OnlineGameSettings.Get(), UTBaseGameMode, this);
					SessionInterface->CreateSession(0, GameSessionName, *OnlineGameSettings);
					return;
				}

			}
			else if (State == EOnlineSessionState::Ended)
			{
				bSessionValid = false;
				UE_LOG(UT, Warning, TEXT("New Map has detected a session that has ended.  This shouldn't happen and is probably bad.  Try to restart it!"));
				StartMatch();
			}
			else
			{
				// We have a valid session.
				TSharedPtr<class FUTOnlineGameSettingsBase> OnlineGameSettings = MakeShareable(new FUTOnlineGameSettingsBase(false, false, false, 10000));
				if (OnlineGameSettings.IsValid() && UTBaseGameMode)
				{
					InitHostBeacon(OnlineGameSettings.Get());
					OnlineGameSettings->ApplyGameSettings(OnlineGameSettings.Get(), UTBaseGameMode, this);
				}

				bSessionValid = true;
				UE_LOG(UT, Verbose, TEXT("Server is already registered and kicking.  No to go any furter."));
			}
		}
		else
		{
			UE_LOG(UT, Verbose, TEXT("Server does not have a valid session interface so it will not be visible."));
		}
	}

	Super::RegisterServer();
}

void AUTGameSessionNonRanked::UnRegisterServer(bool bShuttingDown)
{
	if (bShuttingDown)
	{
		UE_LOG(UT, Verbose, TEXT("--------------[UN-REGISTER SERVER]----------------"));

		const auto OnlineSub = IOnlineSubsystem::Get();
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		EOnlineSessionState::Type State = SessionInterface->GetSessionState(GameSessionName);

		if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
		{
			DestroyHostBeacon();
			if (bShuttingDown)
			{
				if (SessionInterface.IsValid())
				{
					OnDestroySessionCompleteDelegate = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete));
					SessionInterface->DestroySession(GameSessionName);
				}
			}
		}
	}
	Super::UnRegisterServer(bShuttingDown);
}

void AUTGameSessionNonRanked::ValidatePlayer(const FString& Address, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& ErrorMessage, bool bValidateAsSpectator)
{
	if (UniqueId.IsValid())
	{
		FString UIDAsString = UniqueId->ToString();
		for (int32 i = 0; i < BannedUsers.Num(); i++)
		{
			if (UIDAsString == BannedUsers[i].UniqueID)
			{
				ErrorMessage = TEXT("BANNED");
				break;
			}
		}

		for (int32 i = 0; i < InstanceBannedUsers.Num(); i++)
		{
			if (UIDAsString == InstanceBannedUsers[i].UniqueID)
			{
				ErrorMessage = TEXT("BANNED");
				break;
			}
		}
	}

	Super::ValidatePlayer(Address, UniqueId, ErrorMessage, bValidateAsSpectator);
}

void AUTGameSessionNonRanked::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// If we were not successful, then clear the online game settings member and move on
	if (bWasSuccessful)
	{
		UE_LOG(UT, Verbose, TEXT("Session %s Created!"), *SessionName.ToString());
		UpdateSessionJoinability(GameSessionName, true, true, true, false);

		// Immediately start the online session
		StartMatch();
	}
	else
	{
		UE_LOG(UT, Log, TEXT("Failed to Create the session '%s' so this match will not be visible.  We will attempt to restart the session in %i minutes.  See the logs!"), *SessionName.ToString(), int32(SERVER_REREGISTER_WAIT_TIME / 60.0));

		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTGameSession::RegisterServer, SERVER_REREGISTER_WAIT_TIME);
	}

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);
			AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
			if (GameState)
			{
				FNamedOnlineSession* Session = SessionInterface->GetNamedSession(GameSessionName);
				if (Session && Session->SessionInfo.IsValid())
				{
					GameState->ServerSessionId = Session->SessionInfo->GetSessionId().ToString();
				}
			}
		}
	}

}

void AUTGameSessionNonRanked::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		UE_LOG(UT, Log, TEXT("Failed to start the session '%s' so this match will not be visible.  See the logs!"), *SessionName.ToString());
	}
	else
	{
		UE_LOG(UT, Verbose, TEXT("Session %s Started!"), *SessionName.ToString());
		bSessionValid = true;

		// Our session has started, if we are a lobby instance, tell the lobby to go.  NOTE: We don't use the cached version of UTGameMode here
		AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (GM)
		{
			GM->NotifyLobbyGameIsReady();
		}
	}

	// Immediately perform an update so as to pickup any players that have joined since.
	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegate);
			UpdateGameState();
		}
	}
}

void AUTGameSessionNonRanked::OnEndSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		UE_LOG(UT, Log, TEXT("Failed to end the session '%s' so match stats will not save.  See the logs!"), *SessionName.ToString());
		IOnlineSubsystem::Get()->GetSessionInterface()->DumpSessionState();
	}
	else
	{
		UE_LOG(UT, Verbose, TEXT("OnEndSessionComplete %s"), *SessionName.ToString());
	}

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(OnEndSessionCompleteDelegate);
		}
	}
}

void AUTGameSessionNonRanked::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		UE_LOG(UT, Log, TEXT("Failed to destroy the session '%s'!  Matchmaking will be broken until restart.  See the logs!"), *SessionName.ToString());
	}
	else
	{
		UE_LOG(UT, Verbose, TEXT("OnDestroySessionComplete %s"), *SessionName.ToString());
	}

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegate);
		}
	}

	if (bReregisterWhenDone)
	{
		bReregisterWhenDone = false;
		RegisterServer();
	}

}

void AUTGameSessionNonRanked::UpdateGameState()
{
	const auto OnlineSub = IOnlineSubsystem::Get();
	if (UTBaseGameMode && OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			if (!OnUpdateSessionCompleteDelegate.IsValid())
			{
				OnUpdateSessionCompleteDelegate = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnUpdateSessionComplete));
			}

			EOnlineSessionState::Type State = SessionInterface->GetSessionState(GameSessionName);
			if (State != EOnlineSessionState::Creating && State != EOnlineSessionState::Ended && State != EOnlineSessionState::Ending && State != EOnlineSessionState::Destroying && State != EOnlineSessionState::NoSession)
			{
				FUTOnlineGameSettingsBase* OnlineGameSettings = (FUTOnlineGameSettingsBase*)SessionInterface->GetSessionSettings(GameSessionName);
				OnlineGameSettings->ApplyGameSettings(OnlineGameSettings, UTBaseGameMode, this);
				SessionInterface->UpdateSession(SessionName, *OnlineGameSettings, true);
			}
		}
	}

	Super::UpdateGameState();
}

void AUTGameSessionNonRanked::OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// workaround for the online service sometimes losing sessions
	// if we lost our session and have no players, restart to try to acquire a new session
	if (!bWasSuccessful && UTBaseGameMode != NULL && UTBaseGameMode->NumPlayers == 0 && UTBaseGameMode->GetNumMatches() <= 1 && !UTBaseGameMode->IsGameInstanceServer())
	{
		static double LastMCPHackTime = FPlatformTime::Seconds();
		if (FPlatformTime::Seconds() - LastMCPHackTime > 600.0)
		{
			GetWorld()->ServerTravel(GetWorld()->GetOutermost()->GetName());
		}
	}
}

bool AUTGameSessionNonRanked::BanPlayer(class APlayerController* BannedPlayer, const FText& BanReason)
{
	APlayerState* PS = BannedPlayer->PlayerState;
	if (PS)
	{
		UE_LOG(UT, Log, TEXT("Adding Ban for user '%s' (uid: %s) Reason '%s'"), *PS->PlayerName, *PS->UniqueId.ToString(), *BanReason.ToString());
		BannedUsers.Add(FBanInfo(PS->PlayerName, PS->UniqueId.ToString()));
		SaveConfig();

		// Send off some analytics so we can track # of bans, etc.
		if (FUTAnalytics::IsAvailable())
		{
			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("UserName"), PS->PlayerName));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("UniqueId"), PS->UniqueId.ToString()));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("Reason"), BanReason.ToString()));

			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (GS)
			{
				ParamArray.Add(FAnalyticsEventAttribute(TEXT("ServerName"), GS->ServerName));
			}

			FUTAnalytics::GetProvider().RecordEvent(TEXT("AdminBan"), ParamArray);
		}
	}

	return Super::BanPlayer(BannedPlayer, BanReason);
}

bool AUTGameSessionNonRanked::KickPlayer(APlayerController* KickedPlayer, const FText& KickReason)
{
	// Do not kick logged admins
	if (KickedPlayer != NULL && Cast<UNetConnection>(KickedPlayer->Player) != NULL)
	{
		APlayerState* PS = KickedPlayer->PlayerState;
		if (PS)
		{
			if (UTBaseGameMode && UTBaseGameMode->IsGameInstanceServer())
			{
				InstanceBannedUsers.Add(FBanInfo(PS->PlayerName, PS->UniqueId.ToString()));
			}
		}
		KickedPlayer->ClientWasKicked(KickReason);
		KickedPlayer->Destroy();
		return true;
	}
	else
	{
		return false;
	}
}

void AUTGameSessionNonRanked::StartMatch()
{
	UE_LOG(UT, Log, TEXT("--------------[MCP START MATCH] ----------------"));
	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			OnStartSessionCompleteDelegate = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete));
			SessionInterface->StartSession(GameSessionName);
		}
	}

	Super::StartMatch();
}

void AUTGameSessionNonRanked::EndMatch()
{
	UE_LOG(UT, Log, TEXT("--------------[MCP END MATCH] ----------------"));
	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				APlayerController* PlayerController = *Iterator;
				if (!PlayerController->IsLocalController())
				{
					PlayerController->ClientEndOnlineSession();
				}
			}

			SessionInterface->AddOnEndSessionCompleteDelegate_Handle(FOnEndSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnEndSessionComplete));
			SessionInterface->EndSession(SessionName);
		}
	}

	Super::EndMatch();
}

void AUTGameSessionNonRanked::OnConnectionStatusChanged(EOnlineServerConnectionStatus::Type LastConnectionState, EOnlineServerConnectionStatus::Type ConnectionState)
{
	if (ConnectionState == EOnlineServerConnectionStatus::InvalidSession)
	{
		UE_LOG(UT, Warning, TEXT("The Server has been notifed that it's session is no longer valid.  Attempted to recreate a valid session."));

		// We have tried to talk to the MCP but our session is invalid.  So we want to reregister
		DestroyHostBeacon();
		bReregisterWhenDone = true;
		UnRegisterServer(true);
	}
}