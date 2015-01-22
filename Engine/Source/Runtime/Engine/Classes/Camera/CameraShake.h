// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CameraShake.generated.h"

/************************************************************
 * Parameters for defining oscillating camera shakes
 ************************************************************/

/** Shake start offset parameter */
UENUM()
enum EInitialOscillatorOffset
{
	/** Start with random offset (default) */
	EOO_OffsetRandom,
	/** Start with zero offset. */
	EOO_OffsetZero,
	EOO_MAX,
};

/** Defines oscillation of a single number. */
USTRUCT()
struct FFOscillator
{
	GENERATED_USTRUCT_BODY()

	/** Amplitude of the sinusoidal oscillation. */
	UPROPERTY(EditAnywhere, Category=FOscillator)
	float Amplitude;

	/** Frequency of the sinusoidal oscillation. */
	UPROPERTY(EditAnywhere, Category = FOscillator)
	float Frequency;

	/** Defines how to begin (either at zero, or at a randomized value. */
	UPROPERTY(EditAnywhere, Category=FOscillator)
	TEnumAsByte<enum EInitialOscillatorOffset> InitialOffset;
	
	FFOscillator()
		: Amplitude(0)
		, Frequency(0)
		, InitialOffset(0)
	{}
};

/** Defines FRotator oscillation. */
USTRUCT()
struct FROscillator
{
	GENERATED_USTRUCT_BODY()

	/** Pitch oscillation. */
	UPROPERTY(EditAnywhere, Category=ROscillator)
	struct FFOscillator Pitch;

	/** Yaw oscillation. */
	UPROPERTY(EditAnywhere, Category = ROscillator)
	struct FFOscillator Yaw;

	/** Roll oscillation. */
	UPROPERTY(EditAnywhere, Category = ROscillator)
	struct FFOscillator Roll;

};

/** Defines FVector oscillation. */
USTRUCT()
struct FVOscillator
{
	GENERATED_USTRUCT_BODY()

	/** Oscillation in the X axis. */
	UPROPERTY(EditAnywhere, Category = VOscillator)
	struct FFOscillator X;

	/** Oscillation in the Y axis. */
	UPROPERTY(EditAnywhere, Category = VOscillator)
	struct FFOscillator Y;

	/** Oscillation in the Z axis. */
	UPROPERTY(EditAnywhere, Category = VOscillator)
	struct FFOscillator Z;

};


/**
 * A CameraShake is an asset that defines how to shake the camera in 
 * a particular way. CameraShakes can be authored as either oscillating shakes, 
 * animated shakes, or both.
 *
 * An oscillating shake will sinusoidally vibrate various camera parameters over time. Each location
 * and rotation axis can be oscillated independently with different parameters to create complex and
 * random-feeling shakes. These are easier to author and tweak, but can still feel mechanical and are
 * limited to vibration-style shakes, such as earthquakes.
 *
 * Animated shakes play keyframed camera animations.  These can take more effort to author, but enable
 * more natural-feeling results and things like directional shakes.  For instance, you can have an explosion
 * to the camera's right push it primarily to the left.
 */

UCLASS(Blueprintable, editinlinenew)
class ENGINE_API UCameraShake : public UObject
{
	GENERATED_UCLASS_BODY()

	/** 
	 *  If true to only allow a single instance of this shake to play at any given time. 
	 *  Subsequent attempts to play this shake will simply restart the timer.
	 */
	UPROPERTY(EditAnywhere, Category=CameraShake)
	uint32 bSingleInstance:1;

	/** Duration in seconds of current screen shake. Less than 0 means indefinite, 0 means no oscillation. */
	UPROPERTY(EditAnywhere, Category=Oscillation)
	float OscillationDuration;

	/** Duration of the blend-in, where the oscillation scales from 0 to 1. */
	UPROPERTY(EditAnywhere, Category=Oscillation, meta=(ClampMin = "0.0"))
	float OscillationBlendInTime;

	/** Duration of the blend-out, where the oscillation scales from 1 to 0. */
	UPROPERTY(EditAnywhere, Category = Oscillation, meta = (ClampMin = "0.0"))
	float OscillationBlendOutTime;

	/** Rotational oscillation */
	UPROPERTY(EditAnywhere, Category=Oscillation)
	struct FROscillator RotOscillation;

	/** Positional oscillation */
	UPROPERTY(EditAnywhere, Category=Oscillation)
	struct FVOscillator LocOscillation;

	/** FOV oscillation */
	UPROPERTY(EditAnywhere, Category=Oscillation)
	struct FFOscillator FOVOscillation;

	/************************************************************
	 * Parameters for defining CameraAnim-driven camera shakes
	 ************************************************************/

	/** Source camera animation to play. Can be null. */
	UPROPERTY(EditAnywhere, Category=AnimShake)
	class UCameraAnim* Anim;

	/** Scalar defining how fast to play the anim. */
	UPROPERTY(EditAnywhere, Category=AnimShake, meta=(ClampMin = "0.001"))
	float AnimPlayRate;

	/** Scalar defining how "intense" to play the anim. */
	UPROPERTY(EditAnywhere, Category=AnimShake, meta=(ClampMin = "0.0"))
	float AnimScale;

	/** Linear blend-in time. */
	UPROPERTY(EditAnywhere, Category=AnimShake, meta=(ClampMin = "0.0"))
	float AnimBlendInTime;

	/** Linear blend-out time. */
	UPROPERTY(EditAnywhere, Category=AnimShake, meta=(ClampMin = "0.0"))
	float AnimBlendOutTime;

	/**
	 * If true, play a random snippet of the animation of length Duration.  Implies bLoop and bRandomStartTime = true for the CameraAnim.
	 * If false, play the full anim once, non-looped. Useful for getting variety out of a single looped CameraAnim asset.
	 */
	UPROPERTY(EditAnywhere, Category=AnimShake)
	uint32 bRandomAnimSegment:1;

	/** When bRandomAnimSegment is true, this defines how long the anim should play. */
	UPROPERTY(EditAnywhere, Category=AnimShake, meta=(ClampMin = "0.0", editcondition = "bRandomAnimSegment"))
	float RandomAnimSegmentDuration;
};



