// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTFlagRunHUD.h"
#include "UTRadialMenu_BoostPowerup.h"

#include "UTFlagRunPvEHUD.generated.h"

UCLASS()
class AUTFlagRunPvEHUD : public AUTFlagRunHUD
{
	GENERATED_BODY()
public:
	AUTFlagRunPvEHUD(const FObjectInitializer& OI);
protected:
	UPROPERTY()
	UUTRadialMenu_BoostPowerup* BoostWheel;
	UPROPERTY()
	UMaterialInterface* ChargeIndicatorMat;
	UPROPERTY()
	UMaterialInstanceDynamic* ChargeIndicatorMID;
public:
	virtual void BeginPlay() override;
	virtual void ToggleBoostWheel(bool bShow);
	bool bShowBoostWheel;
	virtual void DrawHUD() override;
	virtual void GetPlayerListForIcons(TArray<AUTPlayerState*>& SortedPlayers) override;
};