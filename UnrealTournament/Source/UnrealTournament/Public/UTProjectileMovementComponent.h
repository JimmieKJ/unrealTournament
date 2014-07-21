// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectileMovementComponent.generated.h"

UCLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class UUTProjectileMovementComponent : public UProjectileMovementComponent
{
	GENERATED_UCLASS_BODY()

	/** linear acceleration in the direction of current velocity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
	float AccelRate;
	/** explicit acceleration vector (additive with AccelRate) */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Projectile)
	FVector Acceleration;

	/** additional components that should be moved along with the main UpdatedComponent. Defaults to all colliding children of UpdatedComponent.
	 * closest blocking hit of all components is used for blocking collision
	 *
	 * Ultimately this is a workaround for UPrimitiveComponent::MoveComponent() not sweeping children.
	 */
	UPROPERTY(BlueprintReadOnly, Category = MovementComponent)
	TArray<UPrimitiveComponent*> AddlUpdatedComponents;

	virtual void InitializeComponent() override;

	virtual bool MoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* OutHit);

	virtual FVector CalculateVelocity(FVector OldVelocity, float DeltaTime)
	{
		OldVelocity += OldVelocity.SafeNormal() * AccelRate * DeltaTime + Acceleration * DeltaTime;
		return Super::CalculateVelocity(OldVelocity, DeltaTime);
	}
};