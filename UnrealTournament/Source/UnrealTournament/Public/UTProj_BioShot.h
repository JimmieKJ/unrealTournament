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

	/** @TODO FIXMESTEVE  figure out best way to replicate web once past prototype stage. */
	UPROPERTY(ReplicatedUsing = OnRep_WebLinkOne)
		AUTProj_BioShot* WebLinkOne;

	/** @TODO FIXMESTEVE  figure out best way to replicate web once past prototype stage. */
	UPROPERTY(ReplicatedUsing = OnRep_WebLinkTwo)
		AUTProj_BioShot* WebLinkTwo;

	UFUNCTION()
		virtual void OnRep_WebLinkOne();

	UFUNCTION()
		virtual void OnRep_WebLinkTwo();

	/** How much life to add to goo when added to web. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Web)
		float WebLifeBoost;

	/** Prevent re-entrancy of TriggerWeb(). */
	UPROPERTY()
		bool bTriggeringWeb;

	/** Prevent re-entrancy of RemoveWebLink(). */
	UPROPERTY()
		bool bRemovingWebLink;

	/** Prevent re-entrancy of WebConnected(). */
	UPROPERTY()
		bool bAddingWebLink;

	/** Max distance between goo to link in web */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Web)
		float MaxLinkDistance;

	/** Initial full blob pulse rate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Web)
		float InitialBlobPulseRate;

	/** Max full blob pulse rate (right before web trigger threshold) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Web)
		float MaxBlobPulseRate;

	/** How much to scale when pulsing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Web)
		float BlobPulseScaling;

	/** Current charge of full blob being overcharged to web */
	UPROPERTY()
	float BlobOverCharge;

	/** updated to pulse blob sinusoidally */
	UPROPERTY()
		float BlobPulseTime;

	/** Web triggering overcharge level */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Web)
	float BlobWebThreshold;

	/** Overcharged blob disperses smaller blobs to create web trap */
	UFUNCTION(BlueprintCallable, Category = Bio)
	virtual void BlobToWeb(const FVector& NormalDir);

	/** Linking effect between webbed bio goo */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Web)
		UParticleSystem* WebLinkEffect;

	/**The sound played when the globs WebLink together*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Web)
		USoundBase* WebLinkSound;

	/** Make a connection between this goo and LinkedBio.  Return true if link was added. */
	UFUNCTION(BlueprintCallable, Category = Bio)
	virtual bool AddWebLink(AUTProj_BioShot* LinkedBio);

	/** Remove connection between this goo and LinkedBio */
	UFUNCTION(BlueprintCallable, Category = Bio)
	virtual void RemoveWebLink(AUTProj_BioShot* LinkedBio);

	virtual void Destroyed() override;

	/** played on landing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	TSubclassOf<AUTImpactEffect> LandedEffects;

	/**How long the glob will remain on the ground before exploding*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float RestTime;

	/**Base time a charged glob will remain on the ground before exploding*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float ChargedRestTime;

	/** Biostability expiring = explosion. */
	UFUNCTION()
	virtual void BioStabilityTimer();

	/**Set when the glob sticks to a wall/floor*/
	UPROPERTY(ReplicatedUsing = OnRep_BioLanded, BlueprintReadOnly, Category = Bio)
	bool bLanded;

	UFUNCTION()
		virtual void OnRep_BioLanded();

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

	/** Glob which was overcharged to create a linked web. */
	UPROPERTY(ReplicatedUsing = OnRep_BioLanded, BlueprintReadOnly, Category = Bio)
		AUTProj_BioShot* WebMaster;

	/** Linked list of globs in overcharge web. */
	UPROPERTY(BlueprintReadOnly, Category = Bio)
		AUTProj_BioShot* WebChild;

	/** Return true if can web link to LinkedBio */
	UFUNCTION(BlueprintCallable, Category = Bio)
	virtual bool CanWebLinkTo(AUTProj_BioShot* LinkedBio);

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

	virtual void ShutDown() override;

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
	float DamageRadiusGain;

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
