// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTTrophyRoomRenderingComponent.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTTrophyRoomRenderingComponent : public UPrimitiveComponent
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
};
