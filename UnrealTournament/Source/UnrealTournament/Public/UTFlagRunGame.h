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

public:

	/** optional class spawned at source location after translocating that continues to receive damage for a short duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TSubclassOf<class AUTWeaponRedirector> AfterImageType;

	UPROPERTY()
		float RallyRequestTime;

	virtual float OverrideRespawnTime(TSubclassOf<AUTInventory> InventoryType) override;
	virtual void HandleRallyRequest(AUTPlayerController* PC) override;
	virtual void CompleteRallyRequest(AUTPlayerController* PC) override;

	virtual int32 GetComSwitch(FName CommandTag, AActor* ContextActor, AUTPlayerController* Instigator, UWorld* World);
	virtual void InitFlags() override;
};

