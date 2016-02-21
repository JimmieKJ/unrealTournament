// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameSessionRanked.h"
#include "UTPartyBeaconHost.h"
#include "QoSBeaconHost.h"
#include "UTOnlineGameSettings.h"
#include "UTEmptyServerGameMode.h"

static const float IdleServerTimeout = 30.0f * 60.0f;

AUTGameSessionRanked::AUTGameSessionRanked()
{
	ReservationBeaconHostClass = AUTPartyBeaconHost::StaticClass();
	QosBeaconHostClass = AQosBeaconHost::StaticClass();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		OnConnectionStatusChangedDelegate = FOnConnectionStatusChangedDelegate::CreateUObject(this, &AUTGameSessionRanked::OnConnectionStatusChanged);
		OnCreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &AUTGameSessionRanked::OnCreateSessionComplete);
		OnDestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &AUTGameSessionRanked::OnDestroySessionComplete);
		OnVerifyAuthCompleteDelegate = FOnVerifyAuthCompleteDelegate::CreateUObject(this, &AUTGameSessionRanked::OnVerifyAuthComplete);
		OnRefreshAuthCompleteDelegate = FOnRefreshAuthCompleteDelegate::CreateUObject(this, &AUTGameSessionRanked::OnRefreshAuthComplete);
	}
}

void AUTGameSessionRanked::RegisterServer()
{
	UE_LOG(UT, Verbose, TEXT("--------------[REGISTER SERVER]----------------"));

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		OnConnectionStatusChangedDelegateHandle = OnlineSub->AddOnConnectionStatusChangedDelegate_Handle(OnConnectionStatusChangedDelegate);

		IOnlineIdentityPtr OnlineIdentity = OnlineSub->GetIdentityInterface();
		if (OnlineIdentity.IsValid())
		{
			FOnlineIdentityMcp* OnlineIdentityMcp = (FOnlineIdentityMcp*)OnlineIdentity.Get();
			if (OnlineIdentityMcp)
			{
				OnlineIdentityMcp->AddOnVerifyAuthCompleteDelegate_Handle(OnVerifyAuthCompleteDelegate);
				OnlineIdentityMcp->AddOnRefreshAuthCompleteDelegate_Handle(OnRefreshAuthCompleteDelegate);
			}
		}


		IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			int32 TeamCount = UT_DEFAULT_MAX_TEAM_COUNT;
			int32 TeamSize = UT_DEFAULT_MAX_TEAM_SIZE;
			int32 CurrentPlaylistId = 0;

			UWorld* const World = GetWorld();
			check(World);

			AUTGameMode* const UTGame = World->GetAuthGameMode<AUTGameMode>();
			if (UTGame)
			{
				// If we have an existing playlistid use that
				//FUTPlaylistManager::Get().GetTeamInfoForGame(UTGame->CurrentPlaylistId, TeamCount, TeamSize, MaxPartySize);
			}
			else
			{
				// Get the playlist with the largest team count so we have space for large parties at config time
				//FUTPlaylistManager::Get().GetMaxTeamInfo(TeamCount, TeamSize, MaxPartySize);
			}

			MaxPlayers = TeamCount * TeamSize;

			HostSettings = MakeShareable(new FUTOnlineSessionSettingsDedicatedEmpty(false, false, MaxPlayers));
			
			// Pull the values off the current game if they're set
			ApplyGameSessionSettings(HostSettings.Get(), CurrentPlaylistId);
		
			// Create a beacon
			InitHostBeacon(HostSettings.Get());
			if (ReservationBeaconHost)
			{
				ReservationBeaconHost->OnServerConfigurationRequest().BindUObject(this, &AUTGameSessionRanked::OnServerConfigurationRequest);

				OnCreateSessionCompleteDelegateHandle = SessionInt->AddOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);
				SessionInt->CreateSession(0, GameSessionName, *HostSettings);
			}
			else
			{
				ShutdownDedicatedServer();
			}
		}
	}
}

