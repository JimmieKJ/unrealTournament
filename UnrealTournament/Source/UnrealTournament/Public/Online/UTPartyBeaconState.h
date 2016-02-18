// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Runtime/Online/OnlineSubsystemUtils/Public/PartyBeaconState.h"
#include "UTPartyBeaconState.generated.h"

USTRUCT()
struct FEmptyServerReservation
{
	GENERATED_USTRUCT_BODY();

	FEmptyServerReservation() :
		PlaylistId(INDEX_NONE),
		bMakePrivate(false)
	{}

	FEmptyServerReservation(int32 InPlaylistId) :
		PlaylistId(InPlaylistId),
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

protected:
	
    /** 
	 * Reservation data at the time empty server was allocated
	 * (ie WorldId, WorldDataOwner, PlaylistId)
	 */
	UPROPERTY()
	FEmptyServerReservation ReservationData;

	friend class AUTPartyBeaconHost;
};