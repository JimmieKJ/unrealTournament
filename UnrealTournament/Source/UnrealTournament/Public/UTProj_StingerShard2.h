// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTMovementBaseInterface.h"
#include "UTProj_StingerShard2.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTProj_StingerShard2 : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	virtual void DamageImpactedActor_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) override;
	virtual void OnPawnSphereOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	virtual void Tick(float DeltaTime);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 DirectHitDamage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DirectHitMomentum;

	UPROPERTY()
	TArray<APawn*> PotentialTargets;

	/** Visible static mesh - will collide when shard sticks. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Effects)
	UStaticMeshComponent* ShardMesh;
};