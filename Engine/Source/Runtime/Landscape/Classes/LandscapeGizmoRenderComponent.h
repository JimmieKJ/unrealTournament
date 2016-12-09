// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "LandscapeGizmoRenderComponent.generated.h"

class FPrimitiveSceneProxy;

UCLASS(hidecategories=Object)
class ULandscapeGizmoRenderComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()


	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.
};



