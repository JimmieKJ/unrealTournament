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

	virtual void BeginPlay() override;

	/** played on landing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	TSubclassOf<AUTImpactEffect> LandedEffects;

	/**How long the glob will remain on the ground before exploding*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float RestTime;

	/** Biostability expiring = explosion. */
	UFUNCTION()
	virtual void BioStabilityTimer();

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

	virtual void Landed(UPrimitiveComponent* HitComp, const FVector& HitLocation);

	/** hook to spawn effects when the glob lands*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Bio)
	void OnLanded();

	/**Overridden to do the landing*/
	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) override;

	/**Explode on recieving any damage*/
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser) override;

	/** True if can interact with other bio */
	virtual bool CanInteractWithBio();

	/**The sound played when the globs merge together*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
		USoundBase* MergeSound;

	/**The effect played when the globs merge together*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
		UParticleSystem* MergeEffect;

	/** To prevent double merging */
	UPROPERTY()
		bool bHasMerged;

	/** To prevent merging while spawning globlings */
	UPROPERTY()
		bool bSpawningGloblings;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Replicated, ReplicatedUsing = OnRep_GlobStrength, Category = Bio)
		float GlobStrength;

	UFUNCTION()
		virtual void OnRep_GlobStrength();

	/** add this multiplier to damage radius for every point of glob strength beyond the first */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
		float DamageRadiusGainFactor;

	/**The Time added to RestTime when the glob lands*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
		float ExtraRestTimePerStrength;

	/**Sets the strength of the glob*/
	UFUNCTION(BlueprintCallable, Category = Bio)
		virtual void SetGlobStrength(float NewStrength);

	/** hook to size any mesh or effect based on the GlobStrength */
	UFUNCTION(BlueprintNativeEvent, Category = Bio)
		void OnSetGlobStrength();

	/**Anything higher than this will SplashGloblings*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
		uint8 MaxRestingGlobStrength;

	/**GlobStrength > 1 uses this damage type*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
		TSubclassOf<UDamageType> ChargedDamageType;

	virtual void MergeWithGlob(AUTProj_BioShot* OtherBio);

	/** Randomness added to Splash projectile direction. (Dir = SurfaceNormal + VRand() * SplashSpread)  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
		float SplashSpread;
};
