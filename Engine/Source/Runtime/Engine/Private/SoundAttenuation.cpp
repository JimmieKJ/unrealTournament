// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


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

float FAttenuationSettings::GetFocusPriorityScale(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const
{
	float Focus = FocusSettings.FocusPriorityScale * FocusPriorityScale;
	float NonFocus = FocusSettings.NonFocusPriorityScale * NonFocusPriorityScale;
	float Result = FMath::Lerp(Focus, NonFocus, FocusFactor);
	return FMath::Max(0.0f, Result);
}

float FAttenuationSettings::GetFocusAttenuation(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const
{
	float Focus = FocusSettings.FocusVolumeScale * FocusVolumeAttenuation;
	float NonFocus = FocusSettings.NonFocusVolumeScale * NonFocusVolumeAttenuation;
	float Result = FMath::Lerp(Focus, NonFocus, FocusFactor);
	return FMath::Max(0.0f, Result);
}

float FAttenuationSettings::GetFocusDistanceScale(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const
{
	float Focus = FocusSettings.FocusDistanceScale * FocusDistanceScale;
	float NonFocus = FocusSettings.NonFocusDistanceScale * NonFocusDistanceScale;
	float Result = FMath::Lerp(Focus, NonFocus, FocusFactor);
	return FMath::Max(0.0f, Result);
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
float FAttenuationSettings::AttenuationEval(const float Distance, const float Falloff, const float DistanceScale) const
{
	// Clamp the input distance between 0.0f and Falloff. If the Distance
	// is actually less than the min value, it will use the min-value of the algorithm/curve
	// rather than assume it's 1.0 (i.e. it could be 0.0 for an inverse curve). Similarly, if the distance
	// is greater than the falloff value, it'll use the algorithm/curve value evaluated at Falloff distance,
	// which could be 1.0 (and not 0.0f).

	float FalloffCopy = FMath::Max(Falloff, 1.0f);
	float DistanceCopy = FMath::Clamp(Distance, 0.0f, FalloffCopy);

	DistanceCopy *= DistanceScale;

	float Result = 0.0f;
	switch(DistanceAlgorithm)
	{
		case ATTENUATION_Linear:

			Result = (1.0f - (DistanceCopy / FalloffCopy));
			break;

		case ATTENUATION_Logarithmic:

			Result = 0.5f * -FMath::Loge(DistanceCopy / FalloffCopy);
			break;

		case ATTENUATION_Inverse:

			Result = 0.02f / (DistanceCopy / FalloffCopy);
			break;

		case ATTENUATION_LogReverse:

			Result = 1.0f + 0.5f * FMath::Loge(1.0f - (DistanceCopy / FalloffCopy));
			break;

		case ATTENUATION_NaturalSound:
		{
			check( dBAttenuationAtMax <= 0.0f );
			Result = FMath::Pow(10.0f, ((DistanceCopy / FalloffCopy) * dBAttenuationAtMax) / 20.0f);
			break;
		}

		case ATTENUATION_Custom:

			Result = CustomAttenuationCurve.GetRichCurveConst()->Eval(DistanceCopy / FalloffCopy);
			break;

		default:
			checkf(false, TEXT("Uknown attenuation distance algorithm!"))
			break;
	}

	// Make sure the output is clamped between 0.0 and 1.0f. Some of the algorithms above can
	// result in bad values at the edges.
	return FMath::Clamp(Result, 0.0f, 1.0f);
}

float FAttenuationSettings::AttenuationEvalBox(const FTransform& SoundTransform, const FVector ListenerLocation, const float DistanceScale) const
{
	const float DistanceSq = ComputeSquaredDistanceFromBoxToPoint(-AttenuationShapeExtents, AttenuationShapeExtents,SoundTransform.InverseTransformPositionNoScale(ListenerLocation));
	if (DistanceSq < FalloffDistance * FalloffDistance)
	{ 
		return AttenuationEval(FMath::Sqrt(DistanceSq), FalloffDistance, DistanceScale);
	}

	return 0.f;
}

float FAttenuationSettings::AttenuationEvalCapsule(const FTransform& SoundTransform, const FVector ListenerLocation, const float DistanceScale) const
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

	return AttenuationEval(Distance, FalloffDistance, DistanceScale);
}

float FAttenuationSettings::AttenuationEvalCone(const FTransform& SoundTransform, const FVector ListenerLocation, const float DistanceScale) const
{
	const FVector SoundForward = SoundTransform.GetUnitAxis( EAxis::X );

	float VolumeMultiplier = 1.f;

	const FVector Origin = SoundTransform.GetTranslation() - (SoundForward * ConeOffset);

	const float Distance = FMath::Max(FVector::Dist( Origin, ListenerLocation ) - AttenuationShapeExtents.X, 0.f);
	VolumeMultiplier *= AttenuationEval(Distance, FalloffDistance, DistanceScale);

	if (VolumeMultiplier > 0.f)
	{
		const float theta = FMath::RadiansToDegrees(fabsf(FMath::Acos( FVector::DotProduct(SoundForward, (ListenerLocation - Origin).GetSafeNormal()))));
		VolumeMultiplier *= AttenuationEval(theta - AttenuationShapeExtents.Y, AttenuationShapeExtents.Z, 1.0f);
	}

	return VolumeMultiplier;
}

bool FAttenuationSettings::operator==(const FAttenuationSettings& Other) const
{
	return (   bAttenuate			    == Other.bAttenuate
			&& bSpatialize			    == Other.bSpatialize
			&& dBAttenuationAtMax	    == Other.dBAttenuationAtMax
			&& OmniRadius				== Other.OmniRadius
			&& StereoSpread				== Other.StereoSpread
			&& DistanceAlgorithm	    == Other.DistanceAlgorithm
			&& AttenuationShape		    == Other.AttenuationShape
			&& bAttenuateWithLPF	    == Other.bAttenuateWithLPF
			&& LPFRadiusMin			    == Other.LPFRadiusMin
			&& LPFRadiusMax			    == Other.LPFRadiusMax
			&& FalloffDistance		    == Other.FalloffDistance
			&& AttenuationShapeExtents	== Other.AttenuationShapeExtents
			&& SpatializationAlgorithm  == Other.SpatializationAlgorithm
			&& LPFFrequencyAtMin == Other.LPFFrequencyAtMin
			&& LPFFrequencyAtMax == Other.LPFFrequencyAtMax
			&& bEnableListenerFocus == Other.bEnableListenerFocus
			&& FocusAzimuth				== Other.FocusAzimuth
			&& NonFocusAzimuth			== Other.NonFocusAzimuth
			&& FocusDistanceScale		== Other.FocusDistanceScale
			&& FocusPriorityScale		== Other.FocusPriorityScale
			&& NonFocusPriorityScale	== Other.NonFocusPriorityScale
			&& FocusVolumeAttenuation	== Other.FocusVolumeAttenuation
			&& NonFocusVolumeAttenuation == Other.NonFocusVolumeAttenuation);
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