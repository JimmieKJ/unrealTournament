// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCosmetic.h"
#include "UTHat.generated.h"

UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTHat : public AUTCosmetic
{
	GENERATED_UCLASS_BODY()
	
	FRotator HeadshotRotationRate;

	float HeadshotRotationTime;

	UPROPERTY()
	bool bHeadshotRotating;

	UPROPERTY(EditDefaultsOnly, Category = nonleaderhat)
	TSubclassOf<class AUTHatLeader> LeaderHatClass;

	UFUNCTION()
	void HeadshotRotationComplete();

	virtual void OnWearerHeadshot_Implementation() override;
	
	virtual void SetBodiesToSimulatePhysics();

	virtual void Tick(float DeltaSeconds) override;
};
