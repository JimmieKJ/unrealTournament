// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "Engine.h"
#include "UTWeapon.h"
#include "UTGameMode.h"

#include "SampleGameMode.generated.h"

USTRUCT(BlueprintType)
struct FStartingInventory
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GameMode")
	TArray<TSubclassOf<AUTInventory>> Inventory;
};

USTRUCT(BlueprintType)
struct FDamageTypeToProgess
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameMode")
	TArray<TSubclassOf<UDamageType>> DamageType;
};

UCLASS(Blueprintable, Meta = (ChildCanTick))
class ASampleGameMode : public AUTGameMode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GameMode")
	TArray<FStartingInventory> StartingInventories;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameMode")
	TArray<FDamageTypeToProgess> ScoringDamageTypes;

	virtual void GiveDefaultInventory(APawn* PlayerPawn) override;
	virtual void ScoreKill(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType) override;
	virtual void GiveNewGun(AUTCharacter *UTCharacter);
};