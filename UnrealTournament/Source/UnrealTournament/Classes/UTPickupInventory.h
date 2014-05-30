// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPickupInventory.generated.h"

UCLASS(Blueprintable, CustomConstructor)
class AUTPickupInventory : public AUTPickup
{
	GENERATED_UCLASS_BODY()

	AUTPickupInventory(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, ReplicatedUsing=InventoryTypeUpdated, Category = Inventory)
	TSubclassOf<AUTInventory> InventoryType;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = Pickup)
	UMeshComponent* Mesh;
public:

	inline const UMeshComponent* GetMesh()
	{
		return Mesh;
	}

	virtual void BeginPlay() OVERRIDE;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif

	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void SetInventoryType(TSubclassOf<AUTInventory> NewType);
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void InventoryTypeUpdated();

	virtual void ProcessTouch_Implementation(APawn* TouchedBy) OVERRIDE;
	virtual void GiveTo_Implementation(APawn* Target) OVERRIDE;
	virtual void SetPickupHidden(bool bNowHidden) OVERRIDE;
};