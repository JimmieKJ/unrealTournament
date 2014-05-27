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

	FInstantHitDamageInfo()
		: Damage(10), TraceRange(10000.0f)
	{}
};

UCLASS(Blueprintable, Abstract, NotPlaceable)
class AUTWeapon : public AUTInventory
{
	GENERATED_UCLASS_BODY()

	friend class UUTWeaponState;
	friend class UUTWeaponStateInactive;
	friend class UUTWeaponStateActive;
	friend class UUTWeaponStateEquipping;
	friend class UUTWeaponStateUnequipping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<class AUTWeaponAttachment> AttachmentType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, ReplicatedUsing=OnRep_Ammo, Category = "Weapon")
	int32 Ammo;
	/** handles weapon switch when out of ammo, etc
	 * NOTE: called on server if owner is locally controlled, on client only when owner is remote
	 */
	UFUNCTION()
	virtual void OnRep_Ammo();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Weapon")
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
	
	/** first person mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubobjectPtr<USkeletalMeshComponent> Mesh;

	/** causes weapons fire to originate from the center of the player's view when in first person mode (and human controlled)
	 * in other cases the fire start point defaults to the weapon's world position
	 */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	bool bFPFireFromCenter;
	/** Firing offset from weapon for weapons fire. If bFPFireFromCenter is true and it's a player in first person mode, this is from the camera center */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FVector FireOffset;

	/** time to bring up the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float BringUpTime;
	/** time to put down the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float PutDownTime;

	/** equip anims */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UAnimMontage* BringUpAnim;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UAnimMontage* PutDownAnim;

	/** weapon group - NextWeapon() picks the next highest group, PrevWeapon() the next lowest, etc
	 * generally, the corresponding number key is bound to access the weapons in that group
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	int32 Group;
	/** if the player acquires more than one weapon in a group, we assign a unique GroupSlot to keep a consistent order
	 * this value is only set on clients
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon")
	int32 GroupSlot;

	virtual void BeginPlay() OVERRIDE;
	virtual void RegisterAllComponents() OVERRIDE
	{
		// don't register in game by default for perf, we'll manually call Super from AttachToOwner()
		if (GetWorld()->WorldType == EWorldType::Editor || GetWorld()->WorldType == EWorldType::Preview)
		{
			Super::RegisterAllComponents();
		}
		else
		{
			RootComponent = NULL; // this was set for the editor view, but we don't want it
		}
	}

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
	/** allows blueprint to prevent the weapon from being switched away from */
	UFUNCTION(BlueprintImplementableEvent)
	bool eventPreventPutDown();

	/** attach the visuals to Owner's first person view */
	UFUNCTION(BlueprintNativeEvent)
	void AttachToOwner();
	/** detach the visuals from the Owner's first person view */
	UFUNCTION(BlueprintNativeEvent)
	void DetachFromOwner();

	/** return number of fire modes */
	virtual uint8 GetNumFireModes()
	{
		return FMath::Min3(255, FiringState.Num(), FireInterval.Num());
	}

	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate) OVERRIDE;
	virtual void ClientGivenTo_Internal(bool bAutoActivate) OVERRIDE;

	/** fires a shot and consumes ammo */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void FireShot();
	/** blueprint override for firing
	 * NOTE: do an authority check before spawning projectiles, etc as this function is called on both sides
	 */
	UFUNCTION(BlueprintImplementableEvent)
	bool FireShotOverride();

	/** play firing effects not associated with the shot's results (e.g. muzzle flash but generally NOT emitter to target) */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void PlayFiringEffects();
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void StopFiringEffects();

	/** play effects associated with the shot's impact given the impact point
	 * called only if FlashLocation has been set (instant hit weapon)
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void PlayImpactEffects(const FVector& TargetLoc);

	/** return start point for weapons fire */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	virtual FVector GetFireStartLoc();
	/** return fire direction for weapons fire */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	virtual FRotator GetFireRotation();

	/** add (or remove via negative number) the ammo held by the weapon */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void AddAmmo(int32 Amount);

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

	virtual void Destroyed() OVERRIDE;

	/** we added an editor tool to allow the user to set the MuzzleFlash entries to a component created in the blueprint components view,
	 * but the resulting instances won't be automatically set...
	 * so we need to manually hook it all up
	 * static so we can share with UTWeaponAttachment
	 */
	static void InstanceMuzzleFlashArray(AActor* Weap, TArray<UParticleSystemComponent*>& MFArray);

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