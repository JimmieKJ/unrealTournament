// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTMovementBaseInterface.h"
#include "UTProj_StingerShard.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTProj_StingerShard : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	/**Overridden to do the stick*/
	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser) override;

	virtual void Destroyed() override;

	/** Damage taken by player jumping off impacted shard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shard)
	int32 ImpactedShardDamage;

	/** Damage taken by player jumping off impacted shard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shard)
	float ImpactedShardMomentum;

	/** Normal of wall this shard impacted on. */
	UPROPERTY()
	FVector ImpactNormal;

	/** Visible static mesh - will collide when shard sticks. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Effects)
	UStaticMeshComponent* ShardMesh;
};