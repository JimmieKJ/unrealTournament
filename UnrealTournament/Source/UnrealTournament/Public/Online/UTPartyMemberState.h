// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PartyMemberState.h"
#include "UTPartyMemberState.generated.h"

class UUTPartyGameState;

UENUM()
enum class EUTPartyMemberLocation : uint8
{
	/** No destination yet */
	PreLobby,
	/** Destination found and attempting to join */
	ConnectingToLobby,
	/** Properly connected to the lobby */
	Lobby,
	/** Launching the game */
	JoiningGame,
	/** Successfully in the game */
	InGame
};

/**
 * Simple struct for replication and copying of party member data on updates
 */
USTRUCT()
struct FUTPartyMemberRepState : public FPartyMemberRepState
{
	GENERATED_USTRUCT_BODY()
	
	FUTPartyMemberRepState()
		: Location(EUTPartyMemberLocation::PreLobby)
	{
	}
	
	/** Coarse location of this party member in the game */
	UPROPERTY()
	EUTPartyMemberLocation Location;

	/** Reset the variables of this party member state */
	void Reset() override;
};

/**
 * Main representation of a party member
 */
UCLASS(config = Game, notplaceable)
class UNREALTOURNAMENT_API UUTPartyMemberState : public UPartyMemberState
{
	GENERATED_UCLASS_BODY()

	/** @return the party this member is associated with */
	UUTPartyGameState* GetUTParty() const { return Cast<UUTPartyGameState>(GetOuter()); }

	/** @return the party member replication state */
	FUTPartyMemberRepState& GetState();

	/**
	 * Compare current data to old data, triggering delegates
	 *
	 * @param OldPartyMemberState old view of data to compare against
	 */
	void ComparePartyMemberData(const FPartyMemberRepState& OldPartyMemberState) override;

	/** Reset to default state */
	void Reset() override;
	
	/** State data in struct form */
	UPROPERTY(Transient)
	FUTPartyMemberRepState MemberState;

	friend UUTPartyGameState;

	/** @return the coarse location of the party member in the game */
	EUTPartyMemberLocation GetLocation() const { return MemberState.Location; }

private:

	/**
	* Update the location of the party member within the game
	*
	* @param NewLocation new "location" within the game (see EFortPartyMemberLocation)
	*/
	void SetLocation(EUTPartyMemberLocation NewLocation);
};
