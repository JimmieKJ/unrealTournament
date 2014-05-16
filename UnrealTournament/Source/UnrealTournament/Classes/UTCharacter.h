// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacter.generated.h"

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

	inline class AUTInventory* GetInventory()
	{
		return InventoryList;
	}

	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void AddInventory(AUTInventory* InvToAdd, bool bAutoActivate);

	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void RemoveInventory(AUTInventory* InvToRemove);

	virtual void CheckAutoWeaponSwitch(AUTWeapon* TestWeapon);

	/** switches weapons; handles client/server sync, safe to call on either side */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void SwitchWeapon(AUTWeapon* NewWeapon);

	inline bool IsPendingFire(uint8 FireMode) const
	{
		return (FireMode < PendingFire.Num() && PendingFire[FireMode] != 0);
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
	FVector FlashLocation;

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

protected:

	/** switches weapon locally, must execute independently on both server and client */
	virtual void LocalSwitchWeapon(AUTWeapon* NewWeapon);
	/** RPC to do weapon switch */
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSwitchWeapon(AUTWeapon* NewWeapon);
	UFUNCTION(Client, Reliable)
	virtual void ClientSwitchWeapon(AUTWeapon* NewWeapon);

	/** Handler for a touch input beginning. */
	void TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location);

	/** weapon fire input handling */
	void OnFire();
	void OnStopFire();
	void OnAltFire();
	void OnStopAltFire();

	// firemodes with input currently being held down (pending or actually firing)
	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	TArray<uint8> PendingFire;

	/** weapon firing */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void StartFire(uint8 FireModeNum);
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void StopFire(uint8 FireModeNum);

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

	/** default weapon - TODO: should be in gametype */
	UPROPERTY(EditAnywhere, Category = "Pawn")
	TSubclassOf<AUTWeapon> DefaultWeapon;

	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* InputComponent) OVERRIDE;
	// End of APawn interface
};

