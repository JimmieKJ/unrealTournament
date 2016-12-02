// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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
	/** copy of mesh displayed when inventory is not available */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = Pickup)
	UMeshComponent* GhostMesh;
	/** material to be set on GhostMesh */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = PickupDisplay)
	UMaterialInterface* GhostMeshMaterial;
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	UMeshComponent* EditorMesh;
#endif
public:
	inline const UMeshComponent* GetMesh() const
	{
		return Mesh;
	}
	inline const UMeshComponent* GetGhostMesh() const
	{
		return GhostMesh;
	}

	/** contains code shared between placed and dropped pickups for initializing Mesh given an InventoryType */
	static void CreatePickupMesh(AActor* Pickup, UMeshComponent*& PickupMesh, TSubclassOf<AUTInventory> PickupInventoryType, float MeshFloatHeight, const FRotator& RotationOffset, bool bAllowRotating);

	/** how high the pickup floats (additional Z axis translation applied to pickup mesh) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PickupDisplay)
	float FloatHeight;
	/** added to pickup mesh's RelativeRotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PickupDisplay)
	FRotator RotationOffset;
	/** set true when first spawned in round */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PickupDisplay)
		bool bHasEverSpawned;

	/** If pickup wants spoken spawn notification, whether to play for offense (this is for speech, not announcer) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PickupDisplay)
		bool bNotifySpawnForOffense;

	/** If pickup wants spoken spawn notification, whether to play for defense (this is for speech, not announcer) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PickupDisplay)
		bool bNotifySpawnForDefense;

	/** whether the pickup mesh is allowed to rotate (requires blueprint to have RotatingMovementComponent) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PickupDisplay)
	bool bAllowRotatingPickup;

	virtual void BeginPlay() override;
	virtual void PlayRespawnEffects() override;
	
	FTimerHandle SpawnVoiceLineTimer;
	virtual void PlaySpawnVoiceLine();

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
	inline TSubclassOf<AUTInventory> GetInventoryType() const
	{
		return InventoryType;
	}
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void InventoryTypeUpdated();

	virtual bool AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup) override;
	virtual void GiveTo_Implementation(APawn* Target) override;
	virtual void SetPickupHidden(bool bNowHidden) override;
	virtual void Reset_Implementation() override;
	virtual void PlayTakenEffects(bool bReplicate) override;
	virtual bool FlashOnMinimap_Implementation() override;
	virtual void PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir) override;

	virtual FVector GetAdjustedScreenPosition(UCanvas* Canvas, const FVector& WorldPosition, const FVector& ViewPoint, const FVector& ViewDir, float Dist, float Edge, bool& bDrawEdgeArrow);

	UPROPERTY()
		bool bBeaconWasLeft;

	virtual void AddHiddenComponents(bool bTaken, TSet<FPrimitiveComponentId>& HiddenComponents) override
	{
		Super::AddHiddenComponents(bTaken, HiddenComponents);
		if (bTaken)
		{
			if (GetMesh() != NULL)
			{
				HiddenComponents.Add(GetMesh()->ComponentId);
			}
		}
		else
		{
			if (GetGhostMesh() != NULL)
			{
				HiddenComponents.Add(GetGhostMesh()->ComponentId);
			}
			if (TimerEffect != NULL)
			{
				HiddenComponents.Add(TimerEffect->ComponentId);
			}
		}
	}

	virtual FText GetDisplayName() const
	{
		return (InventoryType != NULL) ? InventoryType.GetDefaultObject()->DisplayName : PickupMessageString;
	}

	/** Announce pickup to recipient */
	virtual void AnnouncePickup(AUTCharacter* P);

	virtual float BotDesireability_Implementation(APawn* Asker, AController* RequestOwner, float TotalDistance) override
	{
		return (InventoryType == NULL) ? 0.0f : InventoryType.GetDefaultObject()->BotDesireability(Asker, RequestOwner, this, TotalDistance);
	}
	virtual float DetourWeight_Implementation(APawn* Asker, float TotalDistance) override
	{
		return (InventoryType == NULL) ? 0.0f : InventoryType.GetDefaultObject()->DetourWeight(Asker, this, TotalDistance);
	}
};