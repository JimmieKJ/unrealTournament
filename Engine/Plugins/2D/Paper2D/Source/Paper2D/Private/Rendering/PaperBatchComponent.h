// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperBatchComponent.generated.h"

// Dummy component designed to create a batch manager render scene proxy that does work on the render thread
UCLASS()
class UPaperBatchComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	// UActorComponent interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// End of UActorComponent interface

	// UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	// End of UPrimitiveComponent interface
};
