// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ComponentVisualizersPrivatePCH.h"

#include "PointLightComponentVisualizer.h"
#include "Components/PointLightComponent.h"


void FPointLightComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	if(View->Family->EngineShowFlags.LightRadius)
	{
		const UPointLightComponent* PointLightComp = Cast<const UPointLightComponent>(Component);
		if(PointLightComp != NULL)
		{
			FTransform LightTM = PointLightComp->ComponentToWorld;
			LightTM.RemoveScaling();

			// Draw light radius
			DrawWireSphereAutoSides(PDI, LightTM, FColor(200, 255, 255), PointLightComp->AttenuationRadius, SDPG_World);

			// Draw point light source shape
			DrawWireCapsule(PDI, LightTM.GetTranslation(), -LightTM.GetUnitAxis( EAxis::Z ), LightTM.GetUnitAxis( EAxis::Y ), LightTM.GetUnitAxis( EAxis::X ),
							FColor(231, 239, 0, 255), PointLightComp->SourceRadius, 0.5f * PointLightComp->SourceLength + PointLightComp->SourceRadius, 25, SDPG_World);
		}
	}

}
