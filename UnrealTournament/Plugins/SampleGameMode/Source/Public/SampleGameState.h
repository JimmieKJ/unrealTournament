// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SampleGameMode.h"

#include "SampleGameState.generated.h"

UCLASS(Config = Game)
class ASampleGameState : public AUTGameState
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "GameState")
	bool bPlayerReachedLastLevel;
};