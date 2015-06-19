// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Runtime/Online/OnlineSubsystemUtils/Classes/OnlineBeaconHostObject.h"
#include "PartyBeaconState.h"
#include "PartyBeaconHost.generated.h"

class FUniqueNetId;
class UNetConnection;
class UPartyBeaconState;
class AOnlineBeaconClient;
class APartyBeaconClient;

/**
 * Delegate fired when a the beacon host detects a reservation addition/removal
 */
DECLARE_DELEGATE(FOnReservationChanged);

/**
 * Delegate fired when a the beacon host has been told to cancel a reservation
 *
 * @param PartyLeader leader of the canceled reservation
 */
DECLARE_DELEGATE_OneParam(FOnCancelationReceived, const FUniqueNetId&);

/**
 * Delegate called when the beacon gets any request, allowing the owner to validate players at a higher level (bans,etc)
 * 
 * @param PartyMembers players making up the reservation
 * @return true if these players are ok to join
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnValidatePlayers, const TArray<FPlayerReservation>&);

/**
 * Delegate fired when a the beacon host detects a duplicate reservation
 *
 * @param PartyReservation reservation that is found to be duplicated
 */
DECLARE_DELEGATE_OneParam(FOnDuplicateReservation, const FPartyReservation&);

/**
 * Delegate fired when a the beacon host detects that all reservations are full
 */
DECLARE_DELEGATE(FOnReservationsFull);

/**
 * A beacon host used for taking reservations for an existing game session
 */
UCLASS(transient, notplaceable, config=Engine)
class ONLINESUBSYSTEMUTILS_API APartyBeaconHost : public AOnlineBeaconHostObject
{
	GENERATED_UCLASS_BODY()

	// Begin AActor Interface
	virtual void Tick(float DeltaTime) override;
	// End AActor Interface

