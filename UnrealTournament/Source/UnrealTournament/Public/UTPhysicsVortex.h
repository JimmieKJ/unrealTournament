// creates a (client side, visuals only) physics vortex effect for cool deaths
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPhysicsVortex.generated.h"

UCLASS(Blueprintable, NotPlaceable)
class UNREALTOURNAMENT_API AUTPhysicsVortex : public AActor
{
	GENERATED_UCLASS_BODY()

	/** starting vortex force */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vortex)
	float StartingForce;
	/** increase in vortex force per second */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vortex)
	float ForcePerSecond;
	/** radius in which ragdolls have the force applied */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vortex)
	float Radius;
	/** damage type passed to SpawnGibs() when blowing up a ragdoll */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vortex)
	TSubclassOf<UUTDamageType> DamageType;

	virtual void Tick(float DeltaTime) override;
};