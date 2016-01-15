// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameSession.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTBaseGameMode.h"
#include "UTLobbyGameMode.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

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

void AUTGameSession::InitOptions( const FString& Options )
{
	bReregisterWhenDone = false;
	Super::InitOptions(Options);

	// Register a connection status change delegate so we can look for failures on the backend.

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		OnConnectionStatusDelegate = OnlineSub->AddOnConnectionStatusChangedDelegate_Handle(FOnConnectionStatusChangedDelegate::CreateUObject(this, &AUTGameSession::OnConnectionStatusChanged));
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
	UNetDriver* NetDriver = NULL;
	if (GetWorld())
	{
		NetDriver = GetWorld()->GetNetDriver();
	}

	if ( !bValidateAsSpectator && (UniqueId.IsValid() && AllowedAdmins.Find(UniqueId->ToString()) == INDEX_NONE) && bNoJoinInProgress && UTBaseGameMode->HasMatchStarted() )
	{
		ErrorMessage = TEXT("CANTJOININPROGRESS");
		return;
	}

	FString LocalAddress = NetDriver->LowLevelGetNetworkNumber();

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
	else if (Address != TEXT("127.0.0.1") && !FParse::Param(FCommandLine::Get(), TEXT("AllowEveryone")) && Cast<AUTLobbyGameMode>(UTBaseGameMode) == NULL)
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

bool AUTGameSession::BanPlayer(class APlayerController* BannedPlayer, const FText& BanReason)
{
	APlayerState* PS = BannedPlayer->PlayerState;
	if (PS)
	{
		UE_LOG(UT,Log,TEXT("Adding Ban for user '%s' (uid: %s) Reason '%s'"), *PS->PlayerName, *PS->UniqueId.ToString(), *BanReason.ToString());
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

bool AUTGameSession::KickPlayer(APlayerController* KickedPlayer, const FText& KickReason)
{
	// Do not kick logged admins
	if (KickedPlayer != NULL && Cast<UNetConnection>(KickedPlayer->Player) != NULL)
	{
		APlayerState* PS = KickedPlayer->PlayerState;
		if (PS)
		{
			if ( UTBaseGameMode && UTBaseGameMode->IsGameInstanceServer() )
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

		// If this is a rank locked server, don't allow players in
		AUTGameMode* UTGameMode = Cast<AUTGameMode>(UTBaseGameMode);
		if (UTGameMode)
		{
			if (UTGameMode->bRankLocked)
			{
				int32 IncomingRank = UGameplayStatics::GetIntOption(Options, TEXT("Rank"), -1);
				UE_LOG(UT,Log,TEXT("Rank Check: %i vs %i = %i"), IncomingRank, UTGameMode->RankCheck, AUTLobbyMatchInfo::CheckRank(IncomingRank, UTGameMode->RankCheck) )
				if (!AUTLobbyMatchInfo::CheckRank(IncomingRank, UTGameMode->RankCheck))
				{
					return TEXT("TOOSTRONG");
				}
			}
		}

		if (GetNetMode() != NM_Standalone && !GetWorld()->IsPlayInEditor())
		{
			bool bSpectator = FCString::Stricmp(*UGameplayStatics::ParseOption(Options, TEXT("SpectatorOnly")), TEXT("1")) == 0;

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

void AUTGameSession::CleanUpOnlineSubsystem()
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
			SessionInterface->ClearOnSessionFailureDelegate_Handle(OnSessionFailuredDelegate);
		}
	}
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

void AUTGameSession::RegisterServer()
{
	UE_LOG(UT,Verbose,TEXT("--------------[REGISTER SERVER]----------------"));

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
				OnCreateSessionCompleteDelegate = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &AUTGameSession::OnCreateSessionComplete));
				bool bLanGame = FParse::Param(FCommandLine::Get(), TEXT("lan"));
				TSharedPtr<class FUTOnlineGameSettingsBase> OnlineGameSettings = MakeShareable(new FUTOnlineGameSettingsBase(bLanGame, false, 10000));

				if (OnlineGameSettings.IsValid() && UTBaseGameMode)
				{
					InitHostBeacon(OnlineGameSettings.Get());
					OnlineGameSettings->ApplyGameSettings(UTBaseGameMode);
					SessionInterface->CreateSession(0, GameSessionName, *OnlineGameSettings);
					return;
				}
			}
			else if (State == EOnlineSessionState::Ended)
			{
				bSessionValid = false;
				UE_LOG(UT,Warning, TEXT("New Map has detected a session that has ended.  This shouldn't happen and is probably bad.  Try to restart it!"));
				StartMatch();
			}
			else
			{
				// We have a valid session.
				TSharedPtr<class FUTOnlineGameSettingsBase> OnlineGameSettings = MakeShareable(new FUTOnlineGameSettingsBase(false, false, 10000));
				if (OnlineGameSettings.IsValid() && UTBaseGameMode)
				{
					InitHostBeacon(OnlineGameSettings.Get());
					OnlineGameSettings->ApplyGameSettings(UTBaseGameMode);
				}

				bSessionValid = true;
				UE_LOG(UT,Verbose,TEXT("Server is already registered and kicking.  No to go any furter."));
			}
		}
		else
		{
			UE_LOG(UT,Verbose,TEXT("Server does not have a valid session interface so it will not be visible."));
		}
	}
}

