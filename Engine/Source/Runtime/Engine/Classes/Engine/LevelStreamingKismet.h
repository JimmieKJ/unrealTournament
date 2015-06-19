// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * LevelStreamingKismet
 *
 * Kismet triggerable streaming implementation.
 *
 */

#pragma once
#include "LevelStreaming.h"
#include "LevelStreamingKismet.generated.h"

UCLASS(MinimalAPI, BlueprintType)
class ULevelStreamingKismet : public ULevelStreaming
{
	GENERATED_UCLASS_BODY()

	/** Whether the level should be loaded at startup																			*/
	UPROPERTY(Category=LevelStreaming, EditAnywhere)
	uint32 bInitiallyLoaded:1;

	/** Whether the level should be visible at startup if it is loaded 															*/
	UPROPERTY(Category=LevelStreaming, EditAnywhere)
	uint32 bInitiallyVisible:1;
	
	// Begin UObject Interface
	virtual void PostLoad() override;
	// End UObject Interface

	// Begin ULevelStreaming Interface
	virtual bool ShouldBeLoaded() const override;
	virtual bool ShouldBeVisible() const override;
	// End ULevelStreaming Interface
};

