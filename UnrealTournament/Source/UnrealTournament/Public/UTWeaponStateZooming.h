// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiring.h"

#include "UTWeaponStateZooming.generated.h"

USTRUCT()
struct FZoomTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	class UUTWeaponStateZooming* ZoomState;

	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) OVERRIDE;
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

	virtual bool IsFiring() const OVERRIDE
	{
		return false;
	}

	virtual void BeginState(const UUTWeaponState* PrevState) OVERRIDE
	{}

	virtual void PendingFireStarted() OVERRIDE;
	virtual void PendingFireStopped() OVERRIDE;
	virtual void BeginFiringSequence(uint8 FireModeNum) OVERRIDE;
	virtual void EndFiringSequence(uint8 FireModeNum) OVERRIDE;

	virtual void TickZoom(float DeltaTime);
};