void AUTGameSessionRanked::OnVerifyAuthComplete(bool bWasSuccessful, const class FAuthTokenMcp& AuthToken, const class FAuthTokenVerifyMcp& AuthTokenVerify, const FString& ErrorStr)
{
	if (!bWasSuccessful)
	{
		if (AuthToken.AuthType != EAuthTokenType::AuthClient)
		{
			FString ReturnReason = NSLOCTEXT("NetworkErrors", "FailedAuth", "User auth check failed.").ToString();

			// failed activation check so find the user and kick him
			bool bDidFindClient = false;
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = *It;
				if (PC != nullptr &&
					PC->PlayerState != nullptr &&
					PC->PlayerState->UniqueId.IsValid() &&
					PC->PlayerState->UniqueId->ToString().Equals(AuthToken.AccountId))
				{
					bDidFindClient = true;
					// If gamesession is on a localplayer, then we're standalone, client will catch OnLogoutComplete delegate
					// If this isnt a local player, kick them from the server to login screen.  Server only verifies at client login time though, not periodically
					if (!PC->IsLocalPlayerController())
					{
						// Server is seeing a client token fail, tell the client to get lost
						UE_LOG(LogOnlineGame, Warning, TEXT("User auth check failed. Server requesting to kick user to login screen UserId=%s bSuccess=%d Error=%s"),
							*AuthToken.AccountId, bWasSuccessful, *ErrorStr);
						PC->ClientReturnToMainMenu(ReturnReason);

					}
				}
			}
			if (bDidFindClient == false)
			{
				UE_LOG(LogOnlineGame, Warning, TEXT("User auth check failed but unable to find playercontroller for UserId=%s bSuccess=%d Error=%s "),
					*AuthToken.AccountId, bWasSuccessful, *ErrorStr);
			}
		}
		else
		{
			// The server auth failed, listen server case.  Listen server will be logged out when/if RefreshAuth fails
			UE_LOG(LogOnlineGame, Error, TEXT("Client auth check failed.  bSuccess=%d Error=%s"),
				bWasSuccessful, *ErrorStr);
		}
	}
}

void AUTGameSessionRanked::OnRefreshAuthComplete(bool bWasSuccessful, const FAuthTokenMcp& AuthToken, const FAuthTokenMcp& AuthTokenRefresh, const FString& ErrorStr)
{
	if (!bWasSuccessful &&
		AuthToken.AuthType == EAuthTokenType::AuthClient)
	{
		// The server auth failed, only thing to do is shutdown
		UE_LOG(LogOnlineGame, Error, TEXT("Client auth refresh failed. Server shutting down! bSuccess=%d Error=%s"),
			bWasSuccessful, *ErrorStr);

		ShutdownDedicatedServer();
	}
}

void AUTGameSessionRanked::CleanUpOnlineSubsystem()
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		OnlineSub->ClearOnConnectionStatusChangedDelegate_Handle(OnConnectionStatusChangedDelegateHandle);

		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
			SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(OnStartSessionCompleteDelegateHandle);
			SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(OnEndSessionCompleteDelegateHandle);
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);
			SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(OnUpdateSessionCompleteDelegateHandle);
		}
	}

	Super::CleanUpOnlineSubsystem();
}

void AUTGameSessionRanked::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName == GameSessionName)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
		if (OnlineSub)
		{
			IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
			SessionInt->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegateHandle);
		}

		if (!bWasSuccessful)
		{
			UE_LOG(LogOnlineGame, Warning, TEXT("Failed to destroy previous game session %s"), *SessionName.ToString());
		}

		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTGameSessionRanked::RegisterServer, 0.1f);
	}
}

