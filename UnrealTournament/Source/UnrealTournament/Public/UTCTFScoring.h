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
		uint32 FlagRunScorePool;

	// The player who initially picked up the flag will give thee points
	UPROPERTY(Config)
		uint32 FlagFirstPickupPoints;

	// The player who caps the flag will get these points
	UPROPERTY(Config)
		uint32 FlagCapPoints;

	// # of points the player receives for returning the flag
	UPROPERTY(Config)
		uint32 FlagReturnPoints;

	// Points multiplied by time your flag was held before being returned
	UPROPERTY(Config)
		uint32 FlagReturnHeldBonus;

	// Points multiplied by time your flag was held before carrier was killed
	UPROPERTY(Config)
		uint32 FlagKillHeldBonus;

	UPROPERTY(Config)
		uint32 MaxFlagHeldBonus;

	// Players who are near the flag get bonuses when they kill
	UPROPERTY(Config)
		uint32 BaseKillScore;

	// Bonus for killing players threatening the flag carrier
	UPROPERTY(Config)
		uint32 FlagCombatKillBonus;

	// Bonus for killing enemy flag carrier.
	UPROPERTY(config)
		float FlagCarrierKillBonus;

	// Bonus for assist by returning enemy flag allowing team to score.
	UPROPERTY(config)
		float FlagReturnAssist;

	// Bonus score for everyone on team after flag cap.
	UPROPERTY(config)
		float TeamCapBonus;

	// Points per second for holding flag when own flag is out
	UPROPERTY(config)
		uint32 FlagHolderPointsPerSecond;

	UPROPERTY(config)
		float RecentActionTimeThreshold;

	virtual void BeginPlay() override;
	virtual void FlagHeldTimer();
	virtual void ScoreDamage(int32 DamageAmount, AController* Victim, AController* Attacker);
	virtual void ScoreKill(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType);
	virtual void ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason, float TimeLimit);

	virtual bool WasThreateningFlagCarrier(AUTPlayerState *VictimPS, APawn* KilledPawn, AUTPlayerState *KillerPS);

	/** Return how long flag was held before current scoring action. */
	virtual float GetTotalHeldTime(AUTCarriedObject* GameObject);
};
