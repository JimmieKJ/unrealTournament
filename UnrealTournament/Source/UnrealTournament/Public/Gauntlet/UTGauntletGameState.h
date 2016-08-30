// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFRoundGameState.h"
#include "UTGauntletFlagDispenser.h"
#include "UTGauntletGameState.generated.h"

class AUTGauntletFlag;

UCLASS()
class UNREALTOURNAMENT_API AUTGauntletGameState: public AUTCTFRoundGameState
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(Replicated) 
	int32 FlagSwapTime;

	UPROPERTY(Replicated)
	AUTGauntletFlag* Flag;

	// The the base that dispenses the flag.  
	UPROPERTY(Replicated)
	AUTGauntletFlagDispenser* FlagDispenser;

	virtual void DefaultTimer() override;

	virtual FName GetFlagState(uint8 TeamNum);
	virtual AUTPlayerState* GetFlagHolder(uint8 TeamNum);
	virtual AUTCTFFlagBase* GetFlagBase(uint8 TeamNum);

	// Returns a pointer to the most important flag, or nullptr if there isn't one
	virtual void GetImportantFlag(int32 TeamNum, TArray<AUTCTFFlag*>& ImportantFlags);

	// Returns a pointer to the most important flag base or nullptr if there isn't one
	virtual void GetImportantFlagBase(int32 TeamNum, TArray<AUTCTFFlagBase*>& ImportantBases);


};