// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NavMeshRenderingComponent.generated.h"

struct FNavMeshSceneProxyData;

UCLASS(hidecategories=Object, editinlinenew)
class ENGINE_API UNavMeshRenderingComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

public:
	// Begin UObject Interface
	virtual bool NeedsLoadForServer() const override;
	// End UObject Interface
	
	// Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void OnRegister()  override;
	virtual void OnUnregister()  override;
	// End UPrimitiveComponent Interface

	// Begin UActorComponent Interface
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	// End UActorComponent Interface

	// Begin USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	// End USceneComponent Interface

	void GatherData(FNavMeshSceneProxyData& DebugDrawData) const;

	static bool IsNavigationShowFlagSet(const UWorld* World);

protected:
	void TimerFunction();

protected:
	uint32 bCollectNavigationData : 1;
	FTimerHandle TimerHandle;
};
