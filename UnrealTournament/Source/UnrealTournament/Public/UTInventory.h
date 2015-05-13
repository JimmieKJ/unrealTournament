// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCharacter.h"
#include "UTHUDWidget.h"
#include "UTInventory.generated.h"

UCLASS(Blueprintable, Abstract, notplaceable, meta = (ChildCanTick))
class UNREALTOURNAMENT_API AUTInventory : public AActor
{
	GENERATED_UCLASS_BODY()

	friend void AUTCharacter::AddInventory(AUTInventory*, bool);
	friend void AUTCharacter::RemoveInventory(AUTInventory*);
	template<typename> friend class TInventoryIterator;

	virtual void PostInitProperties() override;
	virtual void PreInitializeComponents() override;

protected:
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Inventory")
	AUTInventory* NextInventory;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	AUTCharacter* UTOwner;

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
	void ClientGivenTo(APawn* NewInstigator, bool bAutoActivate);
	virtual void ClientGivenTo_Internal(bool bAutoActivate);
	/** called only on the client that is given the item */
	UFUNCTION(BlueprintImplementableEvent)
	void eventClientGivenTo(bool bAutoActivate);
	UFUNCTION(Client, Reliable)
	virtual void ClientRemoved();
	UFUNCTION(BlueprintImplementableEvent)
	void eventClientRemoved();

	FTimerHandle CheckPendingClientGivenToHandle;
	void CheckPendingClientGivenTo();
	virtual void OnRep_Instigator() override;

	uint32 bPendingClientGivenTo : 1;
	uint32 bPendingAutoActivate : 1;

	UPROPERTY(EditDefaultsOnly, Category = Pickup)
	UMeshComponent* PickupMesh;

public:
	/** returns next inventory item in the chain
	 * NOTE: usually you should use TInventoryIterator
	 */
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

	/** Initialized dropped pickup holding this inventory item. */
	virtual void InitializeDroppedPickup(class AUTDroppedPickup* Pickup);

	/** return a component that can be instanced to be applied to pickups */
	UFUNCTION(BlueprintNativeEvent)
	UMeshComponent* GetPickupMeshTemplate(FVector& OverrideScale) const;
	/** call AddOverlayMaterial() on the GRI to add any character or weapon overlay materials; this registration is required to replicate overlays */
	UFUNCTION(BlueprintNativeEvent)
	void AddOverlayMaterials(AUTGameState* GS) const;

	/** Human readable localized name for the item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
		FText DisplayName;

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
	/** sound played when owner takes damage (armor hit sound, for example) */
	UPROPERTY(EditDefaultsOnly, Category = Damage)
	USoundBase* ReceivedDamageSound;
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
	void ModifyDamageTaken(int32& Damage, FVector& Momentum, AUTInventory*& HitArmor, const FDamageEvent& DamageEvent, AController* InstigatedBy, AActor* DamageCauser); // TODO: UHT doesn't handle TSubclassOf<AUTInventory>&
	/** when a character takes damage that is reduced by inventory, the inventory item is included in the hit info and this function is called for all clients on the inventory DEFAULT OBJECT
	 * used to play shield/armor effects
	 * @return whether an effect was played
	 */
	UFUNCTION(BlueprintNativeEvent)
	bool PlayArmorEffects(AUTCharacter* HitPawn) const;

	/** Handles any C++ generated effects, calls blueprint PlayArmorEffects */
	virtual bool HandleArmorEffects(AUTCharacter* HitPawn) const;

	/** return true to prevent an incoming head shot
	* if bConsumeArmor is true, prevention should also consume the item (or a charge or whatever mechanic of degradation is being used)
	*/
	UFUNCTION(BlueprintNativeEvent)
	bool PreventHeadShot(FVector HitLocation, FVector ShotDirection, float WeaponHeadScaling, bool bConsumeArmor);
	/** return effective change in owner's health due to carrying this item
	 * for example, armor would return the amount of damage it could be expected to block
	 * this is used by AI as part of enemy evaluation
	 * if bOnlyVisible is true, return 0 if the item cannot be detected by only a visible inspection (i.e. would need to hit Owner to know it's there)
	 */
	UFUNCTION(BlueprintNativeEvent)
	int32 GetEffectiveHealthModifier(bool bOnlyVisible) const;

	UFUNCTION(BlueprintNativeEvent)
	void OwnerEvent(FName EventName);

	/** draws any relevant HUD that should be drawn whenever this item is held
	 * NOTE: not called by default, generally a HUD widget will call this for item types that are relevant for its area
	 */
	UFUNCTION(BlueprintNativeEvent)
	void DrawInventoryHUD(UUTHUDWidget* Widget, FVector2D Pos, FVector2D Size);

