// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "VisualLoggerRenderingComponent.generated.h"

/**
*	Transient actor used to draw visual logger data on level
*/

UCLASS()
class UVisualLoggerRenderingComponent : public UPrimitiveComponent
{
public:
	GENERATED_UCLASS_BODY()

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
};