void AUTGameSession::UnRegisterServer(bool bShuttingDown)
{
	if (bShuttingDown)
	{
		UE_LOG(UT,Verbose,TEXT("--------------[UN-REGISTER SERVER]----------------"));

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
					OnDestroySessionCompleteDelegate = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &AUTGameSession::OnDestroySessionComplete));
					SessionInterface->DestroySession(GameSessionName);
				}
			}
		}
	}
}

void AUTGameSession::StartMatch()
{
	UE_LOG(UT,Log,TEXT("--------------[MCP START MATCH] ----------------"));
	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			OnStartSessionCompleteDelegate = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(FOnStartSessionCompleteDelegate::CreateUObject(this, &AUTGameSession::OnStartSessionComplete));
			SessionInterface->StartSession(GameSessionName);
		}
	}
}

void AUTGameSession::EndMatch()
{
	UE_LOG(UT,Log,TEXT("--------------[MCP END MATCH] ----------------"));
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

			SessionInterface->AddOnEndSessionCompleteDelegate_Handle(FOnEndSessionCompleteDelegate::CreateUObject(this, &AUTGameSession::OnEndSessionComplete));
			SessionInterface->EndSession(SessionName);
		}
	}
}

// We want different behavior than the default engine implementation.  Our matches remain across level switches so don't allow the main game to mess with them.

void AUTGameSession::HandleMatchHasStarted() {}
void AUTGameSession::HandleMatchHasEnded() {}

