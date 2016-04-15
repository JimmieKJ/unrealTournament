// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFRoundGameState.h"
#include "UTSCTFFlagBase.h"
#include "UTSCTFGameState.generated.h"

class AUTSCTFFlag;

UCLASS()
class UNREALTOURNAMENT_API AUTSCTFGameState: public AUTCTFRoundGameState
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
	AUTSCTFFlag* Flag;

	// The the base that dispenses the flag.  
	UPROPERTY(Replicated)
	AUTSCTFFlagBase* FlagDispenser;


	virtual void DefaultTimer() override;

	UFUNCTION()
	void AttackingTeamChanged();

	UFUNCTION()
	virtual void SetFlagSpawnTimer(int32 NewValue);

	virtual FName GetFlagState(uint8 TeamNum);
	virtual AUTPlayerState* GetFlagHolder(uint8 TeamNum);
	virtual AUTCTFFlagBase* GetFlagBase(uint8 TeamNum);

	// Returns a pointer to the most important flag, or nullptr if there isn't one
	virtual void GetImportantFlag(int32 TeamNum, TArray<AUTCTFFlag*>& ImportantFlags);

	// Returns a pointer to the most important flag base or nullptr if there isn't one
	virtual void GetImportantFlagBase(int32 TeamNum, TArray<AUTCTFFlagBase*>& ImportantBases);
	


};