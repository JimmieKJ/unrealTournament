// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectileMovementComponent.generated.h"

UCLASS(CustomConstructor, ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class UUTProjectileMovementComponent : public UProjectileMovementComponent
{
	GENERATED_UCLASS_BODY()

	UUTProjectileMovementComponent(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	/** linear acceleration in the direction of current velocity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
	float AccelRate;

	virtual FVector CalculateVelocity(FVector OldVelocity, float DeltaTime)
	{
		OldVelocity += OldVelocity.SafeNormal() * AccelRate * DeltaTime;
		return Super::CalculateVelocity(OldVelocity, DeltaTime);
	}
};