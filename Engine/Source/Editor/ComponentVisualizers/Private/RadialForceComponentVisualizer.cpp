// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ComponentVisualizersPrivatePCH.h"

#include "RadialForceComponentVisualizer.h"
#include "PhysicsEngine/RadialForceComponent.h"


void FRadialForceComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	const URadialForceComponent* ForceComp = Cast<const URadialForceComponent>(Component);
	if(ForceComp != NULL)
	{
		FTransform TM = ForceComp->ComponentToWorld;
		TM.RemoveScaling();

		// Draw light radius
		DrawWireSphereAutoSides(PDI, TM, FColor(200, 255, 255), ForceComp->Radius, SDPG_World);
	}
}
