// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Base class for Camera Lens Effects.  Needed so we can have AnimNotifies be able to show camera effects
 * in a nice drop down
 *
 */

#pragma once
#include "Particles/Emitter.h"
#include "EmitterCameraLensEffectBase.generated.h"

UCLASS(abstract, Blueprintable, MinimalAPI)
class AEmitterCameraLensEffectBase : public AEmitter
{
	GENERATED_UCLASS_BODY()

protected:
	/** Particle System to use */
	UPROPERTY()
	class UParticleSystem* PS_CameraEffect;

	/** The effect to use for non extreme content */
	UPROPERTY()
	class UParticleSystem* PS_CameraEffectNonExtremeContent;

public:
	/** In order to get the particle effect looking correct we need to have a base FOV which we just to move the particle closer/further from the camera **/
	UPROPERTY()
	float BaseFOV;

	/** 
	 *  How far in front of the camera this emitter should live, assuming an FOV of 80 degrees. 
	 *  Note that the actual distance will be automatically adjusted to account for the actual FOV.
	 */
	UPROPERTY(EditAnywhere, Category=EmitterCameraLensEffectBase)
	float DistFromCamera;

	/** true if multiple instances of this emitter can exist simultaneously, false otherwise.  */
	UPROPERTY(EditAnywhere, Category=EmitterCameraLensEffectBase)
	uint32 bAllowMultipleInstances:1;

	/** 
	 *  If an emitter class in this array is currently playing, do not play this effect.
	 *  Useful for preventing multiple similar or expensive camera effects from playing simultaneously.
	 */
	UPROPERTY()
	TArray<TSubclassOf<class AEmitterCameraLensEffectBase> > EmittersToTreatAsSame;

protected:
	/** Camera this emitter is attached to, will be notified when emitter is destroyed */
	UPROPERTY(transient)
	class APlayerCameraManager* BaseCamera;

public:


	// Begin AActor Interface
	ENGINE_API virtual void Destroyed() override;
	ENGINE_API virtual void PostInitializeComponents() override;
	// End AActor Interface

	/** Tell the emitter what camera it is attached to. */
	ENGINE_API virtual void RegisterCamera(APlayerCameraManager* C);
	
	/** Called when this emitter is re-triggered, for bAllowMultipleInstances=false emitters. */
	ENGINE_API virtual void NotifyRetriggered();

	/** This will actually activate the lens Effect.  We want this separated from PostInitializeComponents so we can cache these emitters **/
	ENGINE_API virtual void ActivateLensEffect();
	
	/** Given updated camera information, adjust this effect to display appropriately. */
	ENGINE_API virtual void UpdateLocation(const FVector& CamLoc, const FRotator& CamRot, float CamFOVDeg);
};



