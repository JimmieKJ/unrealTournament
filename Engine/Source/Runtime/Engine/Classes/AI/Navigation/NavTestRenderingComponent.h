// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NavTestRenderingComponent.generated.h"

UCLASS(hidecategories=Object)
class UNavTestRenderingComponent: public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
};
