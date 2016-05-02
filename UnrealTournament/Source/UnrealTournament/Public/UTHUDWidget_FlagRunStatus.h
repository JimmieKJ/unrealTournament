// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_CTFFlagStatus.h"
#include "UTHUDWidget_FlagRunStatus.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_FlagRunStatus : public UUTHUDWidget_CTFFlagStatus
{
	GENERATED_UCLASS_BODY()

	virtual void DrawStatusMessage(float DeltaTime);


protected:
	virtual void DrawIndicators(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation);
	virtual FText GetFlagReturnTime(AUTCTFFlag* Flag);
	virtual bool ShouldDrawFlag(AUTCTFFlag* Flag, bool bIsEnemyFlag);
};