// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PartyBeaconState.h"
#include "UTPartyBeaconState.generated.h"

USTRUCT()
struct FEmptyServerReservation
{
	GENERATED_USTRUCT_BODY();

	FEmptyServerReservation() :
		PlaylistId(INDEX_NONE),
		TeamElo(1500),
		bRanked(false),
		bMakePrivate(false)
	{}

	FEmptyServerReservation(int32 InPlaylistId, int32 InTeamElo, bool bInRanked) :
		PlaylistId(InPlaylistId),
		TeamElo(InTeamElo),
		bRanked(false),
		bMakePrivate(false)
	{}

	~FEmptyServerReservation() {}

	/** @return if this is a valid reservation or not */
	bool IsValid() const
	{
		return PlaylistId != INDEX_NONE;
	}

	/** Game mode to play */
	UPROPERTY()
	int32 PlaylistId;
	/** Elo to advertise to play */
	UPROPERTY()
	int32 TeamElo;
	UPROPERTY()
	bool bRanked;
	/** Should the server be created private */
	UPROPERTY()
	bool bMakePrivate;
};

/**
 * A beacon host used for taking reservations for an existing game session
 */
UCLASS(transient, config = Engine)
class UUTPartyBeaconState : public UPartyBeaconState
{
	GENERATED_UCLASS_BODY()
	
	/**
	 * @return the index of the game mode in use
	 */
	const int32 GetPlaylistId() const { return ReservationData.PlaylistId; }

	const int32 GetTeamElo() const { return ReservationData.TeamElo; }

	/**
	 * @return the unique id for the player who configured the beacon
	 */
	const TSharedPtr<const FUniqueNetId>& GetGameSessionOwner() const { return GameSessionOwner.GetUniqueNetId(); }
	
	/**
	 * Change the current owner of the session as far as the beacon is concerned
	 *
	 * @param InNewSessionOwner new user id with admin rights to carry on the session
	 */
	void ChangeSessionOwner(const TSharedRef<const FUniqueNetId>& InNewSessionOwner);

protected:
	
	/** UniqueId of player owning the world session */
	UPROPERTY()
	FUniqueNetIdRepl GameSessionOwner;

    /** 
	 * Reservation data at the time empty server was allocated
	 * (ie WorldId, WorldDataOwner, PlaylistId)
	 */
	UPROPERTY()
	FEmptyServerReservation ReservationData;
	
	/**
	 * Set the various properties associated with this beacon state
	 *
	 * @param ReservationData all reservation data for the request
	 * @param InGameSessionOwner owner of this world cloud save
	 */
	void SetUserConfiguration(const FEmptyServerReservation& InReservationData, const TSharedPtr<const FUniqueNetId>& InGameSessionOwner);
	
	/**
	 * Determine the team number for the given party reservation request.
	 * Uses the list of current reservations to determine what teams have open slots.
	 *
	 * @param PartyRequest the party reservation request received from the client beacon
	 * @return index of the team to assign to all members of this party
	 */
	virtual int32 GetTeamAssignment(const FPartyReservation& Party) override;

	friend class AUTPartyBeaconHost;
};