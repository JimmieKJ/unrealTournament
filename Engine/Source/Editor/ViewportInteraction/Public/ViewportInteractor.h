// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewportInteractorData.h"
#include "ViewportInteractor.generated.h"

/**
 * Represents the interactor in the world
 */
UCLASS( ABSTRACT )
class VIEWPORTINTERACTION_API UViewportInteractor : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Gets the private data for this interactor */
	struct FViewportInteractorData& GetInteractorData();

	/** Sets the world interaction */
	void SetWorldInteraction( class UViewportWorldInteraction* InWorldInteraction );

	/** Sets the other interactor */
	void SetOtherInteractor( UViewportInteractor* InOtherInteractor ); //@todo ViewportInteraction: This should not be public to other modules and should only be called from the world interaction.

	/** Removes the other interactor reference for this interactor */
	void RemoveOtherInteractor();

	/** Gets the paired interactor assigned by the world interaction, can return null when there is no other interactor */
	UViewportInteractor* GetOtherInteractor() const;

	/** Updates the hover state of this interactor */
	void UpdateHoverResult( FHitResult& HitResult );

	/** Whenever the ViewportWorldInteraction is shut down, the interacts will shut down as well */
	virtual void Shutdown();

	/** Update for this interactor called by the ViewportWorldInteraction */
	virtual void Tick( const float DeltaTime ) {};

	/** Gets the hover component from the interactor data */
	virtual class UActorComponent* GetHoverComponent();

	/** Adds a new action to the KeyToActionMap */
	void AddKeyAction( const FKey& Key, const FViewportActionKeyInput& Action );

	/** Removes an action from the KeyToActionMap */
	void RemoveKeyAction( const FKey& Key );

	/** Base classes need to implement getting the input for the input devices for that interactor */
	virtual void PollInput() {}

	/** Handles axis input and translates it actions */
	bool HandleInputKey( const FKey Key, const EInputEvent Event );

	/** Handles axis input and translates it to actions */
	bool HandleInputAxis( const FKey Key, const float Delta, const float DeltaTime );

	/** Gets the world transform of this interactor */
	FTransform GetTransform() const;

	/** Gets the current interactor data dragging mode */
	EViewportInteractionDraggingMode GetDraggingMode() const;

	/** Gets the interactor data previous dragging mode */
	EViewportInteractionDraggingMode GetLastDraggingMode() const;

	/** Gets the interactor data drag velocity */
	FVector GetDragTranslationVelocity() const;

	/**
	 * Gets the start and end point of the laser pointer for the specified hand
	 *
	 * @param HandIndex				Index of the hand to use
	 * @param LasertPointerStart	(Out) The start location of the laser pointer in world space
	 * @param LasertPointerEnd		(Out) The end location of the laser pointer in world space
	 * @param bEvenIfUIIsInFront	If true, returns a laser pointer even if the hand has UI in front of it (defaults to false)
	 * @param LaserLengthOverride	If zero the default laser length (VREdMode::GetLaserLength) is used
	 *
	 * @return	True if we have motion controller data for this hand and could return a valid result
	 */
	bool GetLaserPointer( FVector& LaserPointerStart, FVector& LaserPointerEnd, const bool bEvenIfBlocked = false, const float LaserLengthOverride = 0.0f );

	/** Gets the maximum length of a laser pointer */
	float GetLaserPointerMaxLength() const;

	/** Triggers a force feedback effect on device if possible */
	virtual void PlayHapticEffect( const float Strength ) {};

	/**
	 * Traces along the laser pointer vector and returns what it first hits in the world
	 *
	 * @param HandIndex	Index of the hand that uses has the laser pointer to trace
	 * @param OptionalListOfIgnoredActors Actors to exclude from hit testing
	 * @param bIgnoreGizmos True if no gizmo results should be returned, otherwise they are preferred (x-ray)
	 * @param bEvenIfUIIsInFront If true, ignores any UI that might be blocking the ray
	 * @param LaserLengthOverride If zero the default laser length (VREdMode::GetLaserLength) is used
	 *
	 * @return What the laster pointer hit
	 */
	virtual FHitResult GetHitResultFromLaserPointer( TArray<AActor*>* OptionalListOfIgnoredActors = nullptr, const bool bIgnoreGizmos = false,
		TArray<UClass*>* ObjectsInFrontOfGizmo = nullptr, const bool bEvenIfBlocked = false, const float LaserLengthOverride = 0.0f );

	/** Reset the values before checking the hover actions */
	virtual void ResetHoverState( const float DeltaTime ) {};

	/** Needs to be implemented by the base to calculate drag ray length and the velocity for the ray */
	virtual void CalculateDragRay() {};

	/**
	 * Creates a hand transform and forward vector for a laser pointer for a given hand
	 *
	 * @param HandIndex			Index of the hand to use
	 * @param OutHandTransform	The created hand transform
	 * @param OutForwardVector	The forward vector of the hand
	 *
	 * @return	True if we have motion controller data for this hand and could return a valid result
	 */
	virtual bool GetTransformAndForwardVector( FTransform& OutHandTransform, FVector& OutForwardVector );

	/** Called by StartDragging in world interaction to give the interator a chance of acting upon starting a drag operation */
	virtual void OnStartDragging( UActorComponent* ClickedComponent, const FVector& HitLocation, const bool bIsPlacingActors ) {};

	/** Gets the interactor laser hover location */
	FVector GetHoverLocation() const;

	/** If the interactor laser is currently hovering */
	bool IsHovering() const;

	/** If the interactor laser is currently hovering over a gizmo handle */
	bool IsHoveringOverGizmo();

	/** Sets the current dragging mode for this interactor */
	void SetDraggingMode( const EViewportInteractionDraggingMode NewDraggingMode );

	/** To be overridden by base class. Called by GetLaserPointer to give the derived interactor a chance to disable the laser. By default it is not blocked */
	virtual bool GetIsLaserBlocked() { return false; }

	/** Gets a certain action by iterating through the map looking for the same ActionType */
	// @todo ViewportInteractor: This should be changed to return a const pointer, but we need to fix up some dragging code in WorldInteraction first
	FViewportActionKeyInput* GetActionWithName( const FName InActionName );

	/** Gets the drag haptic feedback strength console variable */
	float GetDragHapticFeedbackStrength() const;

	/**
	 * Enables or disable "light press" locking for this interactor.  This is an advanced feature.
	 * It should be true if we allow locking of the lightly pressed state for the current event.  This can 
	 * be useful to turn off in cases where you always want a full press to override the light press, even 
	 * if it was a very slow press
	 *
	 * @param	bAllow		Enables or disables light press locking for this hand
	 */
	void SetAllowTriggerLightPressLocking( const bool bInAllow );

	/** @return Check if this interactor allows the light press to be locked */
	bool AllowTriggerLightPressLocking() const;

	/**
	 * When responding to a "light press" event, you can call this and pass false to to prevent a full press event from
	 * being possible at all.  Useful when you're not planning to use FullyPressed for anything, but you don't want a 
	 * full press to cancel your light press.
	 *
	 * @param	bInAllow	Whether to allow a full press to interrupt the light press.  Defaults to true, and will reset to true with every re-press of the trigger.
	 */
	void SetAllowTriggerFullPress( const bool bInAllow );

	/** @return Returns whether a full press is allowed to interrupt a light press. */
	bool AllowTriggerFullPress() const;


protected:

	/** To be overridden by base class. Called by HandleInputKey before delegates and default input implementation */
	virtual void HandleInputKey( FViewportActionKeyInput& Action, const FKey Key, const EInputEvent Event, bool& bOutWasHandled ) {};
	
	/** To be overridden by base class. Called by HandleInputAxis before delegates and default input implementation */
	virtual void HandleInputAxis( FViewportActionKeyInput& Action, const FKey Key, const float Delta, const float DeltaTime, bool& bOutWasHandled ) {};

protected:

	/** All the private data for the interactor */
	FViewportInteractorData InteractorData;
	
	/** Mapping of raw keys to actions*/
	TMap<FKey, FViewportActionKeyInput> KeyToActionMap;

	/** The owning world interaction */
	UPROPERTY()
	class UViewportWorldInteraction* WorldInteraction;

	/** The paired interactor by the world interaction */
	UPROPERTY()
	UViewportInteractor* OtherInteractor;
};