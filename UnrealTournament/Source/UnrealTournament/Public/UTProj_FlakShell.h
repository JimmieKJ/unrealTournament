// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProjectile.h"
#include "UTProj_FlakShell.generated.h"

/**
 * Flak Cannon Shell
 * - High trajectory
 * - Explodes on impact
 * - Spawns 5 Shards on impact
 */
UCLASS(Abstract, meta = (ChildCanTick))
class UNREALTOURNAMENT_API AUTProj_FlakShell : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	/** Number of shards to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flak Cannon")
	int32 ShardSpawnCount;

	/** Angle for spawning shards, relative to hit normal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flak Cannon")
	float ShardSpawnAngle;
	
	/** Shard class type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flak Cannon")
	TSubclassOf<AUTProjectile> ShardClass;

	virtual void Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp = NULL) override;
};