void AUTGameSession::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// If we were not successful, then clear the online game settings member and move on
	if (bWasSuccessful)
	{
		UE_LOG(UT,Verbose,TEXT("Session %s Created!"), *SessionName.ToString());
		UpdateSessionJoinability(GameSessionName, true, true, true, false);
		// Immediately start the online session
		StartMatch();
	}
	else
	{
		UE_LOG(UT,Log,TEXT("Failed to Create the session '%s' so this match will not be visible.  We will attempt to restart the session in %i minutes.  See the logs!"), *SessionName.ToString(), int32( SERVER_REREGISTER_WAIT_TIME / 60.0));
		
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

void AUTGameSession::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		UE_LOG(UT,Log,TEXT("Failed to start the session '%s' so this match will not be visible.  See the logs!"), *SessionName.ToString());
	}
	else
	{
		UE_LOG(UT,Verbose,TEXT("Session %s Started!"), *SessionName.ToString());
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

void AUTGameSession::OnEndSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		UE_LOG(UT,Log,TEXT("Failed to end the session '%s' so match stats will not save.  See the logs!"), *SessionName.ToString());
		IOnlineSubsystem::Get()->GetSessionInterface()->DumpSessionState();
	}
	else
	{
		UE_LOG(UT,Verbose,TEXT("OnEndSessionComplete %s"), *SessionName.ToString());
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

void AUTGameSession::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		UE_LOG(UT,Log,TEXT("Failed to destroy the session '%s'!  Matchmaking will be broken until restart.  See the logs!"), *SessionName.ToString());
	}
	else
	{
		UE_LOG(UT,Verbose,TEXT("OnDestroySessionComplete %s"), *SessionName.ToString());
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

void AUTGameSession::UpdateGameState()
{
	const auto OnlineSub = IOnlineSubsystem::Get();
	if (UTBaseGameMode && OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			if (!OnUpdateSessionCompleteDelegate.IsValid())
			{
				OnUpdateSessionCompleteDelegate = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateUObject(this, &AUTGameSession::OnUpdateSessionComplete));
			}

			EOnlineSessionState::Type State = SessionInterface->GetSessionState(GameSessionName);
			if (State != EOnlineSessionState::Creating && State != EOnlineSessionState::Ended && State != EOnlineSessionState::Ending && State != EOnlineSessionState::Destroying && State != EOnlineSessionState::NoSession )
			{
				FUTOnlineGameSettingsBase* OGS = (FUTOnlineGameSettingsBase*)SessionInterface->GetSessionSettings(GameSessionName);

				OGS->Set(SETTING_UTMAXPLAYERS, MaxPlayers, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
				OGS->Set(SETTING_UTMAXSPECTATORS, MaxSpectators, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
				OGS->Set(SETTING_NUMMATCHES, UTBaseGameMode->GetNumMatches(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
				OGS->Set(SETTING_PLAYERSONLINE, UTBaseGameMode->GetNumPlayers(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
				OGS->Set(SETTING_SPECTATORSONLINE, UTBaseGameMode->NumSpectators, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
				OGS->Set(SETTING_SERVERINSTANCEGUID, UTBaseGameMode->ServerInstanceGUID.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

				SessionInterface->UpdateSession(SessionName, *OGS, true);
			}
		}
	}
}

void AUTGameSession::OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful)
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

void AUTGameSession::InitHostBeacon(FOnlineSessionSettings* SessionSettings)
{
	UWorld* const World = GetWorld();
	
	// If the beacon host already exists, just exit this loop.
	if (BeaconHost) 
	{
		UE_LOG(UT,Verbose,TEXT("InitHostBeacon called with an existing beacon."));
		return;
	}

	// Always create a new beacon host
	BeaconHostListener = World->SpawnActor<AOnlineBeaconHost>(AOnlineBeaconHost::StaticClass());
	if (BeaconHostListener == nullptr)
	{
		UE_LOG(UT,Warning,TEXT("Attempt to spawn the BeaconHostListener Failed!  This will cause your online session to not work."));
		return;
	}
	
	BeaconHost = World->SpawnActor<AUTServerBeaconHost>(AUTServerBeaconHost::StaticClass());
	if (BeaconHost == nullptr)
	{
		UE_LOG(UT,Warning,TEXT("Attempt to spawn the BeaconHost Failed!  This will cause your online session to not work."));
		return;
	}

	// Initialize beacon state, either new or from a seamless travel
	bool bBeaconInit = false;
	if (BeaconHostListener && BeaconHostListener->InitHost())
	{
		BeaconHostListener->RegisterHost(BeaconHost);
		UE_LOG(UT,Log,TEXT("---------------------------------------"));
		UE_LOG(UT,Log,TEXT("Server Active Port (ignore ip): %s"), *GetNetDriver()->LowLevelGetNetworkNumber());
		UE_LOG(UT,Log,TEXT("Server Beacon Port: %i"), BeaconHostListener->GetListenPort());
		UE_LOG(UT,Log,TEXT("---------------------------------------"));

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
				UE_LOG(UT,Warning,TEXT("Could not bind to expected beacon port.  Your server will be unresponsive. [%i vs %i]"),BeaconHostListener->GetListenPort(), DesiredPort);
			}
			else
			{
				UE_LOG(UT,Warning,TEXT("Could not bind to expected beacon port.  Restarting server!!! [%i vs %i]"),BeaconHostListener->GetListenPort(), DesiredPort);
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

void AUTGameSession::DestroyHostBeacon()
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

void AUTGameSession::OnConnectionStatusChanged(EOnlineServerConnectionStatus::Type LastConnectionState, EOnlineServerConnectionStatus::Type ConnectionState)
{
	if (ConnectionState == EOnlineServerConnectionStatus::InvalidSession)
	{
		UE_LOG(UT,Warning,TEXT("The Server has been notifed that it's session is no longer valid.  Attempted to recreate a valid session."));
		
		// We have tried to talk to the MCP but our session is invalid.  So we want to reregister
		DestroyHostBeacon();
		bReregisterWhenDone = true;
		UnRegisterServer(true);
	}
}