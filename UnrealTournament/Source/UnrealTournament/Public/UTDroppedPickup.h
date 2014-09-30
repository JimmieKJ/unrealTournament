// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProjectileMovementComponent.h"

#include "UTDroppedPickup.generated.h"

/** a dropped, partially used inventory item that was previously held by a player
 * note that this is NOT a subclass of UTPickup
 */
UCLASS(NotPlaceable)
class UNREALTOURNAMENT_API AUTDroppedPickup : public AActor
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
public:
	inline const UMeshComponent* GetMesh()
	{
		return Mesh;
	}
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Pickup)
	TSubobjectPtr<UCapsuleComponent> Collision;
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Pickup)
	TSubobjectPtr<UUTProjectileMovementComponent> Movement;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override
	{
		Movement->Velocity = NewVelocity;
	}

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Inventory)
	virtual void SetInventory(AUTInventory* NewInventory);
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void InventoryTypeUpdated();

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	virtual void PhysicsStopped(const FHitResult& ImpactResult);

	UFUNCTION(BlueprintNativeEvent)
	void ProcessTouch(APawn* TouchedBy);

	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void GiveTo(APawn* Target);

	/** plays effects/audio for the pickup being taken
	 * NOTE: only called on server, so you need to make sure anything triggered here can replicate to clients
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void PlayTakenEffects(APawn* TakenBy);

	/** defer Instigator picking the item back up */
	UFUNCTION()
	void EnableInstigatorTouch();

	/** returns how much the given Pawn (generally AI controlled) wants this item, where anything >= 1.0 is considered very important
	* note that it isn't necessary for this function to modify the weighting based on the path distance as that is handled internally;
	* the distance is supplied so that the code can make timing decisions and cost/benefit analysis
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = AI)
	float BotDesireability(APawn* Asker, float PathDistance);
};