void AUTGameSessionRanked::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName == GameSessionName)
	{
		UE_LOG(LogOnlineGame, Verbose, TEXT("OnCreateSessionComplete %s bSuccess: %d"), *SessionName.ToString(), bWasSuccessful);

		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
		if (OnlineSub)
		{
			IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
			SessionInt->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegateHandle);
		}

		if (bWasSuccessful)
		{
			FinalizeCreation();
		}
		else
		{
			ShutdownDedicatedServer();
		}
	}
}

void AUTGameSessionRanked::OnConnectionStatusChanged(EOnlineServerConnectionStatus::Type LastConnectionStatus, EOnlineServerConnectionStatus::Type ConnectionStatus)
{
	if (!FPlatformMisc::IsDebuggerPresent())
	{
		switch (ConnectionStatus)
		{
		case EOnlineServerConnectionStatus::InvalidUser:
		case EOnlineServerConnectionStatus::NotAuthorized:
			UE_LOG(LogOnlineGame, Warning, TEXT("Bad user credentials: %s"), EOnlineServerConnectionStatus::ToString(ConnectionStatus));
			ShutdownDedicatedServer();
			break;
		case EOnlineServerConnectionStatus::ConnectionDropped:
		case EOnlineServerConnectionStatus::NoNetworkConnection:
		case EOnlineServerConnectionStatus::ServiceUnavailable:
		case EOnlineServerConnectionStatus::UpdateRequired:
		case EOnlineServerConnectionStatus::ServersTooBusy:
			UE_LOG(LogOnlineGame, Warning, TEXT("Connection has been interrupted: %s"), EOnlineServerConnectionStatus::ToString(ConnectionStatus));
			ShutdownDedicatedServer();
			break;
		case EOnlineServerConnectionStatus::DuplicateLoginDetected:
			UE_LOG(LogOnlineGame, Warning, TEXT("Duplicate login detected: %s"), EOnlineServerConnectionStatus::ToString(ConnectionStatus));
			ShutdownDedicatedServer();
			break;
		case EOnlineServerConnectionStatus::InvalidSession:
			UE_LOG(LogOnlineGame, Warning, TEXT("Invalid session id: %s"), EOnlineServerConnectionStatus::ToString(ConnectionStatus));
			Restart();
			break;
		}
	}
}

void AUTGameSessionRanked::PauseBeaconRequests(bool bPause)
{
	if (BeaconHostListener)
	{
		BeaconHostListener->PauseBeaconRequests(bPause);
	}
}

void AUTGameSessionRanked::Restart()
{
	UWorld* const World = GetWorld();
	check(World);

	World->GetTimerManager().ClearTimer(RestartTimerHandle);

	if (World->IsInSeamlessTravel())
	{
		UE_LOG(LogOnlineGame, Warning, TEXT("Ignoring dedicated server restart, already in a level travel"));
		// TODO: What should happen here?
		return;
	}
	
	UE_LOG(LogOnlineGame, Display, TEXT("Restarting dedicated server"));

	// Deny future beacon traffic while we restart
	PauseBeaconRequests(true);

	AUTBaseGameMode* const UTGameModeBase = World->GetAuthGameMode<AUTBaseGameMode>();
	check(UTGameModeBase);

	bool bPreserveState = false;
	DestroyHostBeacon(bPreserveState);

	UTGameModeBase->bUseSeamlessTravel = false;

	FString TravelURL = FString::Printf(TEXT("ut-entry?game=empty"));

	World->ServerTravel(TravelURL, true, true);
}

void AUTGameSessionRanked::ShutdownDedicatedServer()
{
	UE_LOG(LogOnlineGame, Warning, TEXT("Shutting down dedicated server..."));
	
	CleanUpOnlineSubsystem();
	DestroyHostBeacon();

	GetWorldTimerManager().ClearTimer(StartDedicatedServerTimerHandle);
	GetWorldTimerManager().ClearTimer(RestartTimerHandle);
	FPlatformMisc::RequestExit(false);
}

