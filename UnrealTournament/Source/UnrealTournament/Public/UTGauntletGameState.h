// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTSCTFGameState.h"
#include "UTGauntletGameState.generated.h"

class AUTGauntletFlag;

UCLASS()
class UNREALTOURNAMENT_API AUTGauntletGameState: public AUTSCTFGameState
{
	GENERATED_UCLASS_BODY()

	virtual bool CanShowBoostMenu(AUTPlayerController* Target);
	// Short Circuit this message
	virtual void UpdateTimeMessage(){}
	virtual FText GetGameStatusText(bool bForScoreboard) override;

};