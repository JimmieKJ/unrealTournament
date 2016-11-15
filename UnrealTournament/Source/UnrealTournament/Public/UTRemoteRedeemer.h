// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProj_Redeemer.h"
#include "Classes/Kismet/KismetMaterialLibrary.h"
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

	/** Sound played when player targeting information first appears on HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lock)
	USoundBase*  			LockAcquiredSound;

	/** Sound played when redeemer receives damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Redeemer)
		USoundBase* HitSound;

	UPROPERTY()
	int32 LockCount;

	UPROPERTY()
		int32 KillCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Redeemer)
		float MaxFuelTime;

	UPROPERTY()
		float CurrentFuelTime;

	uint8 CachedTeamNum;

protected:
	/** for outline rendering */
	UPROPERTY()
	UMeshComponent* CustomDepthMesh;
public:

	UFUNCTION()
	virtual void OnStop(const FHitResult& Hit);

	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	UFUNCTION()
	virtual void ExplodeTimed();

	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	virtual void Destroyed() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnRep_PlayerState() override;

	UPROPERTY(BlueprintReadOnly, Category = Vehicle)
	APawn* Driver;

	/** Controller that gets credit for damage, since main Controller will be lost due to driver leaving when we blow up */
	UPROPERTY(BlueprintReadOnly, Category = Projectile)
	AController* DamageInstigator;

	UFUNCTION()
	virtual bool TryToDrive(APawn* NewDriver);

	UFUNCTION()
	virtual bool DriverEnter(APawn* NewDriver);

	UFUNCTION()
	virtual bool DriverLeave(bool bForceLeave);
	
	UFUNCTION()
	void BlowUp(FVector HitNormal=FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual uint8 GetTeamNum() const;
	// not applicable
	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum) override
	{}

	void FaceRotation(FRotator NewControlRotation, float DeltaTime) override;
	virtual void GetActorEyesViewPoint(FVector& out_Location, FRotator& out_Rotation) const override;
	void PawnStartFire(uint8 FireModeNum) override;
	virtual void BeginPlay() override;

	/** material to draw over the player's view when zoomed
	* the material can use the parameter 'TeamColor' to receive the player's team color in team games (won't be changed in FFA modes)
	*/
	UPROPERTY(EditDefaultsOnly, Category = Overlay)
	UMaterialInterface* OverlayMat;
	/** material instance for e.g. team coloring */
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* OverlayMI;
	/** curve for static ramp up on overlay when exploding */
	UPROPERTY(EditDefaultsOnly, Category = Overlay)
	UCurveFloat* OverlayStaticCurve;

	virtual UMaterialInterface* GetPostProcessMaterial()
	{
		if (OverlayMat != NULL)
		{
			if (OverlayMI == NULL)
			{
				OverlayMI = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, OverlayMat);
			}
			return OverlayMI;
		}
		else
		{
			return NULL;
		}
	}

	/** material for drawing enemy indicators */
	UPROPERTY(EditDefaultsOnly, Category = Overlay)
		UTexture2D* TargetIndicator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overlay)
		UTexture2D* RedeemerDisplayOne;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overlay)
		USoundBase* FuelWarningSound;

	virtual void PostRender(class AUTHUD* HUD, UCanvas* Canvas);

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerBlowUp();

	UPROPERTY()
		float StatsHitCredit;

	UPROPERTY()
		FName HitsStatsName;

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
		int32 ProjHealth;

	UPROPERTY()
	float RollSmoothingMultiplier;

	UPROPERTY()
		FVector ExplosionCenter;

	bool bExploded;
	/** set when we were shot down by an enemy instead of exploding on contact with the big boom */
	UPROPERTY(Replicated)
	bool bShotDown;

	virtual void TornOff() override;

	void ShutDown();

	void ExplodeStage(float RangeMultiplier);

	/** Create effects for full nuclear blast. */
	virtual void PlayExplosionEffects();

	/** Play/stop effects for shot down Redeemer that explodes after falling out of the sky */
	virtual void PlayShotDownEffects();

	virtual void OnShotDown();

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
	
	virtual void PawnClientRestart() override;

	UFUNCTION(BlueprintImplementableEvent, Category=Pawn)
	void RedeemerRestarted(AController* NewController);

	virtual void DrawTexture(UCanvas* Canvas, UTexture* Texture, float X, float Y, float Width, float Height, float U, float V, float UL, float VL, float DrawOpacity, FLinearColor DrawColor, FVector2D RenderOffset, float Rotation=0.f, FVector2D RotPivot=FVector2D(0.f,0.f));
};