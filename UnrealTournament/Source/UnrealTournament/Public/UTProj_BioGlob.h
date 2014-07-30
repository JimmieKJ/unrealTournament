// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProj_BioShot.h"
#include "UTProj_BioGlob.generated.h"

/**
 * 
 */
UCLASS()
class AUTProj_BioGlob: public AUTProj_BioShot
{
	GENERATED_UCLASS_BODY()

	/**The sound played when the globs merge together*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	USoundBase* MergeSound;

	/**The effect played when the globs merge together*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	UParticleSystem* MergeEffect;

	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_GlobStrength, Category = Bio)
	uint8 GlobStrength;
	UFUNCTION()
	virtual void OnRep_GlobStrength();

	/**The amount to scale the collision per GlobStrength*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float GlobStrengthCollisionScale;

	/** add this multiplier to damage radius for every point of glob strength beyond the first */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float DamageRadiusGainFactor;

	/**The Time added to RestTime when the glob lands*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float ExtraRestTimePerStrength;

	/**Sets the strength of the glob*/
	UFUNCTION(BlueprintCallable, Category = Bio)
	virtual void SetGlobStrength(uint8 NewStrength);

	/** hook to size any mesh or effect based on the GlobStrength */
	UFUNCTION(BlueprintNativeEvent, Category = Bio)
	void OnSetGlobStrength();

	/**Anything higher than this will SplashGloblings*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	uint8 MaxRestingGlobStrength;

	/**GlobStrength > 1 uses this damage type*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	TSubclassOf<UDamageType> ChargedDamageType;

	/**Over a certain strength we will use this damage type to gib*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	TSubclassOf<UDamageType> GibDamageType;

	void MergeWithGlob(int32 AdditionalGlobStrength);

	/** Overridden to Merge Globs and noclip globlings */
	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) override;

	/** The projectile type that will spawn when (GlobStrength > MaxRestingGlobStrength) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	TSubclassOf<AUTProjectile> SplashProjClass;

	/** Randomness added to Splash projectile direction. (Dir = SurfaceNormal + VRand() * SplashSpread)  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float SplashSpread;

	/**Spawns several children globs */
	virtual void SplashGloblings();

	/**Overridden to SplashGloblings*/
	virtual void Landed(UPrimitiveComponent* HitComp);

	/**Grows the collision based on GlobStrength*/
	virtual void GrowCollision();
};
