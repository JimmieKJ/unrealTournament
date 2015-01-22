// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CameraModifier.generated.h"

//=============================================================================
/**
 * A CameraModifier is a base class for objects that may adjust the final camera properties after
 * being computed by the APlayerCameraManager (@see ModifyCamera). A CameraModifier
 * can be stateful, and is associated uniquely with a specific APlayerCameraManager.
 */
UCLASS()
class ENGINE_API UCameraModifier : public UObject
{
	GENERATED_UCLASS_BODY()

protected:
	/** If true, do not apply this modifier to the camera. */
	UPROPERTY()
	uint32 bDisabled:1;

public:
	/** If true, this modifier will disable itself when finished interpolating out. */
	UPROPERTY()
	uint32 bPendingDisable:1;

	/** Camera this object is associated with. */
	UPROPERTY()
	class APlayerCameraManager* CameraOwner;

protected:
	/** Priority value that determines the order in which modifiers are applied. 0 = highest priority, 255 = lowest. */
	UPROPERTY()
	uint8 Priority;

	/** If true, no other modifiers of same priority allowed. */
	UPROPERTY()
	uint32 bExclusive:1;

	/** When blending in, alpha proceeds from 0 to 1 over this time */
	UPROPERTY()
	float AlphaInTime;

	/** When blending out, alpha proceeds from 1 to 0 over this time */
	UPROPERTY()
	float AlphaOutTime;

	/** Current blend alpha. */
	UPROPERTY(transient)
	float Alpha;

	/** Desired alpha we are interpolating towards. */
	UPROPERTY(transient)
	float TargetAlpha;

public:
	/** If true, enables certain debug visualization features. */
	UPROPERTY(EditAnywhere, Category=Debug)
	uint32 bDebug:1;

protected:
	/** @return Returns the ideal blend alpha for this modifier. Interpolation will seek this value. */
	virtual float GetTargetAlpha(class APlayerCameraManager* Camera);

public:
	/** 
	 * Allows any custom initialization. Called immediately after creation.
	 * @param Camera - The camera this modifier should be associated with.
	 */
	virtual void Init( APlayerCameraManager* Camera );
	
	/**
	 * Directly modifies variables in the camera actor
	 * @param	Camera		reference to camera actor we are modifying
	 * @param	DeltaTime	Change in time since last update
	 * @param	InOutPOV		current Point of View, to be updated.
	 * @return	bool		true if should STOP looping the chain, false otherwise
	 */
	virtual bool ModifyCamera(APlayerCameraManager* Camera, float DeltaTime, struct FMinimalViewInfo& InOutPOV);
	
	/** @return Returns true if modifier is disabled, false otherwise. */
	virtual bool IsDisabled() const;
	
	/**
	 * Camera modifier evaluates itself vs the given camera's modifier list
	 * and decides whether to add itself or not. Handles adding by priority and avoiding 
	 * adding the same modifier twice.
	 *
	 * @param	Camera - reference to camera actor we want add this modifier to
	 * @return	Returns true if modifier added to camera's modifier list, false otherwise.
	 */
	virtual bool AddCameraModifier( APlayerCameraManager* Camera );
	
	/**
	 * Camera modifier removes itself from given camera's modifier list
	 * @param	Camera	- reference to camera actor we want to remove this modifier from
	 * @return	Returns true if modifier removed successfully, false otherwise.
	 */
	virtual bool RemoveCameraModifier( APlayerCameraManager* Camera );
	
	/** 
	 *  Disables this modifier.
	 *  @param  bImmediate  - true to disable with no blend out, false (default) to allow blend out
	 */
	virtual void DisableModifier(bool bImmediate = false);

	/** Enables this modifier. */
	virtual void EnableModifier();

	/** Toggled disabled/enabled state of this modifier. */
	virtual void ToggleModifier();
	
	/**
	 * Called to give modifiers a chance to adjust view rotation updates before they are applied.
	 *
	 * Default just returns ViewRotation unchanged
	 * @param ViewTarget - Current view target.
	 * @param DeltaTime - Frame time in seconds.
	 * @param OutViewRotation - In/out. The view rotation to modify.
	 * @param OutDeltaRot - In/out. How much the rotation changed this frame.
	 * @return Return true to prevent subsequent (lower priority) modifiers to further adjust rotation, false otherwise.
	 */
	virtual bool ProcessViewRotation(class AActor* ViewTarget, float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot);

	/**
	 * Responsible for updating alpha blend value.
	 *
	 * @param	Camera		- Camera that is being updated
	 * @param	DeltaTime	- Amount of time since last update
	 */
	virtual void UpdateAlpha( APlayerCameraManager* Camera, float DeltaTime );

	/** @return Returns the appropriate world context for this object. */
	UWorld* GetWorld() const;
};



