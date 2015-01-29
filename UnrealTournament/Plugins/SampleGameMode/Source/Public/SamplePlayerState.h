// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SampleGameMode.h"

#include "SamplePlayerState.generated.h"

UCLASS(Config = Game)
class ASamplePlayerState : public AUTPlayerState
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "PlayerState")
	int32 PlayerLevel;
};