// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UnrealTournament.h"

#include "CorsairRGB.generated.h"

UCLASS(Blueprintable, Meta = (ChildCanTick))
class ACorsairRGB : public AActor
{
	GENERATED_UCLASS_BODY()
	
};

struct FCorsairRGB : FTickableGameObject, IModuleInterface
{
	FCorsairRGB();
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual bool IsTickableWhenPaused() const override { return true; }

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	// Put a real stat id here
	virtual TStatId GetStatId() const
	{
		return TStatId();
	}

	float DeltaTimeAccumulator;
	float FrameTimeMinimum;
	int FlashSpeed;

	uint64 LastFrameCounter;

	bool bIsFlashingForEnd;
	bool bInitialized;
	bool bCorsairSDKEnabled;
	
	bool bPlayingIdleColors;

	void UpdateIdleColors(float DeltaTime);
};