// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ComponentVisualizersPrivatePCH.h"

#include "PrimitiveComponentVisualizer.h"

static const FColor	COMColor(0,255,0);

void FPrimitiveComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	if (const UPrimitiveComponent* PrimComp = Cast<const UPrimitiveComponent>(Component))
	{
		if (PrimComp->IsSimulatingPhysics())
		{
			if (FBodyInstance* BI = PrimComp->GetBodyInstance())
			{
				const FVector& COMPosition = BI->GetCOMPosition();
				DrawCircle(PDI, COMPosition, FVector(1, 0, 0), FVector(0, 1, 0), COMColor, 15, 25, SDPG_World);
				DrawCircle(PDI, COMPosition, FVector(0, 0, 1), FVector(0, 1, 0), COMColor, 15, 25, SDPG_World);
			}
		}
	}
}
