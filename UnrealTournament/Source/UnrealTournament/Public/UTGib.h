// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"

#include "UTGib.generated.h"

UCLASS(Abstract)
class UNREALTOURNAMENT_API AUTGib : public AActor
{
	GENERATED_UCLASS_BODY()

	/** gib mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Gib)
	UMeshComponent* Mesh;

	/** destroy after this much time alive if out of view (InitialLifeSpan indicates visible lifespan) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gib)
	float InvisibleLifeSpan;

	/** blood effects mirrored from owning Pawn */
	UPROPERTY(BlueprintReadWrite, Category = Effects)
	TArray<UParticleSystem*> BloodEffects;
	UPROPERTY(BlueprintReadWrite, Category = Effects)
	TArray<struct FBloodDecalInfo> BloodDecals;

	/** last time we spawned blood effect/decal */
	UPROPERTY(BlueprintReadWrite, Category = Effects)
	float LastBloodTime;

	virtual void PreInitializeComponents() override;

	virtual void BeginPlay() override;

	/** Destroy gib if not still visible - called after InvisibleLifeSpan seconds. */
	virtual void CheckGibVisibility();

	UFUNCTION()
	virtual void OnPhysicsCollision(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
