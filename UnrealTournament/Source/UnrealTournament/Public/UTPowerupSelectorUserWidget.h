// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UserWidget.h"
#include "AssetData.h"
#include "UTInventory.h"
#include "UTPowerupSelectorUserWidget.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTPowerupSelectorUserWidget : public UUserWidget
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Powerups)
	TArray<TSubclassOf<class AUTInventory>> SelectablePowerups;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Powerups)
	int SelectedPowerupIndex;

	UFUNCTION(BlueprintCallable, Category = "UI Runnables")
	FString GetBuyMenuKeyName();

	UFUNCTION(BlueprintCallable, Category = "UI Runnables")
	void SetPlayerPowerup();
};
