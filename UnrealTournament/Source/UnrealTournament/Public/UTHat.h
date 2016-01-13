// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCosmetic.h"
#include "UTHat.generated.h"

UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTHat : public AUTCosmetic
{
	GENERATED_UCLASS_BODY()
	
	FRotator HeadshotRotationRate;

	float HeadshotRotationTime;

	// If set to true, this hat will not be dropped in to the world upon the death of the character wearing it.
	// it's used for special hats :)
	UPROPERTY(EditDefaultsOnly, Category = hat)
	bool bDontDropOnDeath;
	

	UPROPERTY()
	bool bHeadshotRotating;

	UPROPERTY(EditDefaultsOnly, Category = nonleaderhat)
	TSubclassOf<class AUTHatLeader> LeaderHatClass;

	// If set to true, this hat is special and can't be selected via the menus or URL.  It can only be assigned by the game
	UPROPERTY(EditDefaultsOnly, Category = nonleaderhat)
	bool bOverrideOnly;

	UFUNCTION()
	void HeadshotRotationComplete();

	virtual void OnWearerHeadshot_Implementation() override;
	virtual void OnWearerDeath_Implementation(TSubclassOf<UDamageType> DamageType) override;
	
	virtual void Tick(float DeltaSeconds) override;
};