	/** base AI desireability for picking up this item on the ground */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = AI)
	float BasePickupDesireability;

	/** returns how much the given Pawn (generally AI controlled) wants this item (as pickup), where anything >= 1.0 is considered very important
	* called only when either the pickup is active now, or if timing is enabled and the pickup will become active by the time Asker can cross the distance
	* note that it isn't necessary for this function to modify the weighting based on the path distance as that is handled internally;
	* the distance is supplied so that the code can make timing decisions and cost/benefit analysis
	* note: this function is called on the default object, not a live instance
	* note: Pickup could be class UTPickup or class UTDroppedPickup
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = AI)
	float BotDesireability(APawn* Asker, AActor* Pickup, float PathDistance) const;
	/** similar to BotDesireability but this method is queried for items along the bot's path during most pathing queries, even when it isn't explicitly looking for items
	* (e.g. checking to pick up health on the way to an enemy or game objective)
	* in general this method should be more strict and return 0 in cases where the bot's objective should be higher priority than the item
	* as with BotDesireability(), the PathDistance is weighted internally already and should primarily be used to reject things that are too far out of the bot's way
	* note: this function is called on the default object, not a live instance
	* note: Pickup could be class UTPickup or class UTDroppedPickup
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = AI)
	float DetourWeight(APawn* Asker, AActor* Pickup, float PathDistance) const;

public:
	/** icon for drawing time remaining on the HUD */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = HUD)
	FCanvasIcon HUDIcon;

	/** Whether to show timer for this on spectator slide out HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = HUD)
		bool bShowPowerupTimer;

	/** How important is this inventory item when rendering a group of them */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = HUD)
	float HUDRenderPriority;

	/** Returns the HUD text to display for this item */
	virtual FText GetHUDText() 
	{ 
		return FText::GetEmpty(); 
	}

	// Allows inventory items to decide if a widget should be allowed to render them.
	virtual bool HUDShouldRender(UUTHUDWidget* TargetWidget) 
	{
		return false;
	}

	/** Used to initially flash icon if HUD rendered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		float InitialFlashTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		float InitialFlashScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		FLinearColor InitialFlashColor;

	float FlashTimer;

public:
	// These are different from the menubar icons and are used in some of the menus.

	// Holds a reference to the 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	UTexture2D* MenuGraphic;

	// This is the long description that will be displayed in the menu.  
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	FText MenuDescription;

};

// template to access a character's inventory
// this class automatically handles ignoring invalid (not fully replicated or initialized) inventory items
// it is also resilient against the currently iterated item being destroyed
template<typename InvType = AUTInventory> class UNREALTOURNAMENT_API TInventoryIterator
{
private:
	const AUTCharacter* Holder;
	AUTInventory* CurrentInv;
	AUTInventory* NextInv;
	int32 Count;

	inline bool IsValidForIteration(AUTInventory* TestInv)
	{
		return (TestInv->GetOwner() == Holder && TestInv->GetUTOwner() == Holder && (/*typeid(InvType) == typeid(AUTInventory) || */TestInv->IsA(InvType::StaticClass())));
	}
public:
	TInventoryIterator(const AUTCharacter* InHolder)
		: Holder(InHolder), Count(0)
	{
		if (Holder != NULL)
		{
			CurrentInv = Holder->InventoryList;
			if (CurrentInv != NULL)
			{
				NextInv = CurrentInv->NextInventory;
				if (!IsValidForIteration(CurrentInv))
				{
					++(*this);
				}
			}
			else
			{
				NextInv = NULL;
			}
		}
		else
		{
			CurrentInv = NULL;
			NextInv = NULL;
		}
	}
	void operator++()
	{
		do
		{
			// bound number of items to avoid infinite loops on clients in edge cases with partial replication
			// we could keep track of all the items we've iterated but that's more expensive and probably not worth it
			Count++;
			if (Count > 100)
			{
				CurrentInv = NULL;
			}
			else
			{
				CurrentInv = NextInv;
				if (CurrentInv != NULL)
				{
					NextInv = CurrentInv->NextInventory;
				}
			}
		} while (CurrentInv != NULL && !IsValidForIteration(CurrentInv));
	}
	FORCEINLINE bool IsValid() const
	{
		return CurrentInv != NULL;
	}
	FORCEINLINE operator bool() const
	{
		return IsValid();
	}
	FORCEINLINE InvType* operator*() const
	{
		checkSlow(CurrentInv != NULL && CurrentInv->IsA(InvType::StaticClass()));
		return (InvType*)CurrentInv;
	}
	FORCEINLINE InvType* operator->() const
	{
		checkSlow(CurrentInv != NULL && CurrentInv->IsA(InvType::StaticClass()));
		return (InvType*)CurrentInv;
	}
};