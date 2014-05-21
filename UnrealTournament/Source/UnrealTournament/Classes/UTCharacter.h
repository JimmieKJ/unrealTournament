// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacter.generated.h"

USTRUCT(BlueprintType)
struct FTakeHitInfo
{
	GENERATED_USTRUCT_BODY()

	/** the amount of damage */
	UPROPERTY(BlueprintReadWrite, Category = TakeHitInfo)
	int32 Damage;
	/** the location of the hit (relative to Pawn center) */
	UPROPERTY(BlueprintReadWrite, Category = TakeHitInfo)
	FVector_NetQuantize RelHitLocation;
	/** how much momentum was imparted */
	UPROPERTY(BlueprintReadWrite, Category = TakeHitInfo)
	FVector_NetQuantize Momentum;
	/** the damage type we were hit with */
	UPROPERTY(BlueprintReadWrite, Category = TakeHitInfo)
	TSubclassOf<UDamageType> DamageType;
};

/** ammo counter */
USTRUCT(BlueprintType)
struct FStoredAmmo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Ammo)
	TSubclassOf<class AUTWeapon> Type;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Ammo)
	int32 Amount;
};

UCLASS(config=Game)
class AUTCharacter : public ACharacter
{
	GENERATED_UCLASS_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	TSubobjectPtr<class USkeletalMeshComponent> FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	TSubobjectPtr<class UCameraComponent> CharacterCameraComponent;

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

	/** counters of ammo for which the pawn doesn't yet have the corresponding weapon in its inventory */
	UPROPERTY()
	TArray<FStoredAmmo> SavedAmmo;

	UFUNCTION(BlueprintCallable, Category = Pawn)
	virtual void AddAmmo(const FStoredAmmo& AmmoToAdd);

	inline class AUTInventory* GetInventory()
	{
		return InventoryList;
	}

	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void AddInventory(AUTInventory* InvToAdd, bool bAutoActivate);

	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void RemoveInventory(AUTInventory* InvToRemove);

	/** find an inventory item of a specified type */
	UFUNCTION(BlueprintCallable, Category = Pawn)
	virtual AUTInventory* K2_FindInventoryType(TSubclassOf<AUTInventory> Type, bool bExactClass = false);

	template<typename InvClass>
	inline InvClass* FindInventoryType(TSubclassOf<InvClass> Type, bool bExactClass = false)
	{
		return CastChecked<InvClass>(K2_FindInventoryType(Type, bExactClass));
	}

	/** toss an inventory item in the direction the player is facing
	 * (the inventory must have a pickup defined)
	 */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void TossInventory(AUTInventory* InvToToss);

	/** discards (generally destroys) all inventory items */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void DiscardAllInventory();

	virtual void CheckAutoWeaponSwitch(AUTWeapon* TestWeapon);

	/** switches weapons; handles client/server sync, safe to call on either side */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void SwitchWeapon(AUTWeapon* NewWeapon);

	inline bool IsPendingFire(uint8 FireMode) const
	{
		return (FireMode < PendingFire.Num() && PendingFire[FireMode] != 0);
	}
	/** sets the pending fire flag; generally should be called by whatever weapon processes the firing command, unless it's an explicit single shot */
	inline void SetPendingFire(uint8 FireMode, bool bNowFiring)
	{
		if (PendingFire.Num() < FireMode + 1)
		{
			PendingFire.SetNumZeroed(FireMode + 1);
		}
		PendingFire[FireMode] = bNowFiring ? 1 : 0;
	}

	inline AUTWeapon* GetWeapon() const
	{
		return Weapon;
	}
	inline AUTWeapon* GetPendingWeapon() const
	{
		return PendingWeapon;
	}

	bool IsInInventory(const AUTInventory* TestInv) const;

	/** called by weapon being put down when it has finished being unequipped. Transition PendingWeapon to Weapon and bring it up */
	virtual void WeaponChanged();

	virtual void PossessedBy(AController* NewController);

