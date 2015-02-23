// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeap_Enforcer.generated.h"

UCLASS(abstract)
class AUTWeap_Enforcer : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<class AUTWeaponAttachment> SingleWieldAttachmentType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<class AUTWeaponAttachment> DualWieldAttachmentType;

	/** Left hand weapon mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	USkeletalMeshComponent* LeftMesh;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	UUTWeaponStateEquipping* EnforcerEquippingState;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	UUTWeaponState* EnforcerUnequippingState;

	/** How much spread increases for each shot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Enforcer)
	float SpreadIncrease;

	/** How much spread increases for each shot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Enforcer)
	float SpreadResetInterval;

	/** Max spread  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Enforcer)
	float MaxSpread;

	/** Last time a shot was fired (used for calculating spread). */
	UPROPERTY(BlueprintReadWrite, Category = Enforcer)
	float LastFireTime;

	/** Stopping power against players with melee weapons.  Overrides normal momentum imparted by bullets. */
	UPROPERTY(BlueprintReadWrite, Category = Enforcer)
	float StoppingPower;

	/** Left hand mesh equip anims */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UAnimMontage* LeftBringUpAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.1"))
	TArray<float> FireIntervalDualWield;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<float> SpreadDualWield;

	/** Weapon bring up time when dual wielding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
		float DualBringUpTime;

	/** Weapon put down time when dual wielding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
		float DualPutDownTime;

	virtual float GetBringUpTime() override;
	virtual float GetPutDownTime() override;

	UPROPERTY()
	FVector FirstPLeftMeshOffset;
	UPROPERTY()
	FRotator FirstPLeftMeshRotation;
		
	UPROPERTY()
	int32 FireCount;

	UPROPERTY()
	int32 ImpactCount;

	UPROPERTY()
	bool bDualEnforcerMode;

	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = BecomeDual, Category = "Weapon")
	bool bBecomeDual;

	UPROPERTY(EditAnywhere, Category = "Weapon")
	TArray<UParticleSystemComponent*> LeftMuzzleFlash;

	/**Update the left hand mesh positioning*/
	virtual void Tick(float DeltaTime) override;

	virtual void PlayFiringEffects() override;
	virtual void FireInstantHit(bool bDealDamage, FHitResult* OutHit) override;
	virtual bool StackPickup_Implementation(AUTInventory* ContainedInv) override; 
	virtual void BringUp(float OverflowTime) override;
	virtual void PlayImpactEffects(const FVector& TargetLoc, uint8 FireMode, const FVector& SpawnLocation, const FRotator& SpawnRotation) override;
	virtual void UpdateOverlays() override;
	virtual void SetSkin(UMaterialInterface* NewSkin) override;
	virtual void GotoEquippingState(float OverflowTime) override;
	virtual void FireShot() override;
	virtual void StateChanged() override;

	/** Switch to second enforcer mode
	*/
	UFUNCTION()
	virtual void BecomeDual();
	virtual	float GetImpartedMomentumMag(AActor* HitActor) override;
	virtual void DetachFromOwnerNative() override;
	virtual void AttachToOwnerNative() override;

	virtual void DualEquipFinished();

protected:
	
	UPROPERTY()
	USkeletalMeshComponent* LeftOverlayMesh;

	virtual void AttachLeftMesh();
};

