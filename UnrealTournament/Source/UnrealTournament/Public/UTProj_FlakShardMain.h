// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProj_FlakShard.h"
#include "UTProj_FlakShardMain.generated.h"

/**
 * Flak Cannon Main Shard
 * Same as standard shard, except that:
 * - Can bounce up to 3 times
 * - Damage is increased when firing at point blank at middle of character
 */
UCLASS(Abstract, meta = (ChildCanTick))
class UNREALTOURNAMENT_API AUTProj_FlakShardMain : public AUTProj_FlakShard
{
	GENERATED_UCLASS_BODY()

	/** Momentum bonus for point blank shots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float CenteredMomentumBonus;

	/** Damage bonus for point blank shots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float CenteredDamageBonus;

	/** Timeout for point blank shots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float MaxBonusTime;

	/** Timeout for ShreddedReward */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
		float MaxShreddedTime;

	/** camera shake played on direct short range (bonus applies) hit that kills the target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	TSubclassOf<class UCameraShake> ShortRangeKillShake;

	/** Reward announcement for close up flak kill. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Announcement)
		TSubclassOf<class UUTRewardMessage> CloseFlakRewardMessageClass;

	virtual void DamageImpactedActor_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) override;
	virtual FRadialDamageParams GetDamageParams_Implementation(AActor* OtherActor, const FVector& HitLocation, float& OutMomentum) const override;
	virtual void OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity) override;

};