void AUTGameSessionRanked::FinalizeCreation()
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			GetWorldTimerManager().SetTimer(RestartTimerHandle, this, &AUTGameSessionRanked::Restart, IdleServerTimeout);
		}
	}
	
	UE_LOG(LogOnlineGame, Display, TEXT("Dedicated Server Ready!"));
	GLog->Flush();
}

void AUTGameSessionRanked::DestroyHostBeacon(bool bPreserveReservations)
{
	if (BeaconHostListener)
	{
		if (ReservationBeaconHost)
		{
			BeaconHostListener->UnregisterHost(ReservationBeaconHost->GetBeaconType());
		}

		if (QosBeaconHost)
		{
			BeaconHostListener->UnregisterHost(QosBeaconHost->GetBeaconType());
		}
		
		UE_LOG(LogOnlineGame, Verbose, TEXT("Destroying Host Beacon."));
	}

	if (ReservationBeaconHost)
	{
		ReservationBeaconHost->OnValidatePlayers().Unbind();
		ReservationBeaconHost->OnReservationChanged().Unbind();
		ReservationBeaconHost->OnReservationsFull().Unbind();
		ReservationBeaconHost->OnDuplicateReservation().Unbind();
		ReservationBeaconHost->OnCancelationReceived().Unbind();
		ReservationBeaconHost->OnServerConfigurationRequest().Unbind();
		ReservationBeaconHost->OnProcessReconnectForClient().Unbind();

		UE_LOG(LogOnlineGame, Verbose, TEXT("Destroying Reservation Beacon."));
		ReservationBeaconHost->Destroy();
		ReservationBeaconHost = nullptr;
	}

	if (QosBeaconHost)
	{
		UE_LOG(LogOnlineGame, Verbose, TEXT("Destroying QoS Beacon."));
		QosBeaconHost->Destroy();
		QosBeaconHost = nullptr;
	}
	
	if (!bPreserveReservations)
	{
		// Drop the previous state of the beacon
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
		if (OnlineSub)
		{
			OnlineSub->SetNamedInterface(UT_BEACON_STATE, nullptr);
		}
	}
}

void AUTGameSessionRanked::OnServerConfigurationRequest(const FUniqueNetIdRepl& GameSessionOwner, const FEmptyServerReservation& ReservationData)
{
	UWorld* const World = GetWorld();
	check(World);

	if (ReservationBeaconHost && GameSessionOwner.IsValid() && ReservationData.IsValid())
	{
		int32 TeamCount = UT_DEFAULT_MAX_TEAM_COUNT;
		int32 TeamSize = UT_DEFAULT_MAX_TEAM_SIZE;
		int32 MaxPartySize = UT_DEFAULT_PARTY_SIZE;

		// Get the playlist configuration for team/reservation sizes
		if (ReservationData.PlaylistId > INDEX_NONE)
		{
			//if (FUTPlaylistManager::Get().GetTeamInfoForGame(ReservationData.PlaylistId, ReservationData.ZoneInstanceId, TeamCount, TeamSize, MaxPartySize))
			{
				MaxPlayers = TeamCount * TeamSize;
				ReservationBeaconHost->ReconfigureTeamAndPlayerCount(TeamCount, TeamSize, MaxPlayers);
			}
		}

		// Save the state of the beacon as we're about to travel
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
		if (OnlineSub)
		{
			OnlineSub->SetNamedInterface(UT_BEACON_STATE, ReservationBeaconHost->GetState());
		}

		// Remove the idle timer for dedicated server once owning player makes request
		GetWorldTimerManager().ClearTimer(RestartTimerHandle);

		if (OnlineSub)
		{
			IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
			FOnlineSessionSettings* SessionSettings = SessionInt.IsValid() ? SessionInt->GetSessionSettings(GameSessionName) : NULL;
			if (SessionSettings)
			{
				SessionSettings->bShouldAdvertise = !ReservationData.bMakePrivate;

				// Update the session settings with the reservation data
				ApplyGameSessionSettings(SessionSettings, ReservationData.PlaylistId);

				// Block all party needs
				SetPlayerNeedsSize(GameSessionName, 0, false);

				// Update the connection count to match the team size above
				if (MaxPlayers != 0)
				{
					SessionSettings->NumPublicConnections = MaxPlayers;
				}
			}
		}

		GetWorldTimerManager().SetTimerForNextTick(this, &AUTGameSessionRanked::CreateServerGame);
	}
}

