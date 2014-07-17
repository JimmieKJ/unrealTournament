// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiring.h"

#include "UTWeaponStateZooming.generated.h"

USTRUCT()
struct FZoomTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	class UUTWeaponStateZooming* ZoomState;

	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
	virtual FString DiagnosticMessage();
};

UCLASS()
class UUTWeaponStateZooming : public UUTWeaponStateFiring
{
	GENERATED_UCLASS_BODY()

protected:
	bool bIsZoomed;
	float StartZoomTime;
	FZoomTickFunction ZoomTickHandler;

public:
	/** minimum FOV angle */
	UPROPERTY(EditDefaultsOnly, Category = Zoom)
	float MinFOV;
	/** time to reach minimum FOV from default */
	UPROPERTY(EditDefaultsOnly, Category = Zoom)
	float ZoomTime;
	/** material to draw over the player's view when zoomed
	 * the material can use the parameter 'TeamColor' to receive the player's team color in team games (won't be changed in FFA modes)
	 */
	UPROPERTY(EditDefaultsOnly, Category = Zoom)
	UMaterialInterface* OverlayMat;
	/** material instance for e.g. team coloring */
	UPROPERTY()
	UMaterialInstanceDynamic* OverlayMI;

	virtual bool IsFiring() const override
	{
		return false;
	}

	virtual void BeginState(const UUTWeaponState* PrevState) override
	{}

	virtual void PendingFireStarted() override;
	virtual void PendingFireStopped() override;
	virtual void BeginFiringSequence(uint8 FireModeNum) override;
	virtual void EndFiringSequence(uint8 FireModeNum) override;
	virtual void WeaponBecameInactive() override;
	virtual bool DrawHUD(class UUTHUDWidget* WeaponHudWidget) override;

	virtual void TickZoom(float DeltaTime);
};