// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "WindDirectionalSourceComponent.generated.h"

UCLASS(collapsecategories, hidecategories=(Object, Mobility), editinlinenew)
class UWindDirectionalSourceComponent : public USceneComponent
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

public:
	class FWindSourceSceneProxy* SceneProxy;

protected:
	// Begin UActorComponent interface.
	virtual void CreateRenderState_Concurrent() override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	// End UActorComponent interface.

public:
	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual class FWindSourceSceneProxy* CreateSceneProxy() const;
};



