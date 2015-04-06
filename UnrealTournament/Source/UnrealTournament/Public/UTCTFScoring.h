// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
/** Handles individual scoring in CTF matches. */

#pragma once
#include "UTCTFScoring.generated.h"

UCLASS(config = Game)
class UNREALTOURNAMENT_API AUTCTFScoring : public AInfo
{
	GENERATED_UCLASS_BODY()

	/** Cached reference to the CTF game state */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
	class AUTCTFGameState* CTFGameState;

	// CTF Scoring.. made config for now so we can easily tweak it all.  Won't be config in the final.
	// Each player who carried the flag get's a % of this pool and is guarenteed at least 1 point
	UPROPERTY(Config)
	uint32 FlagTotalScorePool;

	// The player who initially picked up the flag will give thee points
	UPROPERTY(Config)
		uint32 FlagFirstPickupPoints;

	// The player who caps the flag will get these points
	UPROPERTY(Config)
		uint32 FlagCapPoints;

	// # of points the player receives for returning the flag
	UPROPERTY(Config)
		uint32 FlagReturnPoints;
	// # of points the player receives if they return the flag in the enemy score zone
	UPROPERTY(Config)
		uint32 FlagReturnEnemyZoneBonus;

	// # of points hte player receives if they deny a score
	UPROPERTY(Config)
		uint32 FlagReturnDenialBonus;

	// Players who are near the flag get bonuses when they kill
	UPROPERTY(Config)
		uint32 BaseKillScore;

	// How close to the flag carrier do you have to be to get Flag combat bonus points
	UPROPERTY(Config)
		float FlagCombatBonusDistance;

	// How close to the opposing home base does a flag have to be for a denial
	UPROPERTY(Config)
		float FlagDenialDistance;

	// Players who are near the flag when it's captured get this bonus
	UPROPERTY(Config)
		uint32 ProximityCapBonus;

	// Players near the flag when it's returned get this bonus
	UPROPERTY(Config)
		uint32 ProximityReturnBonus;

	// Players who are near the flag get bonuses when they kill
	UPROPERTY(Config)
		uint32 CombatBonusKillBonus;

	// Damage points are muiltipled by this when combat is near the flag.
	UPROPERTY(config)
		float FlagCarrierCombatMultiplier;

	// How many points for picking up a minor pickup
	UPROPERTY(config)
		uint32 MinorPickupScore;

	// How many points for picking up a major pickup
	UPROPERTY(config)
		uint32 MajorPickupScore;

	// How many points for picking up a super pickup
	UPROPERTY(config)
		uint32 SuperPickupScore;

	// Multiplier to use if the player controls a pickup
	UPROPERTY(config)
		float ControlFreakMultiplier;

	UPROPERTY(config)
		uint32 FlagHolderPointsPerSecond;

	virtual void BeginPlay() override;
	virtual void FlagHeldTimer();
	virtual void ScorePickup(AUTPickup* Pickup, AUTPlayerState* PickedUpBy, AUTPlayerState* LastPickedUpBy);
	virtual void ScoreDamage(int32 DamageAmount, AController* Victim, AController* Attacker);
	virtual void ScoreKill(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType);
	virtual void ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason, float TimeLimit);

	virtual bool IsCloseToFlagCarrier(AActor* Who, float CheckDistanceSquared, uint8 TeamNum = 255);
};
