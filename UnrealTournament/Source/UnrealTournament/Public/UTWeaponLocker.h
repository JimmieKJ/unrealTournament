// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPickup.h"
#include "UTInventory.h"
#include "UTPickupWeapon.h"

#include "UTWeaponLocker.generated.h"

USTRUCT(BlueprintType)
struct FWeaponLockerItem
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AUTWeapon> WeaponType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ExtraAmmo;
};

UCLASS(Abstract)
class AUTWeaponLocker : public AUTPickup
{
	GENERATED_BODY()
public:

	AUTWeaponLocker(const FObjectInitializer& OI);

	/** where weapon meshes are placed; there can be more weapons than this in the locker but extras won't be displayed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FTransform> WeaponPlacements;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = WeaponListUpdated)
	TArray<FWeaponLockerItem> WeaponList;

	UPROPERTY(BlueprintReadOnly)
	TArray<UMeshComponent*> WeaponMeshes;

	UPROPERTY(BlueprintReadOnly)
		class AUTGameVolume* MyGameVolume;

	UFUNCTION(BlueprintCallable, Category = WeaponLocker)
	void SetWeaponList(TArray<FWeaponLockerItem> InList);

	UFUNCTION()
	virtual void WeaponListUpdated();

	/* Handle turning off this weapon locker due to touch by TouchedBy*/
	virtual void DisabledByTouchBy(APawn* TouchedBy);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** list of characters that have picked up this weapon recently, used when weapon stay is on to avoid repeats */
	UPROPERTY(BlueprintReadWrite)
	TArray<FWeaponPickupCustomer> Customers;

	/** whether the pickup is taken (unavailable) for this pawn */
	virtual bool IsTaken(APawn* TestPawn) override;

	virtual void ProcessTouch_Implementation(APawn* TouchedBy) override;

	FTimerHandle CheckTouchingHandle;

	/** checks for anyone touching the pickup and checks if they should get the item
	* this is necessary because this type of pickup doesn't toggle collision
	*/
	void CheckTouching();

	virtual float GetRespawnTimeOffset(APawn* Asker) const override;

	virtual void GiveTo_Implementation(APawn* Target) override;
	virtual void SetPickupHidden(bool bNowHidden) override;
	virtual void PlayTakenEffects(bool bReplicate) override;

	virtual void AddHiddenComponents(bool bTaken, TSet<FPrimitiveComponentId>& HiddenComponents) override
	{
		Super::AddHiddenComponents(bTaken, HiddenComponents);
		if (bTaken)
		{
			for (UMeshComponent* WeapMesh : WeaponMeshes)
			{
				if (WeapMesh != NULL)
				{
					HiddenComponents.Add(WeapMesh->ComponentId);
				}
			}
		}
	}

	virtual float BotDesireability_Implementation(APawn* Asker, AController* RequestOwner, float TotalDistance) override;
	virtual float DetourWeight_Implementation(APawn* Asker, float TotalDistance) override;
	virtual bool IsSuperDesireable_Implementation(AController* RequestOwner, float CalculatedDesire) override
	{
		return false;
	}
};