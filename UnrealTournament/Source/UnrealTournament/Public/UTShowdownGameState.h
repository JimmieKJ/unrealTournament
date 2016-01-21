// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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

	/** set during intermission after spawn selection is started */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StartSpawnSelect)
	bool bStartedSpawnSelection;

	/** transiently true whe bStartedSpawnSelection becomes set. */
	UPROPERTY(BlueprintReadOnly)
		bool bStartingSpawnSelection;

	/** set during final countdown before next round begins */
	UPROPERTY(BlueprintReadOnly, Replicated)
	bool bFinalIntermissionDelay;

	/** turns on seeing enemies through walls for all players */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_XRayVision)
	bool bActivateXRayVision;

	/** True once match has been in progress. */
	UPROPERTY(BlueprintReadOnly, Replicated)
		bool bMatchHasStarted;
	
	bool IsAllowedSpawnPoint_Implementation(AUTPlayerState* Chooser, APlayerStart* DesiredStart) const override;

	UFUNCTION()
		virtual void OnRep_StartSpawnSelect();

	UFUNCTION()
	virtual void OnRep_XRayVision();

	virtual void OnRep_MatchState() override;

	virtual void CheckTimerMessage() override;
};