// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
/** Handles individual scoring in CTF matches. */

#pragma once
#include "UTCTFScoring.generated.h"

static const FName NAME_AttackerScore(TEXT("AttackerScore"));
static const FName NAME_DefenderScore(TEXT("DefenderScore"));
static const FName NAME_SupporterScore(TEXT("SupporterScore"));

static const FName NAME_FlagHeldDeny(TEXT("FlagHeldDeny"));
static const FName NAME_FlagHeldDenyTime(TEXT("FlagHeldDenyTime"));
static const FName NAME_FlagHeldTime(TEXT("FlagHeldTime"));
static const FName NAME_FlagReturnPoints(TEXT("FlagReturnPoints"));
static const FName NAME_CarryAssist(TEXT("CarryAssist"));
static const FName NAME_CarryAssistPoints(TEXT("CarryAssistPoints"));
static const FName NAME_FlagCapPoints(TEXT("FlagCapPoints"));
static const FName NAME_DefendAssist(TEXT("DefendAssist"));
static const FName NAME_DefendAssistPoints(TEXT("DefendAssistPoints"));
static const FName NAME_ReturnAssist(TEXT("ReturnAssist"));
static const FName NAME_ReturnAssistPoints(TEXT("ReturnAssistPoints"));
static const FName NAME_TeamCapPoints(TEXT("TeamCapPoints"));
static const FName NAME_EnemyFCDamage(TEXT("EnemyFCDamage"));
static const FName NAME_FCKills(TEXT("FCKills"));
static const FName NAME_FCKillPoints(TEXT("FCKillPoints"));
static const FName NAME_FlagSupportKills(TEXT("FlagSupportKills"));
static const FName NAME_FlagSupportKillPoints(TEXT("FlagSupportKillPoints"));
static const FName NAME_RegularKillPoints(TEXT("RegularKillPoints"));
static const FName NAME_FlagGrabs(TEXT("FlagGrabs"));
static const FName NAME_UDamageTime(TEXT("UDamageTime"));
static const FName NAME_BerserkTime(TEXT("BerserkTime"));
static const FName NAME_InvisibilityTime(TEXT("InvisbilityTime"));
static const FName NAME_BootJumps(TEXT("BootJumps"));
static const FName NAME_ShieldBeltCount(TEXT("ShieldBeltCount"));
static const FName NAME_ArmorVestCount(TEXT("ArmorVestCount"));
static const FName NAME_ArmorPadsCount(TEXT("ArmorPadsCount"));
static const FName NAME_HelmetCount(TEXT("HelmetCount"));

static const FName NAME_TeamKills(TEXT("TeamKills"));
static const FName NAME_TeamFlagGrabs(TEXT("TeamFlagGrabs"));
static const FName NAME_TeamFlagHeldTime(TEXT("TeamFlagHeldTime"));

UCLASS()
class UNREALTOURNAMENT_API AUTCTFScoring : public AInfo
{
	GENERATED_UCLASS_BODY()

	/** Cached reference to the CTF game state */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
	class AUTCTFGameState* CTFGameState;

	// CTF Scoring.. made config for now so we can easily tweak it all.  Won't be config in the final.
	// Each player who carried the flag get's a % of this pool and is guarenteed at least 1 point
	UPROPERTY()
		uint32 FlagRunScorePool;

	// The player who initially picked up the flag will give thee points
	UPROPERTY()
		uint32 FlagFirstPickupPoints;

	// The player who caps the flag will get these points
	UPROPERTY()
		uint32 FlagCapPoints;

	// # of points the player receives for returning the flag
	UPROPERTY()
		uint32 FlagReturnPoints;

	// Points multiplied by time your flag was held before being returned
	UPROPERTY()
		float FlagReturnHeldBonus;

	// Points multiplied by time your flag was held before carrier was killed
	UPROPERTY()
		float FlagKillHeldBonus;

	UPROPERTY()
		uint32 MaxFlagHeldBonus;

	// Players who are near the flag get bonuses when they kill
	UPROPERTY()
		uint32 BaseKillScore;

	// Bonus for killing players threatening the flag carrier
	UPROPERTY()
		uint32 FlagCombatKillBonus;

	// Bonus for killing enemy flag carrier.
	UPROPERTY()
		float FlagCarrierKillBonus;

	// Bonus for assist by returning enemy flag allowing team to score.
	UPROPERTY()
		float FlagReturnAssist;

	// Bonus for assist by killing enemies threatening FC allowing team to score.
	UPROPERTY()
		float FlagSupportAssist;

	// Bonus score for everyone on team after flag cap.
	UPROPERTY()
		float TeamCapBonus;

	// Points per second for holding flag when own flag is out
	UPROPERTY()
		float FlagHolderPointsPerSecond;

	UPROPERTY()
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
