// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCustomPhysicsVolume.generated.h"

UCLASS(BlueprintType)
class UNREALTOURNAMENT_API AUTCustomPhysicsVolume : public APhysicsVolume
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category = "CustomPhysics", EditAnywhere, BlueprintReadWrite)
	float CustomGravityScaling;

	virtual float GetGravityZ() const override;

};