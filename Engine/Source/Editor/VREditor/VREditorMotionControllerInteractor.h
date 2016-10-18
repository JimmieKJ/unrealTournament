// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VREditorInteractor.h"
#include "HeadMountedDisplayTypes.h"	// For EHMDDeviceType::Type
#include "VREditorMotionControllerInteractor.generated.h"

/**
 * Represents the interactor in the world
 */
UCLASS()
class UVREditorMotionControllerInteractor : public UVREditorInteractor
{
	GENERATED_UCLASS_BODY()

public:

	virtual ~UVREditorMotionControllerInteractor();
	
	// UVREditorInteractor overrides
	virtual void Init( class FVREditorMode* InVRMode ) override;
	/** Gets the trackpad slide delta */
	virtual float GetSlideDelta() override;

	/** Sets up all components */
	void SetupComponent( AActor* OwningActor );

	/** Sets the EControllerHand for this motioncontroller */
	void SetControllerHandSide( const EControllerHand InControllerHandSide );

	// IViewportInteractorInterface overrides
	virtual void Shutdown() override;
	virtual void Tick( const float DeltaTime ) override;
	virtual void CalculateDragRay() override;

	/** @return Returns the type of HMD we're dealing with */
	EHMDDeviceType::Type GetHMDDeviceType() const;

	// UViewportInteractor
	virtual void HandleInputKey( FViewportActionKeyInput& Action, const FKey Key, const EInputEvent Event, bool& bOutWasHandled ) override;
	virtual bool GetTransformAndForwardVector( FTransform& OutHandTransform, FVector& OutForwardVector ) override;

	/** Gets the Y delta of the trackpad */
	float GetTrackpadSlideDelta();

	/** Starts haptic feedback for physical motion controller */
	virtual void PlayHapticEffect( const float Strength ) override;
	
	/** Stops playing any haptic effects that have been going for a while.  Called every frame. */
	void StopOldHapticEffects();
	
	/** Get the side of the controller */
	EControllerHand GetControllerSide() const;

	/** Get the motioncontroller component of this interactor */
	class UMotionControllerComponent* GetMotionControllerComponent() const;

	/** Check if the touchpad is currently touched */
	bool IsTouchingTrackpad() const;

	/** Get the current position of the trackpad or analog stick */
	FVector2D GetTrackpadPosition() const;

	/** Get the last position of the trackpad or analog stick */
	FVector2D GetLastTrackpadPosition() const;

	/** If the trackpad values are valid */
	bool IsTrackpadPositionValid( const int32 AxisIndex ) const;

	/** Get when the last time the trackpad position was updated */
	FTimespan& GetLastTrackpadPositionUpdateTime();

protected:

	// ViewportInteractor
	virtual void HandleInputAxis( FViewportActionKeyInput& Action, const FKey Key, const float Delta, const float DeltaTime, bool& bOutWasHandled ) override;

	/** Polls input for the motion controllers transforms */
	virtual void PollInput() override;

	/** Check if the trigger is lightly pressed */
	bool IsTriggerAtLeastLightlyPressed() const;

	/** Get the real time the trigger was lightly pressed */
	double GetRealTimeTriggerWasLightlyPressed() const;

	/** Set that the trigger is at least lighty pressed */
	void SetTriggerAtLeastLightlyPressed( const bool bInTriggerAtLeastLightlyPressed );

	/** Set that the trigger has been released since last press */
	void SetTriggerBeenReleasedSinceLastPress( const bool bInTriggerBeenReleasedSinceLastPress );

private:

	/** Changes the color of the buttons on the handmesh */
	void ApplyButtonPressColors( const FViewportActionKeyInput& Action );

	/** Set the visuals for a button on the motion controller */
	void SetMotionControllerButtonPressedVisuals( const EInputEvent Event, const FName& ParameterName, const float PressStrength );

	/** Pops up some help text labels for the controller in the specified hand, or hides it, if requested */
	void ShowHelpForHand( const bool bShowIt );

	/** Called every frame to update the position of any floating help labels */
	void UpdateHelpLabels();

	/** Given a mesh and a key name, tries to find a socket on the mesh that matches a supported key */
	UStaticMeshSocket* FindMeshSocketForKey( UStaticMesh* StaticMesh, const FKey Key );

	/** Sets the visuals of the LaserPointer */
	void SetLaserVisuals( const FLinearColor& NewColor, const float CrawlFade, const float CrawlSpeed );

	/** Updates the radial menu */
	void UpdateRadialMenuInput( const float DeltaTime );

protected:
	
	/** Motion controller component which handles late-frame transform updates of all parented sub-components */
	UPROPERTY()
	class UMotionControllerComponent* MotionControllerComponent;

	//
	// Graphics
	//

	/** Mesh for this hand */
	UPROPERTY()
	class UStaticMeshComponent* HandMeshComponent;

	/** Mesh for this hand's laser pointer */
	UPROPERTY()
	class UStaticMeshComponent* LaserPointerMeshComponent;

	/** MID for laser pointer material (opaque parts) */
	UPROPERTY()
	class UMaterialInstanceDynamic* LaserPointerMID;

	/** MID for laser pointer material (translucent parts) */
	UPROPERTY()
	class UMaterialInstanceDynamic* TranslucentLaserPointerMID;

	/** Hover impact indicator mesh */
	UPROPERTY()
	class UStaticMeshComponent* HoverMeshComponent;

	/** Hover point light */
	UPROPERTY()
	class UPointLightComponent* HoverPointLightComponent;

	/** MID for hand mesh */
	UPROPERTY()
	class UMaterialInstanceDynamic* HandMeshMID;

	/** Right or left hand */
	EControllerHand ControllerHandSide;

	/** True if this hand has a motion controller (or both!) */
	bool bHaveMotionController;

	// Special key action names for motion controllers
	static const FName TrackpadPositionX;
	static const FName TrackpadPositionY;
	static const FName TriggerAxis;
	static const FName MotionController_Left_FullyPressedTriggerAxis;
	static const FName MotionController_Right_FullyPressedTriggerAxis;
	static const FName MotionController_Left_LightlyPressedTriggerAxis;
	static const FName MotionController_Right_LightlyPressedTriggerAxis;

private:

	//
	// Trigger axis state
	//

	/** True if trigger is fully pressed right now (or within some small threshold) */
	bool bIsTriggerFullyPressed;

	/** True if the trigger is currently pulled far enough that we consider it in a "half pressed" state */
	bool bIsTriggerAtLeastLightlyPressed;

	/** Real time that the trigger became lightly pressed.  If the trigger remains lightly pressed for a reasonably 
	long duration before being pressed fully, we'll continue to treat it as a light press in some cases */
	double RealTimeTriggerWasLightlyPressed;

	/** True if trigger has been fully released since the last press */
	bool bHasTriggerBeenReleasedSinceLastPress;

	//
	// Haptic feedback
	//

	/** The last real time we played a haptic effect.  This is used to turn off haptic effects shortly after they're triggered. */
	double LastHapticTime;

	//
	// Trackpad support
	//

	/** True if the trackpad is actively being touched */
	bool bIsTouchingTrackpad;

	/** Position of the touched trackpad */
	FVector2D TrackpadPosition;

	/** Last position of the touched trackpad */
	FVector2D LastTrackpadPosition;

	/** True if we have a valid trackpad position (for each axis) */
	bool bIsTrackpadPositionValid[ 2 ];

	/** Real time that the last trackpad position was last updated.  Used to filter out stale previous data. */
	FTimespan LastTrackpadPositionUpdateTime;

};