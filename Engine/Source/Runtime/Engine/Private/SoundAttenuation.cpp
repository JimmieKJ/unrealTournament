// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "SoundDefinitions.h"

/*-----------------------------------------------------------------------------
	USoundAttenuation implementation.
-----------------------------------------------------------------------------*/

void FAttenuationSettings::PostSerialize(const FArchive& Ar)
{
	if (Ar.UE4Ver() < VER_UE4_ATTENUATION_SHAPES)
	{
		FalloffDistance = RadiusMax_DEPRECATED - RadiusMin_DEPRECATED;

		switch(DistanceType_DEPRECATED)
		{
		case SOUNDDISTANCE_Normal:
			AttenuationShape = EAttenuationShape::Sphere;
			AttenuationShapeExtents = FVector(RadiusMin_DEPRECATED, 0.f, 0.f);
			break;

		case SOUNDDISTANCE_InfiniteXYPlane:
			AttenuationShape = EAttenuationShape::Box;
			AttenuationShapeExtents = FVector(WORLD_MAX, WORLD_MAX, RadiusMin_DEPRECATED);
			break;

		case SOUNDDISTANCE_InfiniteXZPlane:
			AttenuationShape = EAttenuationShape::Box;
			AttenuationShapeExtents = FVector(WORLD_MAX, RadiusMin_DEPRECATED, WORLD_MAX);
			break;

		case SOUNDDISTANCE_InfiniteYZPlane:
			AttenuationShape = EAttenuationShape::Box;
			AttenuationShapeExtents = FVector(RadiusMin_DEPRECATED, WORLD_MAX, WORLD_MAX);
			break;
		}
	}
}

float FAttenuationSettings::GetMaxDimension() const
{
	float MaxDimension = FalloffDistance;

	switch(AttenuationShape)
	{
	case EAttenuationShape::Sphere:
	case EAttenuationShape::Cone:

		MaxDimension += AttenuationShapeExtents.X;
		break;

	case EAttenuationShape::Box:

		MaxDimension += FMath::Max3(AttenuationShapeExtents.X, AttenuationShapeExtents.Y, AttenuationShapeExtents.Z);
		break;

	case EAttenuationShape::Capsule:
		
		MaxDimension += FMath::Max(AttenuationShapeExtents.X, AttenuationShapeExtents.Y);
		break;

	default:
		check(false);
	}

	return MaxDimension;
}

/**
 * Calculate the attenuation value.
 * @param DistanceModel - Which math model of attenuation is used
 * @param Distance - Distance from the attenuation shape
 * @param Falloff - The distance over which to attenuate the volume
 * @param dBAttenuationAtMax
 *
 * @return Attenuation value (between 0.0 and 1.0)
 */
float AttenuationEval(const ESoundDistanceModel DistanceModel, const float Distance, const float Falloff, const float dBAttenuationAtMax)
{
	if( Distance > Falloff )
	{
		return 0.0f;
	}
	// UsedMinRadius is the point at which to start attenuating
	else if( Distance > 0.f )
	{
		switch(DistanceModel)
		{
			case ATTENUATION_Linear:

				return ( 1.0f - ( Distance / Falloff ) );

			case ATTENUATION_Logarithmic:

				return FMath::Min( 0.5f * -FMath::Loge( Distance / Falloff ), 1.0f );

			case ATTENUATION_Inverse:

				return FMath::Min( 0.02f / ( Distance / Falloff ) , 1.0f );

			case ATTENUATION_LogReverse:

				return FMath::Max(1.0f + 0.5f * FMath::Loge(1.0f - (Distance / Falloff)), 0.0f);

			case ATTENUATION_NaturalSound:
			{
				check( dBAttenuationAtMax <= 0.0f );
				return FMath::Pow( 10.0f, ( ( Distance / Falloff ) * dBAttenuationAtMax ) / 20.0f );
			}
		}
	}
	return 1.0f;
}

float FAttenuationSettings::AttenuationEvalBox(const FTransform& SoundTransform, const FVector ListenerLocation) const
{
	const float DistanceSq = ComputeSquaredDistanceFromBoxToPoint(-AttenuationShapeExtents, AttenuationShapeExtents,SoundTransform.InverseTransformPositionNoScale(ListenerLocation));
	if (DistanceSq < FalloffDistance * FalloffDistance)
	{
		return AttenuationEval(DistanceAlgorithm, FMath::Sqrt(DistanceSq), FalloffDistance, dBAttenuationAtMax);
	}

	return 0.f;
}

