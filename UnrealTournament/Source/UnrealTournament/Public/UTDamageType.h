// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDamageType.generated.h"

USTRUCT(BlueprintType)
struct FUTPointDamageEvent : public FPointDamageEvent
{
	GENERATED_USTRUCT_BODY()

	FUTPointDamageEvent()
	: FPointDamageEvent()
	{}
	FUTPointDamageEvent(float InDamage, const FHitResult& InHitInfo, const FVector& InShotDirection, TSubclassOf<UDamageType> InDamageTypeClass, const FVector& InMomentum = FVector::ZeroVector)
	: FPointDamageEvent(InDamage, InHitInfo, InShotDirection, InDamageTypeClass), Momentum(InMomentum)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	FVector Momentum;

	/** ID for this class. NOTE this must be unique for all damage events. */
	static const int32 ClassID = 101;
	virtual int32 GetTypeID() const { return FUTPointDamageEvent::ClassID; };
	virtual bool IsOfType(int32 InID) const { return (FUTPointDamageEvent::ClassID == InID) || FPointDamageEvent::IsOfType(InID); };
};

USTRUCT(BlueprintType)
struct FUTRadialDamageEvent : public FRadialDamageEvent
{
	GENERATED_USTRUCT_BODY()

	FUTRadialDamageEvent()
	: FRadialDamageEvent(), bScaleMomentum(true)
	{}

	/** momentum magnitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float BaseMomentumMag;
	/** whether to scale the momentum to the percentage of damage received (i.e. due to distance) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	bool bScaleMomentum;

	/** ID for this class. NOTE this must be unique for all damage events. */
	static const int32 ClassID = 102;
	virtual int32 GetTypeID() const { return FUTRadialDamageEvent::ClassID; };
	virtual bool IsOfType(int32 InID) const { return (FUTRadialDamageEvent::ClassID == InID) || FRadialDamageEvent::IsOfType(InID); };
};

UCLASS()
class UNREALTOURNAMENT_API UUTDamageType : public UDamageType
{
	GENERATED_UCLASS_BODY()

	/** allows temporarily reducing walking characters' ability to change directions
	 * this is particularly useful for weapons to exert momentum without kicking the target up into the air
	 * since by default friction and acceleration are very high
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Momentum)
	float WalkMovementReductionPct;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Momentum)
	float WalkMovementReductionDuration;

	/** if set, force this percentage of the momentum of impacts to also be applied to upward Z
	* this makes hits more consistently noticeable and gameplay relevant, particularly in UT
	* because walking acceleration is very fast
	* however, this causes a juggle/lockdown effect if many hits are applied rapidly
	*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Meta = (EditCondition = "bForceZMomentum"), Category = Momentum)
	float ForceZMomentumPct;

	/** Multiplying factor for momentum applied to self */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Momentum)
	float SelfMomentumBoost;

	/** if set SelfMomentumBoost only applies to Z component of momentum */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Momentum)
	uint32 bSelfMomentumBoostOnlyZ : 1;

	/** if set, force some of the momentum of impacts to also be applied to upward Z
	 * this makes hits more consistently noticeable and gameplay relevant, particularly in UT
	 * because walking acceleration is very fast
	 * however, this causes a juggle/lockdown effect if many hits are applied rapidly
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Momentum)
	uint32 bForceZMomentum : 1;

	/** if set this damagetype doesn't cause momentum on Z axis (= victim not set to falling state) while the victim is walking
	 * supersedes bForceZMomentum in that case
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Momentum)
	uint32 bPreventWalkingZMomentum : 1;

	/** whether this damagetype causes blood effects (generally should be set unless damagetype implements some other feedback) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Effects)
	uint32 bCausesBlood : 1;

	/** Whether armor can reduce this damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Momentum)
	uint32 bBlockedByArmor : 1;

	/** optional body color to flash in victim's material when hit with this damage type */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Effects)
	const UCurveLinearColor* BodyDamageColor;
	/** if set apply BodyDamageColor to character edges only (rim shader) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Effects)
	bool bBodyDamageColorRimOnly;

	/** if dead Pawn's health <= this value than it gibs (unless hard disabled by client option)
	 * set to a positive number to always gib (since dead Pawns can't have positive health)
	 * set to a large negative number to never gib
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = DamageType)
	int32 GibHealthThreshold;
	/** if the damage of the fatal blow exceeds this value than the Pawn gibs (unless hard disabled by client option) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = DamageType)
	int32 GibDamageThreshold;

	/** called on the server when a player is killed by this damagetype */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void ScoreKill(AUTPlayerState* KillerState, AUTPlayerState* VictimState, APawn* KilledPawn) const;

	/** called on clients for dead characters killed by this damagetype to decide if they should gib instead of ragdoll */
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	bool ShouldGib(AUTCharacter* Victim) const;

	/** spawn/play any visual hit effects this damagetype should cause to the passed in Pawn
	 * use LastTakeHitInfo to retrieve additional information
	 * client only
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void PlayHitEffects(AUTCharacter* HitPawn, bool bPlayedArmorEffect) const;

	/** spawn/play any clientside effects for a Pawn killed by this damagetype
	 * not called if the character is gibbed (ShouldGib())
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void PlayDeathEffects(AUTCharacter* DyingPawn) const;

	/** spawn/play any clientside effects on gibs */
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void PlayGibEffects(class AUTGib* Gib) const;

	/** This is the console death message that will be sent to everyone not involved when someone dies of this damage type.  Supports all of the {xxx} varaiable commands of the messaging system*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Messages)
	FText ConsoleDeathMessage;

	/** This is the male suicide console death message that will be sent to everyone not involved when someone dies of this damage type.  Supports all of the {xxx} varaiable commands of the messaging system*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Messages)
	FText MaleSuicideMessage;

	/** This is the female suciide console death message that will be sent to everyone not involved when someone dies of this damage type.  Supports all of the {xxx} varaiable commands of the messaging system*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Messages)
	FText FemaleSuicideMessage;

	/** This is the victim message (centered) suicide text. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Messages)
		FText SelfVictimMessage;

	/** this is the name that will be used for the {WeaponName} message option*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Messages)
	FText AssociatedWeaponName;

	// @TODO FIXMESTEVE replace with KillStatsName and DeathStatsName
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Stats)
	FString StatsName;

	/** Reward announcement when kill with this damagetype (e.g. headshot). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Announcement)
	TSubclassOf<class UUTRewardMessage> RewardAnnouncementClass;

	/* If set, will play as weapon spree announcement when WeaponSpreeCount is reached. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Announcement)
		FName SpreeSoundName;

	/* String displayed when weapon spree sound is played. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Announcement)
		FString SpreeString;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Announcement)
		int32 WeaponSpreeCount;
};

/** return the base momentum for the given damage event (before radial damage and any other modifiers) */
extern FVector UTGetDamageMomentum(const FDamageEvent& DamageEvent, const AActor* HitActor, const AController* EventInstigator);