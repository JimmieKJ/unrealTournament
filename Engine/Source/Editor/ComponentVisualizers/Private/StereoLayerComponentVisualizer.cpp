// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ComponentVisualizersPrivatePCH.h"

#include "StereoLayerComponentVisualizer.h"
#include "Components/StereoLayerComponent.h"


void FStereoLayerComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	const UStereoLayerComponent* StereoLayerComp = Cast<const UStereoLayerComponent>(Component);
	if(StereoLayerComp != NULL)
	{
		const FVector2D QuadSize = StereoLayerComp->GetQuadSize() / 2.0f;
		const FBox QuadBox(FVector(0.0f, -QuadSize.X, -QuadSize.Y), FVector(0.0f, QuadSize.X, QuadSize.Y));

		DrawWireBox(PDI, StereoLayerComp->ComponentToWorld.ToMatrixWithScale(), QuadBox, FColor(231, 239, 0, 255), 0);
	}
}