void AUTGameSessionRanked::SetPlayerNeedsSize(FName SessionName, int32 NeedsSize, bool bUpdateSession)
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			FOnlineSessionSettings* SessionSettings = SessionInt->GetSessionSettings(SessionName);
			if (SessionSettings)
			{
				UE_LOG(LogOnlineGame, Verbose, TEXT("Setting %s player need size to %d"), *SessionName.ToString(), NeedsSize);
				if (GetPlayerNeedsSize(SessionName) != NeedsSize)
				{
					// Two values to overcome query limitation
					SessionSettings->Set(SETTING_NEEDS, NeedsSize, EOnlineDataAdvertisementType::ViaOnlineService);
					SessionSettings->Set(SETTING_NEEDSSORT, NeedsSize, EOnlineDataAdvertisementType::ViaOnlineService);
					SessionInt->UpdateSession(SessionName, *SessionSettings, bUpdateSession);
				}
				else
				{
					UE_LOG(LogOnlineGame, Verbose, TEXT("Need size already at %d"), NeedsSize);
				}
			}
		}
	}
}

int32 AUTGameSessionRanked::GetPlayerNeedsSize(FName SessionName)
{
	int32 CurrentNeedsSize = 0;

	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			FOnlineSessionSettings* SessionSettings = SessionInt->GetSessionSettings(SessionName);
			if (SessionSettings)
			{
				SessionSettings->Get(SETTING_NEEDS, CurrentNeedsSize);
			}
		}
	}

	return CurrentNeedsSize;
}

const int32 AUTGameSessionRanked::GetPlaylistId() const
{
	if (ReservationBeaconHost)
	{
		return ReservationBeaconHost->GetPlaylistId();
	}

	return INDEX_NONE;
}

void AUTGameSessionRanked::CreateServerGame()
{
	UWorld* World = GetWorld();
	check(World);
	int32 PlaylistId = GetPlaylistId();
	
	PauseBeaconRequests(true);
	// Let the beacon get destroyed on actor cleanup.  Allows RPCs to finish during the server travel countdown
	bool bPreserveState = true;
	DestroyHostBeacon(bPreserveState);
	
	FString TravelURL = TEXT("/Game/RestrictedAssets/Maps/WIP/DM-Chill?game=TEAMSHOWDOWN?Ranked=1"); // GetMatchURL(PlaylistId);
	
	World->ServerTravel(TravelURL, true, false);
}

void AUTGameSessionRanked::ApplyGameSessionSettings(FOnlineSessionSettings* SessionSettings, int32 PlaylistId) const
{
	if (!SessionSettings)
	{
		return;
	}

	if (PlaylistId > INDEX_NONE)
	{
		SessionSettings->Set(SETTING_PLAYLISTID, PlaylistId, EOnlineDataAdvertisementType::ViaOnlineService);
	}
}

