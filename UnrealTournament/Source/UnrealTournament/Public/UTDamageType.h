// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

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
class UUTDamageType : public UDamageType
{
	GENERATED_UCLASS_BODY()

	/** if set, force some of the momentum of impacts to also be applied to upward Z
	 * this makes hits more consistently noticeable and gameplay relevant, particularly in UT
	 * because walking acceleration is very fast
	 * however, this causes a juggle/lockdown effect if many hits are applied rapidly
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Momentum)
	bool bForceZMomentum;

	/** called on the server when a player is killed by this damagetype */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void ScoreKill(AUTPlayerState* KillerState, AUTPlayerState* VictimState, APawn* KilledPawn) const;
};

/** return the base momentum for the given damage event (before radial damage and any other modifiers) */
extern FVector UTGetDamageMomentum(const FDamageEvent& DamageEvent, const AActor* HitActor, const AController* EventInstigator);