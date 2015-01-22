// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
/**
 *
 */
#include "Animation/AnimInstance.h"
#include "VehicleAnimInstance.generated.h"

UCLASS(transient)
class ENGINE_API UVehicleAnimInstance : public UAnimInstance
{
	GENERATED_UCLASS_BODY()

	/** Makes a montage jump to the end of a named section. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	class AWheeledVehicle * GetVehicle();
};



