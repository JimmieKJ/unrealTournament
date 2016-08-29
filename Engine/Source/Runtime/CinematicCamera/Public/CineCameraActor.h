// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CineCameraComponent.h"

#include "CineCameraActor.generated.h"

/** Settings to control the camera's lookat feature */
USTRUCT()
struct FCameraLookatTrackingSettings
{
	GENERATED_USTRUCT_BODY()

	/** True to enable lookat tracking, false otherwise. */
	UPROPERTY(/*Interp, */EditAnywhere, BlueprintReadWrite, Category = "LookAt")
	uint8 bEnableLookAtTracking : 1;

	/** True to draw a debug representation of the lookat location */
	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, Category = "LookAt")
	uint8 bDrawDebugLookAtTrackingPosition : 1;

	/** Controls degree of smoothing. 0.f for no smoothing, higher numbers for faster/tighter tracking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LookAt")
	float LookAtTrackingInterpSpeed;

	/** Last known lookat tracking rotation (used during interpolation) */
	FRotator LastLookatTrackingRotation;

	/** If set, camera will track this actor's location */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "LookAt")
	AActor* ActorToTrack;

	/** Offset from actor position to look at. Relative to actor if tracking an actor, relative to world otherwise. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "LookAt")
	FVector RelativeOffset;
};

/** 
 * A CineCameraActor is a CameraActor specialized to work like a cinematic camera.
 */
UCLASS(ClassGroup = Common, hideCategories = (Input, Rendering, AutoPlayerActivation), showcategories = ("Input|MouseInput", "Input|TouchInput"), Blueprintable)
class CINEMATICCAMERA_API ACineCameraActor : public ACameraActor
{
	GENERATED_BODY()

public:
	// Ctor
	ACineCameraActor(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void PostInitializeComponents() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	FCameraLookatTrackingSettings LookatTrackingSettings;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	/** Set to true to skip any interpolations on the next update. Resets to false automatically. */
	uint8 bResetInterplation : 1;

	FVector GetLookatLocation() const;

	virtual void NotifyCameraCut() override;

	bool ShouldTickForTracking() const;

private:
	/** Returns CineCameraComponent subobject **/
	class UCineCameraComponent* CineCameraComponent;
};
