// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ComponentVisualizersPrivatePCH.h"

#include "PhysicalAnimationComponentVisualizer.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"

void FPhysicsAnimationComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	if(const UPhysicalAnimationComponent* PhysAnimComp = Cast<const UPhysicalAnimationComponent>(Component))
	{
		PhysAnimComp->DebugDraw(PDI);
	}
}
