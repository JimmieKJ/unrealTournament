// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"

class COMPONENTVISUALIZERS_API FPhysicsAnimationComponentVisualizer : public FComponentVisualizer
{
public:
	//~ Begin FComponentVisualizer Interface
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	//~ End FComponentVisualizer Interface
};