float FAttenuationSettings::AttenuationEvalCapsule(const FTransform& SoundTransform, const FVector ListenerLocation) const
{
	float Distance = 0.f;
	const float CapsuleHalfHeight = AttenuationShapeExtents.X;
	const float CapsuleRadius = AttenuationShapeExtents.Y;

	// Capsule devolves to a sphere if HalfHeight <= Radius
	if (CapsuleHalfHeight <= CapsuleRadius )
	{
		Distance = FMath::Max(FVector::Dist( SoundTransform.GetTranslation(), ListenerLocation ) - CapsuleRadius, 0.f);
	}
	else
	{
		const FVector PointOffset = (CapsuleHalfHeight - CapsuleRadius) * SoundTransform.GetUnitAxis( EAxis::Z );
		const FVector StartPoint = SoundTransform.GetTranslation() + PointOffset;
		const FVector EndPoint = SoundTransform.GetTranslation() - PointOffset;

		Distance = FMath::PointDistToSegment(ListenerLocation, StartPoint, EndPoint) - CapsuleRadius;
	}

	return AttenuationEval(DistanceAlgorithm, Distance, FalloffDistance, dBAttenuationAtMax);
}

float FAttenuationSettings::AttenuationEvalCone(const FTransform& SoundTransform, const FVector ListenerLocation) const
{
	const FVector SoundForward = SoundTransform.GetUnitAxis( EAxis::X );

	float VolumeMultiplier = 1.f;

	const FVector Origin = SoundTransform.GetTranslation() - (SoundForward * ConeOffset);

	const float Distance = FMath::Max(FVector::Dist( Origin, ListenerLocation ) - AttenuationShapeExtents.X, 0.f);
	VolumeMultiplier *= AttenuationEval(DistanceAlgorithm, Distance, FalloffDistance, dBAttenuationAtMax);

	if (VolumeMultiplier > 0.f)
	{
		const float theta = FMath::RadiansToDegrees(fabsf(FMath::Acos( FVector::DotProduct(SoundForward, (ListenerLocation - Origin).GetSafeNormal()))));
		VolumeMultiplier *= AttenuationEval(DistanceAlgorithm, theta - AttenuationShapeExtents.Y, AttenuationShapeExtents.Z, dBAttenuationAtMax);
	}

	return VolumeMultiplier;
}

bool FAttenuationSettings::operator==(const FAttenuationSettings& Other) const
{
	return (   bAttenuate			    == Other.bAttenuate
			&& bSpatialize			    == Other.bSpatialize
			&& dBAttenuationAtMax	    == Other.dBAttenuationAtMax
			&& DistanceAlgorithm	    == Other.DistanceAlgorithm
			&& AttenuationShape		    == Other.AttenuationShape
			&& bAttenuateWithLPF	    == Other.bAttenuateWithLPF
			&& LPFRadiusMin			    == Other.LPFRadiusMin
			&& LPFRadiusMax			    == Other.LPFRadiusMax
			&& FalloffDistance		    == Other.FalloffDistance
			&& AttenuationShapeExtents	== Other.AttenuationShapeExtents
			&& SpatializationAlgorithm  == Other.SpatializationAlgorithm);
}

void FAttenuationSettings::ApplyAttenuation( const FTransform& SoundTransform, const FVector ListenerLocation, float& Volume, float& HighFrequencyGain ) const
{
	// Attenuate the volume based on the model
	if( bAttenuate )
	{
		switch(AttenuationShape)
		{
		case EAttenuationShape::Sphere:
			{
				const float Distance = FMath::Max(FVector::Dist( SoundTransform.GetTranslation(), ListenerLocation ) - AttenuationShapeExtents.X, 0.f);
				Volume *= AttenuationEval(DistanceAlgorithm, Distance, FalloffDistance, dBAttenuationAtMax);
				break;
			}

		case EAttenuationShape::Box:
			Volume *= AttenuationEvalBox(SoundTransform, ListenerLocation);
			break;

		case EAttenuationShape::Capsule:
			Volume *= AttenuationEvalCapsule(SoundTransform, ListenerLocation);
			break;

		case EAttenuationShape::Cone:
			Volume *= AttenuationEvalCone(SoundTransform, ListenerLocation);
			break;

		default:
			check(false);
		}

	}

	// Attenuate with the low pass filter if necessary
	if( bAttenuateWithLPF )
	{
		const float Distance = FMath::Max(FVector::Dist( SoundTransform.GetTranslation(), ListenerLocation ) - AttenuationShapeExtents.X, 0.f);

		if( Distance >= LPFRadiusMax )
		{
			HighFrequencyGain = 0.0f;
		}
		// UsedLPFMinRadius is the point at which to start applying the low pass filter
		else if( Distance > LPFRadiusMin )
		{
			HighFrequencyGain = 1.0f - ( ( Distance - LPFRadiusMin ) / ( LPFRadiusMax - LPFRadiusMin ) );
		}
		else
		{
			HighFrequencyGain = 1.0f;
		}
	}
}

void FAttenuationSettings::CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, AttenuationShapeDetails>& ShapeDetailsMap) const
{
	if (bAttenuate)
	{
		AttenuationShapeDetails ShapeDetails;
		ShapeDetails.Extents = AttenuationShapeExtents;
		ShapeDetails.Falloff = FalloffDistance;
		ShapeDetails.ConeOffset = ConeOffset;

		ShapeDetailsMap.Add(AttenuationShape, ShapeDetails);
	}
}

USoundAttenuation::USoundAttenuation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}