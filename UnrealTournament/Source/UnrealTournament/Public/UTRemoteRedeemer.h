// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProj_Redeemer.h"
#include "UTRemoteRedeemer.generated.h"

UCLASS(Abstract, config = Game)
class UNREALTOURNAMENT_API AUTRemoteRedeemer : public APawn, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	/** Sphere collision component */
	UPROPERTY(VisibleDefaultsOnly, Category = Projectile)
	USphereComponent* CollisionComp;

	/** Projectile movement component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Movement)
	class UUTProjectileMovementComponent* ProjectileMovement;

	/** Capsule collision component */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Projectile)
	UCapsuleComponent* CapsuleComp;

	/** Used to get damage values */
	UPROPERTY(EditDefaultsOnly, Category = Projectile)
	TSubclassOf<class AUTProj_Redeemer> RedeemerProjectileClass;

	UFUNCTION()
	virtual void OnStop(const FHitResult& Hit);

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	virtual void Destroyed() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category = Vehicle)
	APawn* Driver;

	/** Controller that gets credit for damage, since main Controller will be lost due to driver leaving when we blow up */
	UPROPERTY(BlueprintReadOnly, Category = Projectile)
	AController* DamageInstigator;

	/** Effects for full nuclear blast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	TSubclassOf<class AUTImpactEffect> ExplosionEffects;

	/** Effect when detonated by enemy fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	TSubclassOf<class AUTImpactEffect> DetonateEffects;

	UFUNCTION()
	virtual bool TryToDrive(APawn* NewDriver);

	UFUNCTION()
	virtual bool DriverEnter(APawn* NewDriver);

	UFUNCTION()
	virtual bool DriverLeave(bool bForceLeave);
	
	UFUNCTION()
	void BlowUp();

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual uint8 GetTeamNum() const;
	// not applicable
	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum) override
	{}

	void FaceRotation(FRotator NewControlRotation, float DeltaTime) override;
	virtual void GetActorEyesViewPoint(FVector& out_Location, FRotator& out_Rotation) const override;
	void PawnStartFire(uint8 FireModeNum) override;

	/** material to draw over the player's view when zoomed
	* the material can use the parameter 'TeamColor' to receive the player's team color in team games (won't be changed in FFA modes)
	*/
	UPROPERTY(EditDefaultsOnly, Category = Zoom)
		UMaterialInterface* OverlayMat;

	/** material instance for e.g. team coloring */
	UPROPERTY()
		UMaterialInstanceDynamic* OverlayMI;

	/** material for drawing enemy indicators */
	UPROPERTY(EditDefaultsOnly, Category = Zoom)
		UTexture2D* TargetIndicator;

	virtual void PostRender(class AUTHUD* HUD, UCanvas* Canvas);

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerBlowUp();

	UPROPERTY()
	float YawAccel;

	UPROPERTY()
	float PitchAccel;

	UPROPERTY()
	float AccelRate;

	UPROPERTY()
	float AccelerationBlend;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RemoteRedeemer)
	float RedeemerMouseSensitivity;

	UPROPERTY()
	float MaximumRoll;

	UPROPERTY()
	float MaxPitch;
	
	UPROPERTY()
	float MinPitch;

	UPROPERTY()
	float RollMultiplier;

	UPROPERTY()
	float RollSmoothingMultiplier;

	float ExplosionTimings[5];
	float ExplosionRadii[6];
	float CollisionFreeRadius;

	bool bExploded;
	/** set when we were shot down by an enemy instead of exploding on contact with the big boom */
	UPROPERTY(Replicated)
	bool bShotDown;

	virtual void TornOff() override;

	void ShutDown();

	void ExplodeStage(float RangeMultiplier);

	/** Create effects for full nuclear blast. */
	virtual void PlayExplosionEffects();

	/** Create effects for small explosion when destroyed by enemy fire. */
	virtual void PlayDetonateEffects();

	virtual void Detonate();

	UFUNCTION()
	void ExplodeStage1();
	UFUNCTION()
	void ExplodeStage2();
	UFUNCTION()
	void ExplodeStage3();
	UFUNCTION()
	void ExplodeStage4();
	UFUNCTION()
	void ExplodeStage5();
	UFUNCTION()
	void ExplodeStage6();

	UFUNCTION(reliable, client)
	void ForceReplication();

	virtual FVector GetVelocity() const override;

	virtual void RedeemerDenied(AController* InstigatedBy);

	// This should get moved to Vehicle when it gets implemented
	virtual bool IsRelevancyOwnerFor(const AActor* ReplicatedActor, const AActor* ActorOwner, const AActor* ConnectionActor) const override;

	virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
};