// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProj_Redeemer.h"
#include "UTRemoteRedeemer.generated.h"

UCLASS(Abstract, config = Game)
class AUTRemoteRedeemer : public APawn, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	/** Sphere collision component */
	UPROPERTY(VisibleDefaultsOnly, Category = Projectile)
	TSubobjectPtr<USphereComponent> CollisionComp;

	/** Projectile movement component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Movement)
	TSubobjectPtr<class UUTProjectileMovementComponent> ProjectileMovement;
	
	/** Used to get damage values */
	UPROPERTY(EditDefaultsOnly, Category = Projectile)
	TSubclassOf<class AUTProj_Redeemer> RedeemerProjectileClass;

	UFUNCTION()
	virtual void OnStop(const FHitResult& Hit);

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	virtual void Destroyed() override;
	virtual void BeginPlay() override;

	UPROPERTY()
	APawn* Driver;

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

	void FaceRotation(FRotator NewControlRotation, float DeltaTime) override;
	virtual void GetActorEyesViewPoint(FVector& out_Location, FRotator& out_Rotation) const override;
	void PawnStartFire(uint8 FireModeNum) override;

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

	UPROPERTY()
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

	bool bExploded;

	void ShutDown();

	void ExplodeStage(float RangeMultiplier);

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
};