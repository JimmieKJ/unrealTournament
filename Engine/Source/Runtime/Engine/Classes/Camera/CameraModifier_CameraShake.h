// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Camera modifier that provides support for code-based oscillating camera shakes.
 */

#pragma once

#include "Camera/CameraModifier.h"
#include "CameraModifier_CameraShake.generated.h"

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
	/** List of active CameraShake instances */
	UPROPERTY()
	TArray<class UCameraShake*> ActiveShakes;

	/** 
	 * Adds a new active screen shake to be applied. 
	 * @param NewShake - The class of camera shake to instantiate.
	 * @param Scale - The scalar intensity to play the shake.
	 * @param PlaySpace - Which coordinate system to play the shake in.
	 * @param UserPlaySpaceRot - Coordinate system to play shake when PlaySpace == CAPS_UserDefined.
	 */
	virtual class UCameraShake* AddCameraShake(TSubclassOf<class UCameraShake> NewShake, float Scale, ECameraAnimPlaySpace::Type PlaySpace=ECameraAnimPlaySpace::CameraLocal, FRotator UserPlaySpaceRot = FRotator::ZeroRotator);
	
	/**
	 * Stops and removes the camera shake of the given class from the camera.
	 * @param Shake - the camera shake class to remove.
	 */
	virtual void RemoveCameraShake(UCameraShake* ShakeInst);

	/** Stops and removes all camera shakes of the given class from the camera. */
	virtual void RemoveAllCameraShakesOfClass(TSubclassOf<class UCameraShake> ShakeClass);

	/** Stops and removes all camera shakes from the camera. */
	virtual void RemoveAllCameraShakes();
	
	// Begin UCameraModifer Interface
	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;
	// End UCameraModifer Interface

protected:
	/** Scaling factor applied to all camera shakes in when in splitscreen mode. Normally used to reduce shaking, since shakes feel more intense in a smaller viewport. */
	UPROPERTY(EditAnywhere, Category = CameraModifier_CameraShake)
	float SplitScreenShakeScale;
};
