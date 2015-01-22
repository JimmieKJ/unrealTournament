// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NavMeshRenderingComponent.generated.h"

UCLASS(hidecategories=Object, editinlinenew)
class ENGINE_API UNavMeshRenderingComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	// Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// End UPrimitiveComponent Interface

	// Begin UActorComponent Interface
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	// End UActorComponent Interface

	// Begin USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	// End USceneComponent Interface

	void GatherData(struct FNavMeshSceneProxyData*) const;
};
