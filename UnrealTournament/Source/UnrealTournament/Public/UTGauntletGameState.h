// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFGameState.h"
#include "UTGauntletGameState.generated.h"

class AUTGauntletFlag;

UCLASS()
class UNREALTOURNAMENT_API AUTGauntletGameState: public AUTCTFGameState
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(Replicated, replicatedUsing = AttackingTeamChanged)
	uint8 AttackingTeam;

	UPROPERTY(Replicated) 
	int32 FlagSpawnTimer;

	UPROPERTY(Replicated) 
	int32 FlagSwapTime;

	UPROPERTY(Replicated)
	AUTGauntletFlag* Flag;

	virtual void DefaultTimer() override;

	UFUNCTION()
	void AttackingTeamChanged();

	UFUNCTION()
	virtual void SetFlagSpawnTimer(int32 NewValue);
};