// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTCTFGameMode.generated.h"

UCLASS()
class AUTCTFGameMode : public AUTTeamGameMode
{
	GENERATED_UCLASS_BODY()

	/** If true, CTF will use old school CTF end-game rules where ScoreLimit needs to be hit and there is just 1 half */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint32 bOldSchool:1;	

	/** Holds the # of teams expected for this version of CTF.  This isn't a command line switch rather a var that can be subclassed or blueprinted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint8 MaxNumberOfTeams;

	/** Cached reference to the CTF game state */
	UPROPERTY(BlueprintReadOnly, Category=CTF)
	AUTCTFGameState* CTFGameState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint32 HalftimeDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint32 OvertimeDuration;


	virtual void InitGameState();

	virtual void InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage );
	virtual void ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason);
	virtual bool CheckScore(AUTPlayerState* Scorer);
	virtual bool IsAWinner(AUTPlayerController* PC);
	virtual void CheckGameTime();
	virtual void GameObjectiveInitialized(AUTGameObjective* Obj);
	virtual void StartHalftime();
	virtual void FreezePlayers();
	virtual void FocusOnBestPlayer();
	virtual void RestartPlayer(AController* aPlayer);

protected:

	// CTF Scoring.. made config for now so we can easily tweak it all.  Won't be config in the final.

	// FLAG Capture points

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
	uint32 FlagReturnPoints;
	// # of points the player receives if they return the flag in the enemy score zone
	uint32 FlagReturnEnemyZoneBonus;

	// # of points hte player receives if they deny a score
	uint32 FlagReturnDenialBonus;

	// Players who are near the flag get bonuses when they kill
	UPROPERTY(Config)
	uint32 BaseKillScore;

	// Flag combat

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


	UFUNCTION()
	virtual void HalftimeIsOver();

	virtual void ScoreDamage(int DamageAmount, AController* Victim, AController* Attacker);
	virtual void ScoreKill(AController* Killer, AController* Other);

	virtual bool IsCloseToFlagCarrier(AActor* Who, float CheckDistanceSquared, uint8 TeamNum=255);

};


