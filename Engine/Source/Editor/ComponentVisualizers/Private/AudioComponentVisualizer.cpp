// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ComponentVisualizersPrivatePCH.h"

#include "SoundDefinitions.h"
#include "AudioComponentVisualizer.h"
#include "Components/AudioComponent.h"


void FAudioComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	if(View->Family->EngineShowFlags.AudioRadius)
	{
		const UAudioComponent* AudioComp = Cast<const UAudioComponent>(Component);
		if(AudioComp != NULL)
		{
			const FTransform& Transform = AudioComp->ComponentToWorld;

			TMultiMap<EAttenuationShape::Type, FAttenuationSettings::AttenuationShapeDetails> ShapeDetailsMap;
			AudioComp->CollectAttenuationShapesForVisualization(ShapeDetailsMap);

			FVector Translation = Transform.GetTranslation();
			FVector UnitXAxis   = Transform.GetUnitAxis( EAxis::X );
			FVector UnitYAxis   = Transform.GetUnitAxis( EAxis::Y );
			FVector UnitZAxis   = Transform.GetUnitAxis( EAxis::Z );

			for ( auto It = ShapeDetailsMap.CreateConstIterator(); It; ++It )
			{
				FColor AudioOuterRadiusColor(255, 153, 0);
				FColor AudioInnerRadiusColor(216, 130, 0);

				const FAttenuationSettings::AttenuationShapeDetails& ShapeDetails = It.Value();
				switch(It.Key())
				{
				case EAttenuationShape::Box:

					if (ShapeDetails.Falloff > 0.f)
					{
						DrawOrientedWireBox( PDI, Translation, UnitXAxis, UnitYAxis, UnitZAxis, ShapeDetails.Extents + FVector(ShapeDetails.Falloff), AudioOuterRadiusColor, SDPG_World);
						DrawOrientedWireBox( PDI, Translation, UnitXAxis, UnitYAxis, UnitZAxis, ShapeDetails.Extents, AudioInnerRadiusColor, SDPG_World);
					}
					else
					{
						DrawOrientedWireBox( PDI, Translation, UnitXAxis, UnitYAxis, UnitZAxis, ShapeDetails.Extents, AudioOuterRadiusColor, SDPG_World);
					}
					break;

				case EAttenuationShape::Capsule:

					if (ShapeDetails.Falloff > 0.f)
					{
						DrawWireCapsule( PDI, Translation, UnitXAxis, UnitYAxis, UnitZAxis, AudioOuterRadiusColor, ShapeDetails.Extents.Y + ShapeDetails.Falloff, ShapeDetails.Extents.X + ShapeDetails.Falloff, 25, SDPG_World);
						DrawWireCapsule( PDI, Translation, UnitXAxis, UnitYAxis, UnitZAxis, AudioInnerRadiusColor, ShapeDetails.Extents.Y, ShapeDetails.Extents.X, 25, SDPG_World);
					}
					else
					{
						DrawWireCapsule( PDI, Translation, UnitXAxis, UnitYAxis, UnitZAxis, AudioOuterRadiusColor, ShapeDetails.Extents.Y, ShapeDetails.Extents.X, 25, SDPG_World);
					}
					break;

				case EAttenuationShape::Cone:
					{
						FTransform Origin = Transform;
						Origin.SetScale3D(FVector(1.f));
						Origin.SetTranslation(Translation - (UnitXAxis * ShapeDetails.ConeOffset));

						if (ShapeDetails.Falloff > 0.f || ShapeDetails.Extents.Z > 0.f)
						{
							float ConeRadius = ShapeDetails.Extents.X + ShapeDetails.Falloff + ShapeDetails.ConeOffset;
							float ConeAngle = ShapeDetails.Extents.Y + ShapeDetails.Extents.Z;
							DrawWireSphereCappedCone(PDI, Origin, ConeRadius, ConeAngle, 16, 4, 10, AudioOuterRadiusColor, SDPG_World);

							ConeRadius = ShapeDetails.Extents.X + ShapeDetails.ConeOffset;
							ConeAngle = ShapeDetails.Extents.Y;
							DrawWireSphereCappedCone(PDI, Origin, ConeRadius, ConeAngle, 16, 4, 10, AudioInnerRadiusColor, SDPG_World );
						}
						else
						{
							const float ConeRadius = ShapeDetails.Extents.X + ShapeDetails.ConeOffset;
							const float ConeAngle = ShapeDetails.Extents.Y;
							DrawWireSphereCappedCone(PDI, Origin, ConeRadius, ConeAngle, 16, 4, 10, AudioOuterRadiusColor, SDPG_World );
						}
					}
					break;

				case EAttenuationShape::Sphere:

					if (ShapeDetails.Falloff > 0.f)
					{
						DrawWireSphereAutoSides(PDI, Translation, AudioOuterRadiusColor, ShapeDetails.Extents.X + ShapeDetails.Falloff, SDPG_World);
						DrawWireSphereAutoSides(PDI, Translation, AudioInnerRadiusColor, ShapeDetails.Extents.X, SDPG_World);
					}
					else
					{
						DrawWireSphereAutoSides(PDI, Translation, AudioOuterRadiusColor, ShapeDetails.Extents.X, SDPG_World);
					}
					break;

				default:
					check(false);
				}
			}
		}
	}
}
