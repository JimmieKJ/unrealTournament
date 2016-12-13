// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTFlagRunGameState.h"

#include "UTFlagRunPvEGameState.generated.h"

UCLASS()
class AUTFlagRunPvEGameState : public AUTFlagRunGameState
{
	GENERATED_BODY()
public:
	UPROPERTY(Replicated)
	int32 KillsUntilExtraLife;
	UPROPERTY()
	int32 ExtraLivesGained;
};