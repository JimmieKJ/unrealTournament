// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "LandscapeGizmoRenderComponent.generated.h"

UCLASS(hidecategories=Object)
class ULandscapeGizmoRenderComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()


	// Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// End UPrimitiveComponent Interface

	// Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	// End USceneComponent interface.
};



