// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PrimitiveComponent.h"
#include "GameplayDebuggerRenderingComponent.generated.h"

UCLASS(NotBlueprintable, NotBlueprintType, noteditinlinenew, hidedropdown, Transient)
class UGameplayDebuggerRenderingComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;

	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
};
