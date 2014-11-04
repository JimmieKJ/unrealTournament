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

USTRUCT()
struct FBioWebLink
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	class AUTProj_BioShot* LinkedBio;

	UPROPERTY()
	UParticleSystemComponent* WebLink;

	FBioWebLink()
		: LinkedBio(NULL)
		, WebLink(NULL)
	{}

	FBioWebLink(class AUTProj_BioShot* InLinkedBio, UParticleSystemComponent* InWebLink) : LinkedBio(InLinkedBio), WebLink(InWebLink) {};
};

UCLASS()
class AUTProj_BioShot : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	virtual void BeginPlay() override;

	/** Array of bio goo web linked to this bio. */
	UPROPERTY()
	TArray<FBioWebLink> WebLinks;

	/** How much life to add to goo when added to web. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Web)
		float WebLifeBoost;

	/** Prevent re-entrancy of RemoveWebLink(). */
	UPROPERTY()
		bool bRemovingWebLink;

	/** Prevent re-entrancy of WebConnected(). */
	UPROPERTY()
		bool bAddingWebLink;

	/** Max distance between goo to link in web */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Web)
		float MaxLinkDistance;

	/** Linking effect between webbed bio goo */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Web)
		UParticleSystem* WebLinkEffect;

	/** Make a connection between this goo and LinkedBio */
	virtual void WebConnected(AUTProj_BioShot* LinkedBio);

	/** Remove connection between this goo and LinkedBio */
	virtual void RemoveWebLink(AUTProj_BioShot* LinkedBio);

	virtual void Destroyed() override;

	/** played on landing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	TSubclassOf<AUTImpactEffect> LandedEffects;

	/**How long the glob will remain on the ground before exploding*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float RestTime;

	/** Biostability expiring = explosion. */
	UFUNCTION()
	virtual void BioStabilityTimer();

	/**Set when the glob sticks to a wall/floor*/
	UPROPERTY(BlueprintReadOnly, Category = Bio)
	bool bLanded;

	/**Set when the glob can track enemies*/
	UPROPERTY(BlueprintReadOnly, Category = Bio)
	bool bCanTrack;

	/** How far away to notify other bio about tracking*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
		float TrackingRange;

	/** How far away to notify other bio about tracking*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
		float MaxTrackingSpeed;

	/** Radius scaling for large globs in the air*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Bio)
		float GlobRadiusScaling;

	/** Move toward TrackedPawn */
	virtual void Track(AUTCharacter* NewTracked);

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	/**The noraml of the wall we are stuck to*/
	UPROPERTY(BlueprintReadWrite, Category = Bio)
	AUTCharacter* TrackedPawn;

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

	virtual void OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity) override;

	/** Max speed goo can slide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float MaxSlideSpeed;

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

	/** Base overlap radius for Landed bio */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
		float LandedOverlapRadius;

	/** Adds to landed overlap radius based on glob size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
		float LandedOverlapScaling;

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
