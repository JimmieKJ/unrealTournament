// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Camera modifier that provides support for code-based oscillating camera shakes.
 */

#pragma once

#include "Camera/CameraModifier.h"
#include "CameraModifier_CameraShake.generated.h"

/** 
 * A CameraShakeInstance is an active instance of a shake asset. It contains
 * the necessary state information to smoothly update this shake over time.
 */
USTRUCT()
struct ENGINE_API FCameraShakeInstance
{
	GENERATED_USTRUCT_BODY()

	/** The source camera shake data. */
	UPROPERTY()
	const class UCameraShake* SourceShake;

	/** Optional name of shake. Used to identify shakes when single instances are desired. */
	UPROPERTY()
	FName SourceShakeName;

	/** Time remaining for oscillation shakes. Less than 0.f means shake infinitely. */
	UPROPERTY()
	float OscillatorTimeRemaining;

	/** True if this shake is currently blending in. */
	UPROPERTY()
	uint32 bBlendingIn:1;

	/** How long this instance has been blending in. */
	UPROPERTY()
	float CurrentBlendInTime;

	/** True if this shake is currently blending out. */
	UPROPERTY()
	uint32 bBlendingOut:1;

	/** How long this instance has been blending out. */
	UPROPERTY()
	float CurrentBlendOutTime;

	/** Current location sinusoidal offset. */
	UPROPERTY()
	FVector LocSinOffset;

	/** Current rotational sinusoidal offset. */
	UPROPERTY()
	FVector RotSinOffset;

	/** Current FOV sinusoidal offset. */
	UPROPERTY()
	float FOVSinOffset;

	/** Overall intensity scale for this shake instance. */
	UPROPERTY()
	float Scale;

	/** The playing instance of the CameraAnim-based shake, if any. */
	UPROPERTY()
	class UCameraAnimInst* AnimInst;

	/** What space to play the shake in before applying to the camera.  Affects both Anim and Oscillation shakes. */
	UPROPERTY()
	TEnumAsByte<ECameraAnimPlaySpace::Type> PlaySpace;

	/** Matrix defining the playspace, used when PlaySpace == CAPS_UserDefined */
	UPROPERTY()
	FMatrix UserPlaySpaceMatrix;

	FCameraShakeInstance()
		: SourceShake(NULL)
		, OscillatorTimeRemaining(0)
		, bBlendingIn(false)
		, CurrentBlendInTime(0)
		, bBlendingOut(false)
		, CurrentBlendOutTime(0)
		, LocSinOffset(ForceInit)
		, RotSinOffset(ForceInit)
		, FOVSinOffset(0)
		, Scale(0)
		, AnimInst(NULL)
		, PlaySpace(0)
		, UserPlaySpaceMatrix(ForceInit)
	{}

};

//=============================================================================
/**
 * A UCameraModifier_CameraShake is a camera modifier that can apply a UCameraShake to 
 * the owning camera.
 */
UCLASS(config=Camera)
class ENGINE_API UCameraModifier_CameraShake : public UCameraModifier
{
	GENERATED_UCLASS_BODY()

public:
	/** List of active CameraShakes */
	UPROPERTY()
	TArray<struct FCameraShakeInstance> ActiveShakes;

protected:
	/** Scaling factor applied to all camera shakes in when in splitscreen mode. Normally used to dampen, since shakes feel more intense in a smaller viewport. */
	UPROPERTY(EditAnywhere, Category=CameraModifier_CameraShake)
	float SplitScreenShakeScale;

	/** @return Returns the current overall scaling factor for this shake. */
	virtual float GetShakeScale(FCameraShakeInstance const& ShakeInst) const;
	
	/** 
	 * Restarts a playing camerashake from the beginning. 
	 * @param ShakeInst - The shake instance to restart.
	 * @param Scale - The base scale to restart at.
	 */
	virtual void ReinitShake(FCameraShakeInstance& ShakeInst, float Scale);

	/** @return Returns an initial offset for the given oscillator parameter. */
	float InitializeOffset( const struct FFOscillator& Param ) const;
	
	/** 
	 * Internal. Creates a new camera shake instance.
	 * @param NewShake - The class of camera shake to instantiate.
	 * @param Scale - The scalar intensity to play the shake.
	 * @param PlaySpace - Which coordinate system to play the shake in.
	 * @param UserPlaySpaceRot - Coordinate system to play shake when PlaySpace == CAPS_UserDefined.
	 * @return Returns the new camera shake instance.
	 */
	virtual FCameraShakeInstance InitializeShake(TSubclassOf<class UCameraShake> NewShake, float Scale, ECameraAnimPlaySpace::Type PlaySpace, FRotator UserPlaySpaceRot = FRotator::ZeroRotator);
	
public:
	/** 
	 * Adds a new active screen shake to be applied. 
	 * @param NewShake - The class of camera shake to instantiate.
	 * @param Scale - The scalar intensity to play the shake.
	 * @param PlaySpace - Which coordinate system to play the shake in.
	 * @param UserPlaySpaceRot - Coordinate system to play shake when PlaySpace == CAPS_UserDefined.
	 */
	virtual void AddCameraShake(TSubclassOf<class UCameraShake> NewShake, float Scale, ECameraAnimPlaySpace::Type PlaySpace=ECameraAnimPlaySpace::CameraLocal, FRotator UserPlaySpaceRot = FRotator::ZeroRotator);
	
	/**
	 * Stops and removes the camera shake of the given class from the camera.
	 * @param Shake - the camera shake class to remove.
	 */
	virtual void RemoveCameraShake(TSubclassOf<class UCameraShake> Shake);

	/** Stops and removes all camera shakes from the camera. */
	virtual void RemoveAllCameraShakes();
	
	/** 
	 * Called per-tick to update a CameraShake.
	 * @param DeltaTime - Simulation timeslice, in seconds.
	 * @param ShakeInst - The shake instance to update.
	 * @param InOutPOV - the POV data to update with the results of the shake.
	 */
	virtual void UpdateCameraShake(float DeltaTime, FCameraShakeInstance& ShakeInst, struct FMinimalViewInfo& InOutPOV);
	
	// Begin UCameraModifer Interface
	virtual bool ModifyCamera(APlayerCameraManager* Camera, float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;
	// End UCameraModifer Interface
};
