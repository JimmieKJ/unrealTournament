// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Engine/EngineTypes.h"
#include "Curves/CurveFloat.h"
#include "SoundAttenuation.generated.h"

UENUM()
enum ESoundDistanceModel
{
	ATTENUATION_Linear,
	ATTENUATION_Logarithmic,
	ATTENUATION_Inverse,
	ATTENUATION_LogReverse,
	ATTENUATION_NaturalSound,
	ATTENUATION_Custom,
	ATTENUATION_MAX,
};

UENUM()
enum ESoundDistanceCalc
{
	SOUNDDISTANCE_Normal,
	SOUNDDISTANCE_InfiniteXYPlane,
	SOUNDDISTANCE_InfiniteXZPlane,
	SOUNDDISTANCE_InfiniteYZPlane,
	SOUNDDISTANCE_MAX,
};

UENUM()
namespace EAttenuationShape
{
	enum Type
	{
		Sphere,
		Capsule,
		Box,
		Cone
	};
}

UENUM()
enum ESoundSpatializationAlgorithm
{
	SPATIALIZATION_Default,
	SPATIALIZATION_HRTF,
};

/*
The settings for attenuating.
*/
USTRUCT(BlueprintType)
struct ENGINE_API FAttenuationSettings
{
	GENERATED_USTRUCT_BODY()