void AUTGameSessionRanked::InitHostBeacon(FOnlineSessionSettings* SessionSettings)
{
	UWorld* const World = GetWorld();
	AUTBaseGameMode* const UTGame = World->GetAuthGameMode<AUTBaseGameMode>();
	check(UTGame);

	AUTEmptyServerGameMode* EmptyDedicatedMode = Cast<AUTEmptyServerGameMode>(UTGame);
	bool bIsEmptyServer = EmptyDedicatedMode != nullptr;

	UE_LOG(LogOnlineGame, Verbose, TEXT("Creating host beacon."));
	check(!ReservationBeaconHost);
	check(!QosBeaconHost);

	// All the parameters needed for configuring the beacon host
	int32 PlaylistId = INDEX_NONE;
	FString WUID, ZoneInstanceId;

	// Always create a new beacon host, state will be determined in a moment
	BeaconHostListener = World->SpawnActor<AOnlineBeaconHost>(AOnlineBeaconHost::StaticClass());
	check(BeaconHostListener);

	UPartyBeaconState* BeaconState = nullptr;

	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		BeaconState = Cast<UPartyBeaconState>(OnlineSub->GetNamedInterface(UT_BEACON_STATE));
	}

	// Initialize beacon state, either new or from a seamless travel
	bool bBeaconInit = false;
	if (BeaconHostListener->InitHost())
	{
		ReservationBeaconHost = World->SpawnActor<AUTPartyBeaconHost>(ReservationBeaconHostClass);
		check(ReservationBeaconHost);

		int32 TeamCount = UT_DEFAULT_MAX_TEAM_COUNT;
		int32 TeamSize = UT_DEFAULT_MAX_TEAM_SIZE;

		//FUTPlaylistManager& PlaylistManager = FFortPlaylistManager::Get();

		if (BeaconState)
		{
			// If we have a beacon state get the game values off the old beacon state
			bBeaconInit = ReservationBeaconHost->InitFromBeaconState(BeaconState);

			PlaylistId = ReservationBeaconHost->GetPlaylistId();

			MaxPlayers = ReservationBeaconHost->GetNumTeams() * ReservationBeaconHost->GetMaxPlayersPerTeam();

//			PlaylistManager.GetTeamInfoForGame(PlaylistId, ZoneInstanceId, TeamCount, TeamSize, MaxPartySize);
			if (TeamCount != ReservationBeaconHost->GetNumTeams())
			{
				UE_LOG(LogOnline, Warning, TEXT("Playlist team count != Beacon team count"));
			}

			if (TeamSize != ReservationBeaconHost->GetMaxPlayersPerTeam())
			{
				UE_LOG(LogOnline, Warning, TEXT("Playlist team size != Beacon team size"));
			}

			UpdatePlayerNeedsStatus();
		}
		else
		{
			// If we don't have a beacon state get the values off the session settings
			if (!bIsEmptyServer && (!GetGameSessionSettings(SessionSettings, PlaylistId) || PlaylistId == INDEX_NONE))
			{
				UE_LOG(LogOnline, Warning, TEXT("Invalid playlist id at beacon creation time"));
			}

			//PlaylistManager.GetTeamInfoForGame(PlaylistId, ZoneInstanceId, TeamCount, TeamSize, MaxPartySize);

			MaxPlayers = TeamCount * TeamSize;

			bBeaconInit = ReservationBeaconHost->InitHostBeacon(TeamCount, TeamSize, TeamSize * TeamCount, GameSessionName);
		}

		BeaconHostListener->RegisterHost(ReservationBeaconHost);

		QosBeaconHost = World->SpawnActor<AQosBeaconHost>(QosBeaconHostClass);
		if (QosBeaconHost)
		{
			if (QosBeaconHost->Init(GameSessionName))
			{
				BeaconHostListener->RegisterHost(QosBeaconHost);
				if (SessionSettings)
				{
					SessionSettings->Set(SETTING_QOS, 1, EOnlineDataAdvertisementType::ViaOnlineService);
				}
			}
		}

		if (!bIsEmptyServer)
		{
			// Do any other advertising, not sure UT needs a lobby beacon like other games
		}
	}

	// Update the beacon port
	if (bBeaconInit)
	{
		if (SessionSettings)
		{
			SessionSettings->Set(SETTING_BEACONPORT, BeaconHostListener->GetListenPort(), EOnlineDataAdvertisementType::ViaOnlineService);
		}
	}
	else
	{
		UE_LOG(LogOnlineGame, Warning, TEXT("Failed to spawn reservation host beacon %s"), *ReservationBeaconHostClass->GetName());
		DestroyHostBeacon();
		// Shutdown the server??
	}

	if (ReservationBeaconHost)
	{
		// Setup values for the world manager when it is created later. This uses either the old beacon data or the session settings
		//UTGame->CurrentPlaylistId = PlaylistId;

		//ReservationBeaconHost->OnValidatePlayers().BindUObject(this, &AUTGameSessionRanked::OnBeaconValidatePlayers);
		ReservationBeaconHost->OnReservationChanged().BindUObject(this, &AUTGameSessionRanked::OnBeaconReservationChange);
		//ReservationBeaconHost->OnReservationsFull().BindUObject(this, &AUTGameSessionRanked::OnBeaconReservationsFull);
		ReservationBeaconHost->OnDuplicateReservation().BindUObject(this, &AUTGameSessionRanked::OnDuplicateReservation);
		//ReservationBeaconHost->OnCancelationReceived().BindUObject(this, &AUTGameSessionRanked::OnCancelationReceived);
		//ReservationBeaconHost->OnProcessReconnectForClient().BindUObject(this, &AUTGameSessionRanked::OnProcessReconnectForClient);
		PauseBeaconRequests(false);
	}
}

