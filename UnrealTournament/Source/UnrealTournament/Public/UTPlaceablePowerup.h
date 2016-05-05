// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPlaceablePowerup.generated.h"

UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTPlaceablePowerup : public AUTInventory
{
	GENERATED_UCLASS_BODY()

	/**Number of powerups you can drop */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadWrite, Category = Powerup)
		int NumCharges;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Powerup)
		bool bUseCharges;

	/*If checked, when this Inventory item is destroyed, all spawned powerups will also be destroyed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Powerup)
		bool bDestroySpawnedPowerups;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Powerup)
		FTransform SpawnOffset;

	/*What object we should spawn in the world when this powerup is used.*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Powerup)
		TSubclassOf<AActor> PowerupToSpawn;

	/*Handles spawning a SpawnedPowerup item.*/
	virtual void SpawnPowerup();

	virtual void Destroyed();

public:
	/** Returns the HUD text to display for this item. */
	virtual FText GetHUDText() const override
	{ 
		return FText::AsNumber(NumCharges); 
	}

	virtual void DrawInventoryHUD_Implementation(UUTHUDWidget* Widget, FVector2D Pos, FVector2D Size) override;

private:
	/** Holds all the power ups this Inventory item has spawned. */
	TArray<TWeakObjectPtr<AActor>> SpawnedPowerups;

	virtual bool HUDShouldRender_Implementation(UUTHUDWidget* TargetWidget) override;
};
