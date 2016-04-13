// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTActivatedPowerupPlaceholder.generated.h"

UCLASS(Blueprintable, Abstract, notplaceable)
class UNREALTOURNAMENT_API AUTActivatedPowerUpPlaceholder : public AUTInventory
{
	GENERATED_UCLASS_BODY()

	virtual void Tick(float DeltaTime) override;
	virtual bool HUDShouldRender_Implementation(UUTHUDWidget* TargetWidget) override;

	virtual void AddOverlayMaterials_Implementation(AUTGameState* GS) const override;

	/** overlay effect added to the player's weapon while this powerup is being charged by holding the activate powerup button */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	FOverlayEffect OverlayEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	FOverlayEffect OverlayEffect1P;

	/** Event when play begins for this actor. */
	virtual void BeginPlay() override;

private:
	bool bHasOverlayBeenAddedAlready;

};