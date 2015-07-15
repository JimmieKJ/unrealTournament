// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiring.h"

#include "UTWeaponStateZooming.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTWeaponStateZooming : public UUTWeaponStateFiring
{
	GENERATED_UCLASS_BODY()

protected:

	UPROPERTY()
	UAudioComponent* ZoomLoopComp;

public:
	/** material to draw over the player's view when zoomed
	 * the material can use the parameter 'TeamColor' to receive the player's team color in team games (won't be changed in FFA modes)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Zoom)
	UMaterialInterface* OverlayMat;
	/** material instance for e.g. team coloring */
	UPROPERTY()
	UMaterialInstanceDynamic* OverlayMI;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Zoom)
	USoundBase* ZoomInSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Zoom)
	USoundBase* ZoomLoopSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Zoom)
	USoundBase* ZoomOutSound;

	/** whether to display enemy heads while zoomed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Zoom)
	bool bDrawHeads;
	/** whether to ping compensate the head drawing (if enabled) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Zoom)
	bool bDrawPingAdjustedTargets;

	/** material for drawing enemy indicators */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Zoom)
	UTexture2D* TargetIndicator;

	virtual bool IsFiring() const override
	{
		return false;
	}

	virtual void BeginState(const UUTWeaponState* PrevState) override
	{}


	virtual void PendingFireStarted() override;
	virtual void PendingFireStopped() override;
	virtual bool BeginFiringSequence(uint8 FireModeNum, bool bClientFired) override;
	virtual void EndFiringSequence(uint8 FireModeNum) override;
	virtual void WeaponBecameInactive() override;
	virtual bool DrawHUD(class UUTHUDWidget* WeaponHudWidget) override;

	virtual void OnZoomingFinished();

	/**Toggles the looping sound. Looping sound is always using firemode 0 the first run; not sure if bug*/
	UFUNCTION(BlueprintCallable, Category = Zoom)
	virtual void ToggleZoomInSound(bool bNowOn);

	virtual bool WillSpawnShot(float DeltaTime) override;
};