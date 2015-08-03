// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTGameState.h"

#include "UTShowdownGameState.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTShowdownGameState : public AUTGameState
{
	GENERATED_UCLASS_BODY()

	/** if in round intermission, player selecting spawn point */
	UPROPERTY(BlueprintReadOnly, Replicated)
	AUTPlayerState* SpawnSelector;
	/** Time remaining in current intermission stage (post-round delay or spawn choice timer) */
	UPROPERTY(BlueprintReadOnly, Replicated)
	uint8 IntermissionStageTime;
	/** set during final countdown before next round begins */
	UPROPERTY(BlueprintReadOnly, Replicated)
	bool bFinalIntermissionDelay;

	/** required distance between chosen spawn points */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated)
	float MinSpawnDistance;

	bool IsAllowedSpawnPoint_Implementation(AUTPlayerState* Chooser, APlayerStart* DesiredStart) const override;
};