	/** replicated weapon firing info */
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = FiringInfoUpdated, Category = "Weapon")
	uint8 FlashCount;
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weapon")
	uint8 FireMode;
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = FiringInfoUpdated, Category = "Weapon")
	FVector_NetQuantize FlashLocation;

	/** set info for one instance of firing and plays firing effects; assumed to be a valid shot - call ClearFiringInfo() if the weapon has stopped firing
	 * if a location is not needed (projectile) call IncrementFlashCount() instead
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void SetFlashLocation(const FVector& InFlashLoc, uint8 InFireMode);
	/** set info for one instance of firing and plays firing effects; assumed to be a valid shot - call ClearFiringInfo() if the weapon has stopped firing
	* if a location is needed (instant hit, beam, etc) call SetFlashLocation() instead
	*/
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void IncrementFlashCount(uint8 InFireMode);
	/** clears firing variables; i.e. because the weapon has stopped firing */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void ClearFiringInfo();

	/** called when firing variables are updated to trigger/stop effects */
	UFUNCTION()
	virtual void FiringInfoUpdated();

	UPROPERTY(BlueprintReadWrite, Category = Pawn, Replicated)
	int32 Health;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Pawn)
	int32 HealthMax;

	UPROPERTY(BlueprintReadWrite, Category = Pawn, Replicated, ReplicatedUsing=PlayTakeHitEffects)
	FTakeHitInfo LastTakeHitInfo;
	/** time of last SetLastTakeHitInfo() - authority only */
	UPROPERTY(BlueprintReadOnly, Category = Pawn)
	float LastTakeHitTime;

	virtual void BeginPlay() OVERRIDE;

	virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) OVERRIDE;

	/** Set LastTakeHitInfo from a damage event and call PlayTakeHitEffects() */
	virtual void SetLastTakeHitInfo(int32 Damage, const FDamageEvent& DamageEvent);

	/** TEMP blood effect until we have a better hit effects system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	UParticleSystem* BloodEffect;

	/** plays clientside hit effects using the data previously stored in LastTakeHitInfo */
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void PlayTakeHitEffects();

	/** called when we die (generally, by running out of health)
	 *  SERVER ONLY - do not do visual effects here!
	 * return true if we can die, false if immortal (gametype effect, powerup, mutator, etc)
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Pawn)
	virtual bool Died(AController* EventInstigator, const FDamageEvent& DamageEvent);

	/** plays death effects; use LastTakeHitInfo to do damage-specific death effects */
	virtual void PlayDying();
	virtual void AddDefaultInventory(TArray<TSubclassOf<AUTInventory>> DefaultInventoryToAdd);

	UFUNCTION(BlueprintCallable, Category = Pawn)
	bool IsDead();

	/** weapon firing */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void StartFire(uint8 FireModeNum);
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void StopFire(uint8 FireModeNum);

	virtual void UnPossessed();

	virtual void PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker);

protected:

	/** switches weapon locally, must execute independently on both server and client */
	virtual void LocalSwitchWeapon(AUTWeapon* NewWeapon);
	/** RPC to do weapon switch */
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSwitchWeapon(AUTWeapon* NewWeapon);
	UFUNCTION(Client, Reliable)
	virtual void ClientSwitchWeapon(AUTWeapon* NewWeapon);

	// firemodes with input currently being held down (pending or actually firing)
	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	TArray<uint8> PendingFire;

	/** Handles moving forward/backward */
	void MoveForward(float Val);

	/** Handles stafing movement, left and right */
	void MoveRight(float Val);

	/**
	 * Called via input to turn at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Pawn")
	AUTInventory* InventoryList;

	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	AUTWeapon* PendingWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	class AUTWeapon* Weapon;

	UPROPERTY(EditAnywhere, Category = "Pawn")
	TArray<TSubclassOf<AUTInventory> > DefaultCharacterInventory;

	/** default weapon - TODO: should be in gametype */
	UPROPERTY(EditAnywhere, Category = "Pawn")
	TSubclassOf<AUTWeapon> DefaultWeapon;

	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* InputComponent) OVERRIDE;
	// End of APawn interface

};

inline bool AUTCharacter::IsDead()
{
	return bTearOff || bPendingKillPending;
}