void AUTGameSessionRanked::OnBeaconReservationChange()
{
	UE_LOG(LogOnlineGame, Verbose, TEXT("OnBeaconReservationChange"));
	UWorld* World = GetWorld();
	check(World);
	
	UpdatePlayerNeedsStatus();
}

void AUTGameSessionRanked::UpdatePlayerNeedsStatus()
{
	if (ReservationBeaconHost)
	{
		int32 NeedsSize = ReservationBeaconHost->GetMaxAvailableTeamSize();
		SetPlayerNeedsSize(GameSessionName, NeedsSize, true);
	}
}

void AUTGameSessionRanked::OnDuplicateReservation(const FPartyReservation& DuplicateReservation)
{
	UE_LOG(LogOnlineGame, Verbose, TEXT("OnDuplicateReservation %s"), *DuplicateReservation.PartyLeader.ToString());

	for (int32 PartyMemberIdx = 0; PartyMemberIdx < DuplicateReservation.PartyMembers.Num(); PartyMemberIdx++)
	{
		const FUniqueNetIdRepl& PlayerId = DuplicateReservation.PartyMembers[PartyMemberIdx].UniqueId;
		CheckForDuplicatePlayer(PlayerId);
	}

	UpdatePlayerNeedsStatus();
}

void AUTGameSessionRanked::CheckForDuplicatePlayer(const FUniqueNetIdRepl& PlayerId)
{
	UWorld* World = GetWorld();
	APlayerController* PC = GetPlayerControllerFromNetId(World, *PlayerId);
	if (PC)
	{
		// Unregister the player from the session, then nullptr the id so HandlePlayerLogout (during Destroy) doesn't remove the reservation for the new logging in connection
		UnregisterPlayer(PC);
		PC->PlayerState->UniqueId.SetUniqueNetId(nullptr);

		// Kick the player (destroys pawn/controller)
		KickPlayer(PC, NSLOCTEXT("NetworkErrors", "DuplicateReservation", "Duplicate player detected."));
	}
}

bool AUTGameSessionRanked::GetGameSessionSettings(const FOnlineSessionSettings* SessionSettings, int32& OutPlaylistId) const
{
	if (!SessionSettings)
	{
		return false;
	}

	bool bFound = false;

	bFound |= SessionSettings->Get(SETTING_PLAYLISTID, OutPlaylistId);

	return bFound;
}

void AUTGameSessionRanked::UpdateSession(FName SessionName, FOnlineSessionSettings& SessionSettings)
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			OnUpdateSessionCompleteDelegateHandle = SessionInt->AddOnUpdateSessionCompleteDelegate_Handle(OnUpdateSessionCompleteDelegate);
			SessionInt->UpdateSession(SessionName, SessionSettings, true);
		}
	}
}
