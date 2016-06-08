// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTMutator.h"

#include "UTMutator_WeaponArena.generated.h"

UCLASS(Blueprintable, Meta = (ChildCanTick), Config = Game)
class UNREALTOURNAMENT_API AUTMutator_WeaponArena : public AUTMutator
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadWrite, Config)
	FString ArenaWeaponPath;
	/** whether to leave the translocator in the game */
	UPROPERTY(BlueprintReadWrite, Config)
	bool bAllowTranslocator;
	UPROPERTY(BlueprintReadWrite)
	TSubclassOf<class AUTWeapon> ArenaWeaponType;
	UPROPERTY(BlueprintReadWrite)
	TSubclassOf<class AUTPickupAmmo> ArenaAmmoType;

	virtual void BeginPlay() override;
	void ModifyPlayer_Implementation(APawn* Other, bool bIsNewSpawn) override;
	bool CheckRelevance_Implementation(AActor* Other) override;
	void GetGameURLOptions_Implementation(TArray<FString>& OptionsList) const override;
	void Init_Implementation(const FString& Options) override;
};