	/* Enable attenuation via volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation )
	uint32 bAttenuate:1;

	/* Enable the source to be positioned in 3D. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation )
	uint32 bSpatialize:1;

	/* Enable attenuation via low pass filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LowPassFilter )
	uint32 bAttenuateWithLPF:1;

	/** Whether or not listener-focus calculations are enabled for this attenuation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Focus, meta = (EditCondition = "bSpatialize"))
	uint32 bEnableListenerFocus:1;

	/** Whether or not to enable line-of-sight occlusion checking for the sound that plays in this audio component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Occlusion, meta = (EditCondition = "bSpatialize"))
	uint32 bEnableOcclusion:1;

	/** Whether or not to enable complex geometry occlusion checks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Occlusion)
	uint32 bUseComplexCollisionForOcclusion:1;

	/* The type of volume versus distance algorithm to use for the attenuation model. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation )
	TEnumAsByte<enum ESoundDistanceModel> DistanceAlgorithm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation)
	FRuntimeFloatCurve CustomAttenuationCurve;

	UPROPERTY()
	TEnumAsByte<enum ESoundDistanceCalc> DistanceType_DEPRECATED;

	/* The shape of the attenuation volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation )
	TEnumAsByte<enum EAttenuationShape::Type> AttenuationShape;

	/* The volume at maximum distance in deciBels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation, meta=(DisplayName = "dB Attenuation At Max", ClampMax = "0" ))
	float dBAttenuationAtMax;

	/** At what distance we start treating the sound source as spatialized */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation, meta=(ClampMin = "0", EditCondition="bSpatialize", DisplayName="Non-Spatialized Radius"))
	float OmniRadius;

	/** The distance between left and right stereo channels when stereo assets spatialized. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attenuation, meta = (ClampMin = "0", EditCondition = "bSpatialize", DisplayName = "3D Stereo Spread"))
	float StereoSpread;

	/** Which spatialization algorithm to use if spatializing mono sources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attenuation, meta = (ClampMin = "0", EditCondition = "bSpatialize", DisplayName = "Spatialization Algorithm"))
	TEnumAsByte<enum ESoundSpatializationAlgorithm> SpatializationAlgorithm;

	UPROPERTY()
	float RadiusMin_DEPRECATED;

	UPROPERTY()
	float RadiusMax_DEPRECATED;

	/* The dimensions to use for the attenuation shape. Interpretation of the values differ per shape.
	   Sphere  - X is Sphere Radius. Y and Z are unused
	   Capsule - X is Capsule Half Height, Y is Capsule Radius, Z is unused
	   Box     - X, Y, and Z are the Box's dimensions
	   Cone    - X is Cone Radius, Y is Cone Angle, Z is Cone Falloff Angle
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation)
	FVector AttenuationShapeExtents;

	/* The distance back from the sound's origin to begin the cone when using the cone attenuation shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation, meta=(ClampMin = "0"))
	float ConeOffset;

	/* The distance over which falloff occurs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attenuation, meta=(ClampMin = "0"))
	float FalloffDistance;

	/* The range at which to start applying a low pass filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LowPassFilter )
	float LPFRadiusMin;

	/* The range at which to apply the maximum amount of low pass filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LowPassFilter )
	float LPFRadiusMax;

	/* The Frequency in hertz at which to set the LPF when the sound is at LPFRadiusMin. (defaults to bypass) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LowPassFilter)
	float LPFFrequencyAtMin;

	/* The Frequency in hertz at which to set the LPF when the sound is at LPFRadiusMax. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LowPassFilter)
	float LPFFrequencyAtMax;

	/** Azimuth angle (in degrees) relative to the listener forward vector which defines the focus region of sounds. Sounds playing at an angle less than this will be in focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Focus, meta = (EditCondition = "bEnableListenerFocus"))
	float FocusAzimuth;

	/** Azimuth angle (in degrees) relative to the listener forward vector which defines the non-focus region of sounds. Sounds playing at an angle greater than this will be out of focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Focus, meta = (EditCondition = "bEnableListenerFocus"))
	float NonFocusAzimuth;

	/** Amount to scale the distance calculation of sounds that are in-focus. Can be used to make in-focus sounds appear to be closer or further away than they actually are. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Focus, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableListenerFocus"))
	float FocusDistanceScale;

	/** Amount to scale the distance calculation of sounds that are not in-focus. Can be used to make in-focus sounds appear to be closer or further away than they actually are.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Focus, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableListenerFocus"))
	float NonFocusDistanceScale;

	/** Amount to scale the priority of sounds that are in focus. Can be used to boost the priority of sounds that are in focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Focus, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableListenerFocus"))
	float FocusPriorityScale;

	/** Amount to scale the priority of sounds that are not in-focus. Can be used to reduce the priority of sounds that are not in focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Focus, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableListenerFocus"))
	float NonFocusPriorityScale;

	/** Amount to attenuate sounds that are in focus. Can be overridden at the sound-level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Focus, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableListenerFocus"))
	float FocusVolumeAttenuation;

	/** Amount to attenuate sounds that are not in focus. Can be overridden at the sound-level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Focus, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableListenerFocus"))
	float NonFocusVolumeAttenuation;

	/* Which trace channel to use for audio occlusion checks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attenuation)
	TEnumAsByte<enum ECollisionChannel> OcclusionTraceChannel;

	/** The low pass filter frequency (in hertz) to apply if the sound playing in this audio component is occluded. This will override the frequency set in LowPassFilterFrequency. A frequency of 0.0 is the device sample rate and will bypass the filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Occlusion, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableOcclusion"))
	float OcclusionLowPassFilterFrequency;

	/** The amount of volume attenuation to apply to sounds which are occluded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Occlusion, meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bEnableOcclusion"))
	float OcclusionVolumeAttenuation;

	/** The amount of time in seconds to interpolate to the target OcclusionLowPassFilterFrequency when a sound is occluded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Occlusion, meta = (ClampMin = "0", UIMin = "0.0", EditCondition = "bEnableOcclusion"))
	float OcclusionInterpolationTime;

	FAttenuationSettings()
		: bAttenuate(true)
		, bSpatialize(true)
		, bAttenuateWithLPF(false)
		, bEnableListenerFocus(false)
		, bEnableOcclusion(false)
		, bUseComplexCollisionForOcclusion(false)
		, DistanceAlgorithm(ATTENUATION_Linear)
		, DistanceType_DEPRECATED(SOUNDDISTANCE_Normal)
		, AttenuationShape(EAttenuationShape::Sphere)
		, dBAttenuationAtMax(-60.f)
		, OmniRadius(0.0f)
		, StereoSpread(0.0f)
		, SpatializationAlgorithm(ESoundSpatializationAlgorithm::SPATIALIZATION_Default)
		, RadiusMin_DEPRECATED(400.f)
		, RadiusMax_DEPRECATED(4000.f)
		, AttenuationShapeExtents(400.f, 0.f, 0.f)
		, ConeOffset(0.f)
		, FalloffDistance(3600.f)
		, LPFRadiusMin(3000.f)
		, LPFRadiusMax(6000.f)
		, LPFFrequencyAtMin(20000.f)
		, LPFFrequencyAtMax(20000.f)
		, FocusAzimuth(30.0f)
		, NonFocusAzimuth(60.0f)
		, FocusDistanceScale(1.0f)
		, NonFocusDistanceScale(1.0f)
		, FocusPriorityScale(1.0f)
		, NonFocusPriorityScale(1.0f)
		, FocusVolumeAttenuation(1.0f)
		, NonFocusVolumeAttenuation(1.0f)
		, OcclusionTraceChannel(ECC_Visibility)
		, OcclusionLowPassFilterFrequency(20000.f)
		, OcclusionVolumeAttenuation(1.0f)
		, OcclusionInterpolationTime(0.1f)
	{
	}

	bool operator==(const FAttenuationSettings& Other) const;
	void PostSerialize(const FArchive& Ar);

	struct AttenuationShapeDetails
	{
		FVector Extents;
		float Falloff;
		float ConeOffset;
	};

	void CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, AttenuationShapeDetails>& ShapeDetailsMap) const;
	float GetMaxDimension() const;
	float GetFocusPriorityScale(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const;
	float GetFocusAttenuation(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const;
	float GetFocusDistanceScale(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const;
	float AttenuationEval(const float Distance, const float Falloff, const float DistanceScale) const;
	float AttenuationEvalBox(const FTransform& SoundLocation, const FVector ListenerLocation, const float DistanceScale) const;
	float AttenuationEvalCapsule(const FTransform& SoundLocation, const FVector ListenerLocation, const float DistanceScale) const;
	float AttenuationEvalCone(const FTransform& SoundLocation, const FVector ListenerLocation, const float DistanceScale) const;
};

template<>
struct TStructOpsTypeTraits<FAttenuationSettings> : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithPostSerialize = true,
	};
};

/** 
 * Defines how a sound changes volume with distance to the listener
 */
UCLASS(BlueprintType, hidecategories=Object, editinlinenew, MinimalAPI)
class USoundAttenuation : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Settings)
	FAttenuationSettings Attenuation;
};
