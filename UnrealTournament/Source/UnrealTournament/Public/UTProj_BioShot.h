// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTProj_BioShot.generated.h"

UENUM(BlueprintType)
enum EHitType
{
	HIT_None,
	HIT_Wall,
	HIT_Ceiling,
	HIT_Floor
};
/**
 * 
 */
UCLASS()
class AUTProj_BioShot : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	/** played on landing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	TSubclassOf<AUTImpactEffect> LandedEffects;

	/**The size of the glob collision when we land*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float FloorCollisionRadius;

	/**How long the glob will remain on the ground before exploding*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float RestTime;

	/**The remaining time until this explodes*/
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_RemainingRestTime)
	float RemainingRestTime;
	UFUNCTION()
	virtual void OnRep_RemainingRestTime();

	/**Server: Update RemainingRestTime and explode when finished
	Client: explode when called*/
	UFUNCTION()
	void RemainingRestTimer();

	UFUNCTION(BlueprintCallable, Category = Bio)
	virtual void SetRemainingRestTime(float NewValue);

	/**Set when the glob sticks to a wall*/
	UPROPERTY(BlueprintReadOnly, Category = Bio)
	bool bLanded;
	/**The noraml of the wall we are stuck to*/
	UPROPERTY(BlueprintReadOnly, Category = Bio)
	FVector SurfaceNormal;
	/**The surface type (wall/floor/ceiling)*/
	UPROPERTY(BlueprintReadOnly, Category = Bio)
	TEnumAsByte<EHitType> SurfaceType;

	/**Abs(SurfaceNormal.Z) > SurfaceWallThreshold != Wall*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float SurfaceWallThreshold;

	virtual void Landed(UPrimitiveComponent* HitComp);

	/** hook to spawn effects when the glob lands*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Bio)
	void OnLanded();

	/**Overridden to do the landing*/
	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) override;

	/**Explode on recieving any damage*/
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser);

	/**Grows the collision when we land*/
	virtual void GrowCollision();
};
