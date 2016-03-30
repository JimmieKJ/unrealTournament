// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTJumpPadRenderingComponent.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTJumpPadRenderingComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	// Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	/** Should recreate proxy one very update */
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override{ return true; }
	// End UPrimitiveComponent Interface

	// Begin USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	// End USceneComponent Interface

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	UPROPERTY()
	FVector GameThreadJumpVelocity;

	UPROPERTY()
	float GameThreadGravityZ;
};
