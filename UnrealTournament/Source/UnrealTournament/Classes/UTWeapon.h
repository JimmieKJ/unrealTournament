// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeapon.generated.h"

USTRUCT(BlueprintType)
struct FInstantHitDamageInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageInfo")
	int32 Damage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageInfo")
	TSubclassOf<UDamageType> DamageType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageInfo")
	float Momentum;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageInfo")
	float TraceRange;
};

UCLASS(Blueprintable, Abstract)
class AUTWeapon : public AUTInventory
{
	GENERATED_UCLASS_BODY()

	friend class UUTWeaponState;
	friend class UUTWeaponStateInactive;
	friend class UUTWeaponStateActive;
	friend class UUTWeaponStateEquipping;
	friend class UUTWeaponStateUnequipping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	int32 Ammo;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	int32 MaxAmmo;
	/** ammo cost for one shot of each fire mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<int32> AmmoCost;
	/** projectile class for fire mode (if applicable) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray< TSubclassOf<AUTProjectile> > ProjClass;
	/** instant hit data for fire mode (if applicable) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<FInstantHitDamageInfo> InstantHitInfo;
	/** firing state for mode, contains core firing sequence and directs to appropriate global firing functions */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<class UUTWeaponStateFiring*> FiringState;
	/** time between shots, trigger checks, etc */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<float> FireInterval;
	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<USoundBase*> FireSound;
	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<UAnimMontage*> FireAnimation;
	/** particle component for muzzle flash */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TArray<UParticleSystemComponent*> MuzzleFlash;
	/** particle system for firing effects (instant hit beam and such)
	 * particles will be sourced at FireOffset and a parameter HitLocation will be set for the target, if applicable
	 */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TArray<UParticleSystem*> FireEffect;
	
	/** causes weapons fire to originate from the center of the player's view when in first person mode (and human controlled)
	 * in other cases the fire start point defaults to the weapon's world position
	 */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	bool bFPFireFromCenter;
	/** Firing offset from weapon for weapons fire. If bFPFireFromCenter is true and it's a player in first person mode, this is only used for visual effects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FVector FireOffset;

	/** time to bring up the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float BringUpTime;
	/** time to put down the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float PutDownTime;

	virtual void BeginPlay() OVERRIDE;

	void GotoState(class UUTWeaponState* NewState);

	/** firing entry point */
	virtual void StartFire(uint8 FireModeNum);
	virtual void StopFire(uint8 FireModeNum);
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerStartFire(uint8 FireModeNum);
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerStopFire(uint8 FireModeNum);

	virtual void BeginFiringSequence(uint8 FireModeNum);
	virtual void EndFiringSequence(uint8 FireModeNum);

	/** activates the weapon as part of a weapon switch
	 * (this weapon has already been set to Pawn->Weapon)
	 */
	virtual void BringUp();
	/** puts the weapon away as part of a weapon switch
	 * return false to prevent weapon switch (must keep this weapon equipped)
	 */
	virtual bool PutDown();
	UFUNCTION(BlueprintImplementableEvent)
	bool eventPreventPutDown();

	/** return number of fire modes */
	virtual uint8 GetNumFireModes()
	{
		return FMath::Min3(255, FiringState.Num(), FireInterval.Num());
	}

	virtual void ClientGivenTo_Internal(bool bAutoActivate) OVERRIDE;

	/** fires a shot and consumes ammo */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void FireShot();
	UFUNCTION(BlueprintImplementableEvent)
	bool FireShotOverride();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void PlayFiringEffects();
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void StopFiringEffects();

	/** return start point for weapons fire */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	virtual FVector GetFireStartLoc();
	/** return fire direction for weapons fire */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	virtual FRotator GetFireRotation();

	/** use up AmmoCost units of ammo for the current fire mode
	 * also handles triggering auto weapon switch if out of ammo
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void ConsumeAmmo(uint8 FireModeNum);
	
	virtual void FireInstantHit();
	virtual AUTProjectile* FireProjectile();

	/** returns whether we can meet AmmoCost for the given fire mode */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual bool HasAmmo(uint8 FireModeNum);

	/** get interval between shots, including any fire rate modifiers */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual float GetRefireTime(uint8 FireModeNum);

	inline uint8 GetCurrentFireMode()
	{
		return CurrentFireMode;
	}

	inline void GotoActiveState()
	{
		GotoState(ActiveState);
	}

	virtual void Tick(float DeltaTime) OVERRIDE;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	UUTWeaponState* CurrentState;
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	uint8 CurrentFireMode;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	TSubobjectPtr<UUTWeaponState> ActiveState;
	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	TSubobjectPtr<UUTWeaponState> EquippingState;
	UPROPERTY(Instanced, BlueprintReadOnly,  Category = "States")
	TSubobjectPtr<UUTWeaponState> UnequippingState;
	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	TSubobjectPtr<UUTWeaponState> InactiveState;
};