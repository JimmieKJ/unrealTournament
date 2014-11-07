// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPickup.h"
#include "UTInventory.h"

#include "UTPickupInventory.generated.h"

UCLASS(Blueprintable)
class UNREALTOURNAMENT_API AUTPickupInventory : public AUTPickup
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, ReplicatedUsing=InventoryTypeUpdated, Category = Inventory)
	TSubclassOf<AUTInventory> InventoryType;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = Pickup)
	UMeshComponent* Mesh;
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	UMeshComponent* EditorMesh;
#endif
public:
	inline const UMeshComponent* GetMesh()
	{
		return Mesh;
	}

	/** contains code shared between placed and dropped pickups for initializing Mesh given an InventoryType */
	static void CreatePickupMesh(AActor* Pickup, UMeshComponent*& PickupMesh, TSubclassOf<AUTInventory> PickupInventoryType, float MeshFloatHeight);

	/** how high the pickup floats (additional Z axis translation applied to pickup mesh) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Pickup)
	float FloatHeight;

	virtual void BeginPlay() override;
#if WITH_EDITOR
	/** create transient pickup mesh for editor previewing */
	virtual void CreateEditorPickupMesh();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
#endif

	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void SetInventoryType(TSubclassOf<AUTInventory> NewType);
	inline TSubclassOf<AUTInventory> GetInventoryType()
	{
		return InventoryType;
	}
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void InventoryTypeUpdated();

	virtual bool AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup) override;
	virtual void GiveTo_Implementation(APawn* Target) override;
	virtual void SetPickupHidden(bool bNowHidden) override;

	virtual float BotDesireability_Implementation(APawn* Asker, float TotalDistance) override
	{
		return (InventoryType == NULL) ? 0.0f : InventoryType.GetDefaultObject()->BotDesireability(Asker, this, TotalDistance);
	}
	virtual float DetourWeight_Implementation(APawn* Asker, float TotalDistance) override
	{
		return (InventoryType == NULL) ? 0.0f : InventoryType.GetDefaultObject()->DetourWeight(Asker, this, TotalDistance);
	}
};