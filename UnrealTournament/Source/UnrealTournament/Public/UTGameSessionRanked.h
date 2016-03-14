// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PROFILE
#include "OnlineIdentityMcp.h"
#endif

#include "PartyBeaconState.h"

#include "UTGameSessionRanked.generated.h"

class AUTPartyBeaconHost;
class AUTPartyBeaconClient;
class AQosBeaconHost;
struct FEmptyServerReservation;

UCLASS(Config = Game)
class UNREALTOURNAMENT_API AUTGameSessionRanked : public AUTGameSession
{
	GENERATED_BODY()

public:
	AUTGameSessionRanked();
	
	/**
	 * Called after successful creation/advertisement of a dedicated server session
	 * Starts the idle timer to reset the server after IdleTimeout seconds
	 */
	void FinalizeCreation();
	
	/** 
	 * Delegate fired when the beacon receives an empty server configuration request 
	 *
	 * @param GameSessionOwner player making the request
	 * @param PlaylistId game mode to configure
	 * @param WorldSaveName cloud file save to use
	 */
	virtual void OnServerConfigurationRequest(const FUniqueNetIdRepl& GameSessionOwner, const FEmptyServerReservation& ReservationData);

	FOnConnectionStatusChangedDelegate OnConnectionStatusChangedDelegate;
	FOnCreateSessionCompleteDelegate OnCreateSessionCompleteDelegate;
	FOnDestroySessionCompleteDelegate OnDestroySessionCompleteDelegate;
#if WITH_PROFILE
	FOnVerifyAuthCompleteDelegate OnVerifyAuthCompleteDelegate;
	FOnRefreshAuthCompleteDelegate OnRefreshAuthCompleteDelegate;
#endif
	FOnUpdateSessionCompleteDelegate OnUpdateSessionCompleteDelegate;

	FDelegateHandle OnConnectionStatusChangedDelegateHandle;
	FDelegateHandle OnCreateSessionCompleteDelegateHandle;
	FDelegateHandle OnStartSessionCompleteDelegateHandle;
	FDelegateHandle OnEndSessionCompleteDelegateHandle;
	FDelegateHandle OnDestroySessionCompleteDelegateHandle;
	FDelegateHandle OnUpdateSessionCompleteDelegateHandle;

	virtual void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnConnectionStatusChanged(EOnlineServerConnectionStatus::Type LastConnectionState, EOnlineServerConnectionStatus::Type ConnectionState);

#if WITH_PROFILE
	virtual void OnVerifyAuthComplete(bool bWasSuccessful, const class FAuthTokenMcp& AuthToken, const class FAuthTokenVerifyMcp& AuthTokenVerify, const FString& ErrorStr);
	virtual void OnRefreshAuthComplete(bool bWasSuccessful, const FAuthTokenMcp& AuthToken, const FAuthTokenMcp& AuthTokenRefresh, const FString& ErrorStr);
#endif
	
	/** Delegate fired when asking the beacon owner if this reservation is legit */
	//virtual bool OnBeaconValidatePlayers(const TArray<FPlayerReservation>& PartyMembers);

	/** Delegate fired when the beacon indicates a reservation add/remove */
	virtual void OnBeaconReservationChange();

	/** Delegate fired when the beacon indicates all reservations are taken */
	//virtual void OnBeaconReservationsFull();

	/** Delegate fired when the beacon indicates a duplicate reservation request */
	virtual void OnDuplicateReservation(const FPartyReservation& DuplicateReservation);

	/** Delegate fired when the beacon indicates a cancellation has occurred */
	//virtual void OnCancelationReceived(const FUniqueNetId& PartyLeader);

	/** Delegate fired when the beacon indicates a party beacon client has reconnected */
	//virtual void OnProcessReconnectForClient(AUTPartyBeaconClient* ReconnectClient, EPartyReservationResult::Type ReconnectResult);

	virtual void RegisterServer() override;
	virtual void CleanUpOnlineSubsystem() override;

	virtual void ShutdownDedicatedServer();
	virtual void Restart();
	virtual void PauseBeaconRequests(bool bPause);

	virtual void UpdatePlayerNeedsStatus();
	virtual void SetPlayerNeedsSize(FName SessionName, int32 NeedsSize, bool bUpdateSession);
	virtual int32 GetPlayerNeedsSize(FName SessionName);
	virtual void CreateServerGame();
	virtual void CheckForDuplicatePlayer(const FUniqueNetIdRepl& PlayerId);
	
	/**
	 * Update the online session object based on current settings
	 *
	 * @param SessionName Name of session to update
	 * @param SessionSettings Settings to use for update
	 */
	virtual void UpdateSession(FName SessionName, FOnlineSessionSettings& SessionSettings);

	/**
	 * Modifies session settings object to have the passed in values
	 *
	 * @param PlaylistId Sets playlist id setting always
	 */
	virtual void ApplyGameSessionSettings(FOnlineSessionSettings* SessionSettings, int32 PlaylistId, int32 TeamElo) const;
	virtual bool GetGameSessionSettings(const FOnlineSessionSettings* SessionSettings, int32& OutPlaylistId) const;

	virtual void InitHostBeacon(FOnlineSessionSettings* SessionSettings);

	/**
	 * Cleanup host beacon
	 *
	 * @param bPreserveReservations should beacon reservation state be preserved (typically during map travel)
	 */
	virtual void DestroyHostBeacon(bool bPreserveReservations = false);


	const int32 GetPlaylistId() const;

	FTimerHandle StartDedicatedServerTimerHandle;
	FTimerHandle RestartTimerHandle;

	/** Class of beacon to be used for managing reservations/setup */
	UPROPERTY(Transient)
	TSubclassOf<AOnlineBeaconHostObject> ReservationBeaconHostClass;
	/** Class of beacon to be used for receiving Qos requests */
	UPROPERTY(Transient)
	TSubclassOf<AOnlineBeaconHostObject> QosBeaconHostClass;

	/** General beacon listener for registering beacons with */
	AOnlineBeaconHost* BeaconHostListener;
	/** Beacon controlling access to this game */
	UPROPERTY(Transient)
	AUTPartyBeaconHost* ReservationBeaconHost;
	/** Beacon giving Qos details about this server */
	UPROPERTY(Transient)
	AQosBeaconHost* QosBeaconHost;
	/** Current host settings */
	TSharedPtr<FOnlineSessionSettings> HostSettings;

};