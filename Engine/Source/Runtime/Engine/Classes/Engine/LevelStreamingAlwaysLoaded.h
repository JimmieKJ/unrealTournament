// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * LevelStreamingAlwaysLoaded
 *
 * @documentation
 *
 */

#pragma once
#include "LevelStreamingAlwaysLoaded.generated.h"

UCLASS(MinimalAPI)
class ULevelStreamingAlwaysLoaded : public ULevelStreaming
{
	GENERATED_UCLASS_BODY()

	// Begin ULevelStreaming Interface
	virtual bool ShouldBeLoaded() const override;
	virtual bool ShouldBeAlwaysLoaded() const override { return true; } 
	// End ULevelStreaming Interface
};

