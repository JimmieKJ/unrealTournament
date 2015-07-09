// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UnrealTournament.h"
#include "Net/UnrealNetwork.h"

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

UCLASS(Blueprintable, Abstract, Meta = (ChildCanTick), Config = SampleGameMode)
class ASampleGameMode : public AUTGameMode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GameMode")
	TArray<FStartingInventory> StartingInventories;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameMode")
	TArray<FDamageTypeToProgess> ScoringDamageTypes;

	UPROPERTY(config)
	int32 MagicNumber;

	virtual void GiveDefaultInventory(APawn* PlayerPawn) override;
	virtual void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;
	virtual void GiveNewGun(AUTCharacter *UTCharacter);
};