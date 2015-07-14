// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProjectileMovementComponent.h"

#include "UTDroppedPickup.generated.h"

/** a dropped, partially used inventory item that was previously held by a player
 * note that this is NOT a subclass of UTPickup
 */
UCLASS(NotPlaceable)
class UNREALTOURNAMENT_API AUTDroppedPickup : public AActor, public IUTResetInterface
{
	GENERATED_UCLASS_BODY()

protected:
	/** the item that this pickup represents, given to the Pawn that picks us up - SERVER ONLY */
	UPROPERTY(BlueprintReadOnly, Category = Pickup)
	AUTInventory* Inventory;
	/** same as Inventory->GetClass(), used to replicate visuals to clients */
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = InventoryTypeUpdated, Category = Pickup)
	TSubclassOf<AUTInventory> InventoryType;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = Pickup)
	UMeshComponent* Mesh;

	/** set after all needed properties are set for this pickup to grant its item (e.g. SetInventoryType() called in the case of the default implementation, may vary for subclasses) */
	UPROPERTY(BlueprintReadWrite)
	bool bFullyInitialized;
public:
	inline const UMeshComponent* GetMesh() const
	{
		return Mesh;
	}
	inline TSubclassOf<AUTInventory> GetInventoryType() const
	{
		return InventoryType;
	}
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Pickup)
	UCapsuleComponent* Collision;
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Pickup)
	UUTProjectileMovementComponent* Movement;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override
	{
		Movement->Velocity = NewVelocity;
	}

	virtual void Reset_Implementation() override
	{
		Destroy();
	}

	UFUNCTION(BlueprintNativeEvent)
	class USoundBase* GetPickupSound() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Inventory)
	virtual void SetInventory(AUTInventory* NewInventory);
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void InventoryTypeUpdated();

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	virtual void PhysicsStopped(const FHitResult& ImpactResult);

	UFUNCTION(BlueprintNativeEvent)
	bool AllowPickupBy(APawn* Other, bool bDefaultAllowPickup);
	UFUNCTION(BlueprintNativeEvent)
	void ProcessTouch(APawn* TouchedBy);

	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void GiveTo(APawn* Target);

	/** plays effects/audio for the pickup being taken
	 * NOTE: only called on server, so you need to make sure anything triggered here can replicate to clients
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void PlayTakenEffects(APawn* TakenBy);

	FTimerHandle EnableInstigatorTouchHandle;

	/** defer Instigator picking the item back up */
	UFUNCTION()
	void EnableInstigatorTouch();

	/** called to re-check overlapping actors, in case one was previously disallowed for some reason that may no longer be true (not fully initialized, obstruction removed, etc) */
	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void CheckTouching();

	virtual void Tick(float DeltaTime)
	{
		if (!bFullyInitialized)
		{
			bFullyInitialized = true;
			CheckTouching();
		}

		Super::Tick(DeltaTime);
	}

	/** returns how much the given Pawn (generally AI controlled) wants this item, where anything >= 1.0 is considered very important
	* note that it isn't necessary for this function to modify the weighting based on the path distance as that is handled internally;
	* the distance is supplied so that the code can make timing decisions and cost/benefit analysis
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = AI)
	float BotDesireability(APawn* Asker, float PathDistance);
	/** similar to BotDesireability but this method is queried for items along the bot's path during most pathing queries, even when it isn't explicitly looking for items
	* (e.g. checking to pick up health on the way to an enemy or game objective)
	* in general this method should be more strict and return 0 in cases where the bot's objective should be higher priority than the item
	* as with BotDesireability(), the PathDistance is weighted internally already and should primarily be used to reject things that are too far out of the bot's way
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = AI)
	float DetourWeight(APawn* Asker, float PathDistance);
};