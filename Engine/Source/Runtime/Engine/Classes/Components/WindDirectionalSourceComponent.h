// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Components/SceneComponent.h"
#include "WindDirectionalSourceComponent.generated.h"

/** Component that provides a directional wind source. Only affects SpeedTree assets. */
UCLASS(collapsecategories, hidecategories=(Object, Mobility), editinlinenew)
class ENGINE_API UWindDirectionalSourceComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(interp, Category=WindDirectionalSourceComponent)
	float Strength;

	UPROPERTY(interp, Category=WindDirectionalSourceComponent)
	float Speed;

	UPROPERTY(interp, Category = WindDirectionalSourceComponent)
	float MinGustAmount;

	UPROPERTY(interp, Category = WindDirectionalSourceComponent)
	float MaxGustAmount;

	UPROPERTY(interp, Category = WindDirectionalSourceComponent, meta = (editcondition = "bSimulatePhysics", ClampMin = "0.1", UIMin = "0.1"))
	float Radius;

	UPROPERTY(EditAnywhere, Category = WindDirectionalSourceComponent)
	uint32 bPointWind : 1;

public:
	class FWindSourceSceneProxy* SceneProxy;

protected:
	//~ Begin UActorComponent Interface.
	virtual void CreateRenderState_Concurrent() override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	//~ End UActorComponent Interface.

public:
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual class FWindSourceSceneProxy* CreateSceneProxy() const;
};



