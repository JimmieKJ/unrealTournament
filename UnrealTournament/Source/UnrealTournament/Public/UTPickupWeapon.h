// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPickupInventory.h"
#include "UTWeapon.h"

#include "UTPickupWeapon.generated.h"

USTRUCT(BlueprintType)
struct FWeaponPickupCustomer
{
	GENERATED_USTRUCT_BODY()

	/** the pawn that picked it up */
	UPROPERTY(BlueprintReadWrite, Category = Customer)
	APawn* P;
	/** next time pickup is allowed */
	UPROPERTY(BlueprintReadWrite, Category = Customer)
	float NextPickupTime;

	FWeaponPickupCustomer()
	{}
	FWeaponPickupCustomer(APawn* InP, float InPickupTime)
		: P(InP), NextPickupTime(InPickupTime)
	{}
};

UCLASS(Blueprintable, CustomConstructor, HideCategories=(Inventory, Pickup))
class UNREALTOURNAMENT_API AUTPickupWeapon : public AUTPickupInventory
{
	GENERATED_UCLASS_BODY()

	AUTPickupWeapon(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	/** weapon type that can be picked up here */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Weapon)
	TSubclassOf<AUTWeapon> WeaponType;

	/** list of characters that have picked up this weapon recently, used when weapon stay is on to avoid repeats */
	UPROPERTY(BlueprintReadWrite, Category = PickupWeapon)
	TArray<FWeaponPickupCustomer> Customers;

	/** whether the pickup is taken (unavailable) for this pawn */
	UFUNCTION(BlueprintCallable, Category = Pickup)
	bool IsTaken(APawn* TestPawn);

	virtual void BeginPlay() override;
	virtual void SetInventoryType(TSubclassOf<AUTInventory> NewType) override;

	virtual void ProcessTouch_Implementation(APawn* TouchedBy) override;
};