// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFGameState.h"
#include "UTCTFRoundGameState.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTCTFRoundGameState : public AUTCTFGameState
{
	GENERATED_UCLASS_BODY()


		UPROPERTY(Replicated)
		uint32 IntermissionTime;

	virtual float GetIntermissionTime() override;
	virtual void DefaultTimer() override;
};
