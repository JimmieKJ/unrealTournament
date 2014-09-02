// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCharacter.h"

#include "UTInventory.generated.h"

UCLASS(Blueprintable, Abstract, notplaceable, meta = (ChildCanTick))
class UNREALTOURNAMENT_API AUTInventory : public AActor
{
	GENERATED_UCLASS_BODY()

	friend void AUTCharacter::AddInventory(AUTInventory*, bool);
	friend void AUTCharacter::RemoveInventory(AUTInventory*);

protected:
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Inventory")
	AUTInventory* NextInventory;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	AUTCharacter* UTOwner;

	virtual void PostInitProperties() override;
	virtual void PreInitializeComponents() override;

	/** called when this inventory item has been given to the specified character */
	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly)
	void eventGivenTo(AUTCharacter* NewOwner, bool bAutoActivate);
	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate);
	/** called when this inventory item has been removed from its owner */
	UFUNCTION(BlueprintImplementableEvent)
	void eventRemoved();
	virtual void Removed();

	/** client side handling of owner transition */
	UFUNCTION(Client, Reliable)
	void ClientGivenTo(bool bAutoActivate);
	virtual void ClientGivenTo_Internal(bool bAutoActivate);
	/** called only on the client that is given the item */
	UFUNCTION(BlueprintImplementableEvent)
	void eventClientGivenTo(bool bAutoActivate);
	UFUNCTION(Client, Reliable)
	virtual void ClientRemoved();
	UFUNCTION(BlueprintImplementableEvent)
	void eventClientRemoved();

	void CheckPendingClientGivenTo();
	virtual void OnRep_Instigator() override;

	uint32 bPendingClientGivenTo : 1;
	uint32 bPendingAutoActivate : 1;

	UPROPERTY(EditDefaultsOnly, Category = Pickup)
	UMeshComponent* PickupMesh;

public:
	AUTInventory* GetNext() const
	{
		return NextInventory;
	}
	AUTCharacter* GetUTOwner() const
	{
		checkSlow(UTOwner == GetOwner() || Role < ROLE_Authority); // on client RPC to assign UTOwner could be delayed
		return UTOwner;
	}
	virtual void DropFrom(const FVector& StartLocation, const FVector& TossVelocity);
	virtual void Destroyed() override;

	/** return a component that can be instanced to be applied to pickups */
	UFUNCTION(BlueprintNativeEvent)
	UMeshComponent* GetPickupMeshTemplate(FVector& OverrideScale) const;
	/** call AddOverlayMaterial() on the GRI to add any character or weapon overlay materials; this registration is required to replicate overlays */
	UFUNCTION(BlueprintNativeEvent)
	void AddOverlayMaterials(AUTGameState* GS) const;

	/** respawn time for level placed pickups of this type */
	UPROPERTY(EditDefaultsOnly, Category = Pickup)
	float RespawnTime;
	/** if set, item starts off not being available when placed in the level (must wait RespawnTime from start of match) */
	UPROPERTY(EditDefaultsOnly, Category = Pickup)
	bool bDelayedSpawn;
	/** if set, item is always dropped when its holder dies if uses/charges/etc remain */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Inventory)
	bool bAlwaysDropOnDeath;
	/** sound played on pickup */
	UPROPERTY(EditDefaultsOnly, Category = Pickup)
	USoundBase* PickupSound;
	/** class used when this item is dropped by its holder */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Pickup)
	TSubclassOf<class AUTDroppedPickup> DroppedPickupClass;

	/** called by pickups when another inventory of same class will be given, allowing the item to simply stack instance values
	 * instead of spawning a new item
	 * ContainedInv may be NULL if it's a pickup that spawns new instead of containing a partially used existing item
	 * return true to prevent giving/spawning a new inventory item
	 */
	UFUNCTION(BlueprintNativeEvent)
	bool StackPickup(AUTInventory* ContainedInv);

	/** if set, inventory gets the ModifyDamageTaken() and PreventHeadShot() functions/events when the holder takes damage */
	UPROPERTY(EditDefaultsOnly, Category = Events)
	uint32 bCallDamageEvents : 1;
	/** if set, receive OwnerEvent() calls for various holder events (jump, land, fire, etc) */
	UPROPERTY(EditDefaultsOnly, Category = Events)
	uint32 bCallOwnerEvent : 1;

	UFUNCTION(BlueprintNativeEvent)
	void ModifyDamageTaken(int32& Damage, FVector& Momentum, bool& bHitArmor, const FDamageEvent& DamageEvent, AController* InstigatedBy, AActor* DamageCauser);
	/** return true to prevent an incoming head shot
	* if bConsumeArmor is true, prevention should also consume the item (or a charge or whatever mechanic of degradation is being used)
	*/
	UFUNCTION(BlueprintNativeEvent)
	bool PreventHeadShot(FVector HitLocation, FVector ShotDirection, float WeaponHeadScaling, bool bConsumeArmor);

	UFUNCTION(BlueprintNativeEvent)
	void OwnerEvent(FName EventName);

	/** draws any relevant HUD that should be drawn whenever this item is held
	 * NOTE: not called by default, generally a HUD widget will call this for item types that are relevant for its area
	 */
	UFUNCTION(BlueprintNativeEvent)
	void DrawInventoryHUD(UUTHUDWidget* Widget, FVector2D Pos, FVector2D Size);
};
