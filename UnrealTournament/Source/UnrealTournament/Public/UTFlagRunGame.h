// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTCTFScoring.h"
#include "UTCTFRoundGame.h"
#include "UTFlagRunGame.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTFlagRunGame : public AUTCTFRoundGame
{
	GENERATED_UCLASS_BODY()

	virtual void BroadcastVictoryConditions();
	virtual float OverrideRespawnTime(TSubclassOf<AUTInventory> InventoryType) override;

public:
	virtual int32 GetComSwitch(FName CommandTag, AActor* ContextActor, AUTPlayerController* Instigator, UWorld* World);

};

