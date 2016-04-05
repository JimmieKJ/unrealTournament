// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTActivatedPowerupPlaceholder.generated.h"

UCLASS(Blueprintable, Abstract, notplaceable)
class UNREALTOURNAMENT_API AUTActivatedPowerUpPlaceholder : public AUTInventory
{
	GENERATED_UCLASS_BODY()

	virtual void Tick(float DeltaTime) override;
	virtual FText GetHUDText() const override;
	virtual void UpdateHUDText() override;
	virtual bool HUDShouldRender_Implementation(UUTHUDWidget* TargetWidget) override;

	virtual void AddOverlayMaterials_Implementation(AUTGameState* GS) const override;

	/** overlay effect added to the player's weapon while this powerup is being charged by holding the activate powerup button */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	FOverlayEffect OverlayEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	FOverlayEffect OverlayEffect1P;

	//This determines what the FlashTimer value will be overrriden to when the ActivatePowerup button has been held long enough.
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = Effects)
	float FlashTimerOverride;

	//This determines what the InitialFlashTimer value will be overriden to when the ActivatePowerup Button has been held long enough
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = Effects)
	float InitialFlashTimeOverride;

	//This determines what the InitialFlashScale value will be overriden to when the ActivatePowerup Button has been held long enough
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite, Category = Effects)
	float InitialFlashScaleOverride;

	/** Event when play begins for this actor. */
	virtual void BeginPlay() override;

private:
	float OriginalHUDIconUL;
	bool bHasOverlayBeenAddedAlready;

};