	// Begin AOnlineBeaconHostObject Interface 
	virtual AOnlineBeaconClient* SpawnBeaconActor(UNetConnection* ClientConnection) override;
	virtual void ClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection) override;
	virtual void RemoveClientActor(AOnlineBeaconClient* ClientActor) override;
	// End AOnlineBeaconHost Interface 

	/**
	 * Initialize the party host beacon
	 *
	 * @param InTeamCount number of teams to make room for
	 * @param InTeamSize size of each team
	 * @param InMaxReservation max number of reservations allowed
	 * @param InSessionName name of session related to the beacon
	 * @param InForceTeamNum team to force players on if applicable (usually only 1 team games)
	 *
	 * @return true if successful created, false otherwise
	 */
	virtual bool InitHostBeacon(int32 InTeamCount, int32 InTeamSize, int32 InMaxReservations, FName InSessionName, int32 InForceTeamNum = 0);

	/**
	 * Initialize the party host beacon from a previous state/configuration
	 * all existing reservations and configuration values are preserved
     *
	 * @return true if successful created, false otherwise
	 */
	virtual bool InitFromBeaconState(UPartyBeaconState* PrevState);

	/** 
	 * Reconfigures the beacon for a different team/player count configuration
	 * Allows dedicated server to change beacon parameters after a playlist configuration has been made
	 * Does no real checking against current reservations because we assume the UI wouldn't let 
	 * this party start a gametype if they were too big to fit on a team together
	 * @param InNumTeams the number of teams that are expected to join
	 * @param InNumPlayersPerTeam the number of players that are allowed to be on each team
	 * @param InNumReservations the total number of players to allow to join (if different than team * players)
	 */
	virtual bool ReconfigureTeamAndPlayerCount(int32 InNumTeams, int32 InNumPlayersPerTeam, int32 InNumReservations);

	/**
	 * @return reference to the state of the PartyBeacon
	 */
	UPartyBeaconState* GetState() const { return State; }

	/**
	 * Notify the beacon of a player logout
	 *
	 * @param PlayerId UniqueId of player logging out
	 */
	virtual void HandlePlayerLogout(const FUniqueNetIdRepl& PlayerId);

	/**
	 * Get the current reservation count inside the beacon
	 *
	 * @return number of consumed reservations
	 */
	virtual int32 GetReservationCount() const { return State->GetReservationCount(); }

	/**
	 * Get the number of players on a team across all existing reservations
	 *
	 * @param TeamIdx team to query
	 *
	 * @return number of players on the given team
	 */
	int32 GetNumPlayersOnTeam(int32 TeamIdx) const;

	/**
	 * Finds the current team assignment of the given player net id.
	 * Looks at the reservation entries to find the player.
	 *
	 * @param PlayerId net id of player to find
	 * @return index of team assignment for the given player, INDEX_NONE if not found
	 */
	int32 GetTeamForCurrentPlayer(const FUniqueNetId& PlayerId) const;

	/**
	 * Get the number of teams.
	 *
	 * @return The number of teams.
	 */
	int32 GetNumTeams() const { return State->GetNumTeams(); }

	/**
	 * Get the max number of players per team
	 *
	 * @return The number of players per team
	 */
	virtual int32 GetMaxPlayersPerTeam() const { return State->GetMaxPlayersPerTeam(); }

	/**
	 * Determine the maximum team size that can be accommodated based
	 * on the current reservation slots occupied.
	 *
	 * @return maximum team size that is currently available
	 */
	virtual int32 GetMaxAvailableTeamSize() const { return State->GetMaxAvailableTeamSize(); }

	/**
	 * Swap the parties between teams, parties must be of same size
	 *
	 * @param PartyLeader party 1 to swap
	 * @param OtherPartyLeader party 2 to swap
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool SwapTeams(const FUniqueNetIdRepl& PartyLeader, const FUniqueNetIdRepl& OtherPartyLeader);

	/**
	 * Place a party on a new team, party must fit and team must exist
	 *
	 * @param PartyLeader party to change teams
	 * @param NewTeamNum team to change to
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool ChangeTeam(const FUniqueNetIdRepl& PartyLeader, int32 NewTeamNum);

	/**
	 * Does a given player id have an existing reservation
     *
	 * @param PlayerId uniqueid of the player to check
     *
	 * @return true if a reservation exists, false otherwise
	 */
	virtual bool PlayerHasReservation(const FUniqueNetId& PlayerId) const;

	/**
	 * Obtain player validation string from party reservation entry
	 *
	 * @param PlayerId unique id of player to find validation in an existing reservation
	 * @param OutValidation [out] validation string used when player requested a reservation
	 *
	 * @return true if reservation exists for player
	 *
	 */
	virtual bool GetPlayerValidation(const FUniqueNetId& PlayerId, FString& OutValidation) const;

	/**
	 * Attempts to add a party reservation to the beacon
     *
     * @param ReservationRequest reservation attempt
     *
     * @return add attempt result
	 */
	EPartyReservationResult::Type AddPartyReservation(const FPartyReservation& ReservationRequest);

	/**
	 * Attempts to remove a party reservation from the beacon
     *
     * @param PartyLeader reservation leader
	 */
	virtual void RemovePartyReservation(const FUniqueNetIdRepl& PartyLeader);

	/**
	 * Handle a reservation request received from an incoming client
	 *
	 * @param Client client beacon making the request
	 * @param SessionId id of the session that is being checked
	 * @param ReservationRequest payload of request
	 */
	virtual void ProcessReservationRequest(APartyBeaconClient* Client, const FString& SessionId, const FPartyReservation& ReservationRequest);

	/**
	 * Handle a reservation cancellation request received from an incoming client
	 *
	 * @param Client client beacon making the request
	 * @param PartyLeader reservation leader
	 */
	virtual void ProcessCancelReservationRequest(APartyBeaconClient* Client, const FUniqueNetIdRepl& PartyLeader);

	/**
	 * Delegate fired when a the beacon host detects a reservation addition/removal
	 */
	FOnReservationsFull& OnReservationsFull() { return ReservationsFull; }

	/**
	 * Delegate fired when a the beacon host detects that all reservations are full
	 */
	FOnReservationChanged& OnReservationChanged() { return ReservationChanged; }

	/**
	 * Delegate fired when a the beacon host cancels a reservation
	 */
	FOnCancelationReceived& OnCancelationReceived() { return CancelationReceived; }

	/**
	 * Delegate fired when a the beacon detects a duplicate reservation
	 */
	FOnDuplicateReservation& OnDuplicateReservation() { return DuplicateReservation; }

	/**
	 * Delegate called when the beacon gets any request, allowing the owner to validate players at a higher level (bans,etc)
	 */
	FOnValidatePlayers& OnValidatePlayers() { return ValidatePlayers; }

	/**
	 * Output current state of reservations to log
	 */
	virtual void DumpReservations() const;

protected:

	/** State of the beacon */
	UPROPERTY()
	UPartyBeaconState* State;

	/** List of all party beacon actors with active connections */
	UPROPERTY()
	TArray<AOnlineBeaconClient*> ClientActors;

	/** Delegate fired when the beacon indicates all reservations are taken */
	FOnReservationsFull ReservationsFull;
	/** Delegate fired when the beacon indicates a reservation add/remove */
	FOnReservationChanged ReservationChanged;
	/** Delegate fired when the beacon indicates a reservation cancellation */
	FOnCancelationReceived CancelationReceived;
	/** Delegate fired when the beacon detects a duplicate reservation */
	FOnDuplicateReservation DuplicateReservation;
	/** Delegate fired when asking the beacon owner if this reservation is legit */
	FOnValidatePlayers ValidatePlayers;

	/** Seconds that can elapse before a reservation is removed due to player not being registered with the session */
	UPROPERTY(Transient, Config)
	float SessionTimeoutSecs;
	/** Seconds that can elapse before a reservation is removed due to player not being registered with the session during a travel */
	UPROPERTY(Transient, Config)
	float TravelSessionTimeoutSecs;

	/**
	 * @return the class of the state object inside this beacon
	 */
	virtual TSubclassOf<UPartyBeaconState> GetPartyBeaconHostClass() const { return UPartyBeaconState::StaticClass(); }

	/**
	 * Handle a newly added player
	 *
	 * @param NewPlayer reservation of newly joining player
	 */
	void NewPlayerAdded(const FPlayerReservation& NewPlayer);

	/**
	 * Does the session match the one associated with this beacon
	 *
	 * @param SessionId session to compare
	 *
	 * @return true if the session matches, false otherwise
	 */
	bool DoesSessionMatch(const FString& SessionId) const;
};
