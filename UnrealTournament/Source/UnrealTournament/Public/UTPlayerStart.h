// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPlayerStart.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	/** if set ignore this PlayerStart for Showdown/Team Showdown */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIgnoreInShowdown;

	/** if set ignore this PlayerStart for non-team game modes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIgnoreInNonTeamGame;

	/** if set ignore this PlayerStart for asymmetric CTF */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIgnoreInASymCTF;

	/** if set, this player state will be rated higher if the team is on defense */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDefensiveStart;

	/** pickup associated with this start. If it's a WeaponLocker, the contents will be given to players upon spawning. Otherwise, it can be used as a UI hint for spawn selection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class AUTPickup* AssociatedPickup;

	/** In FlagRUn, will avoid using playerstarts from the same group twice in a row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName PlayerStartGroup;
};