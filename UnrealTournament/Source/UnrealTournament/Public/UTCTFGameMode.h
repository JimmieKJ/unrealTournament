// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTTeamGameMode.h"
#include "UTCTFGameMode.generated.h"

namespace MatchState
{
	extern const FName MatchEnteringHalftime;		// Entering Halftime
	extern const FName MatchIsAtHalftime;			// The match has entered halftime
	extern const FName MatchExitingHalftime;		// Exiting Halftime
	extern const FName MatchEnteringSuddenDeath;	// The match is entering sudden death
	extern const FName MatchIsInSuddenDeath;		// The match is in sudden death
} 

UCLASS()
class UNREALTOURNAMENT_API AUTCTFGameMode : public AUTTeamGameMode
{
	GENERATED_UCLASS_BODY()

	/** If true, CTF allow for a sudden death after overtime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint32 bSuddenDeath:1;	

	/** Cached reference to the CTF game state */
	UPROPERTY(BlueprintReadOnly, Category=CTF)
	AUTCTFGameState* CTFGameState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint32 HalftimeDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint32 OvertimeDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint32 SuddenDeathHealthDrain;

	virtual void InitGameState();

	virtual void InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage );
	virtual void ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason);
	virtual void ScoreHolder(AUTPlayerState* Holder);
	virtual bool CheckScore(AUTPlayerState* Scorer);
	virtual void CheckGameTime();
	virtual void GameObjectiveInitialized(AUTGameObjective* Obj);

	virtual void CallMatchStateChangeNotify() override;

	virtual void HandleEnteringHalftime();
	virtual void HandleHalftime();
	virtual void HandleExitingHalftime();
	virtual void HandleEnteringSuddenDeath();
	virtual void HandleSuddenDeath();
	virtual void HandleEnteringOvertime();
	virtual void HandleMatchInOvertime() override;

	virtual void DefaultTimer();
	virtual bool PlayerCanRestart( APlayerController* Player );

	virtual void EndGame(AUTPlayerState* Winner, FName Reason);
	virtual void SetEndGameFocus(AUTPlayerState* Winner);
	void BuildServerResponseRules(FString& OutRules);

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

	UPROPERTY(config)
	uint32 TelefragBonus;

	virtual void HandleMatchHasStarted();

	UFUNCTION()
	virtual void HalftimeIsOver();

	UFUNCTION()
	virtual bool IsMatchInSuddenDeath();

	virtual void ScorePickup(AUTPickup* Pickup, AUTPlayerState* PickedUpBy, AUTPlayerState* LastPickedUpBy);
	virtual void ScoreDamage(int32 DamageAmount, AController* Victim, AController* Attacker) override;
	virtual void ScoreKill(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType);

	virtual bool IsCloseToFlagCarrier(AActor* Who, float CheckDistanceSquared, uint8 TeamNum=255);

	// returns the team index of a team with advatage or < 0 if no team has one
	virtual uint8 TeamWithAdvantage();

	// Look to see if the team that had advantage still has it
	virtual bool CheckAdvantage();

	// Holds the amount of time to give a flag carrier who has the flag out going in to half-time
	int AdvantageGraceTime;

	virtual void EndOfHalf();

	virtual void UpdateSkillRating() override;

#if !UE_SERVER
public:
	virtual void BuildPlayerInfo(TSharedPtr<SVerticalBox> Panel, AUTPlayerState* PlayerState);
#endif


};


