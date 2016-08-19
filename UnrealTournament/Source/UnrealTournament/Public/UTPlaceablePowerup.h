// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPlaceablePowerup.generated.h"

UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTPlaceablePowerup : public AUTInventory
{
	GENERATED_UCLASS_BODY()

	/*If checked, when this Inventory item is destroyed, all spawned powerups will also be destroyed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Powerup)
		bool bDestroySpawnedPowerups;

	/*If checked, when you are out of remaining boosts, this item will destroy itself. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Powerup)
		bool bDestroyWhenOutOfBoosts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Powerup)
		FTransform SpawnOffset;

	/*What object we should spawn in the world when this powerup is used.*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Powerup)
		TSubclassOf<AActor> PowerupToSpawn;

	/*If we should attach the spawned actor to our UT owner after spawning it*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Powerup)
		bool bAttachToOwner;

	/*Handles spawning a SpawnedPowerup item.*/
	virtual void SpawnPowerup();

	virtual void Destroyed();

	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate) override;

public:
	/** Returns the HUD text to display for this item. */
	virtual FText GetHUDText() const override
	{ 
		if (UTOwner && UTOwner->PlayerState)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTOwner->PlayerState);
			if (UTPS)
			{
				return FText::AsNumber(UTPS->GetRemainingBoosts());
			}
		}

		return FText::GetEmpty();
	}

	virtual void DrawInventoryHUD_Implementation(UUTHUDWidget* Widget, FVector2D Pos, FVector2D Size) override;

private:
	/** Holds all the power ups this Inventory item has spawned. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SpawnedPowerups;

	virtual bool HUDShouldRender_Implementation(UUTHUDWidget* TargetWidget) override;
};
