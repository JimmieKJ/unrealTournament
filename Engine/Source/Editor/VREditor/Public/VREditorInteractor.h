// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/ViewportInteraction/Public/ViewportInteractor.h"
#include "VREditorInteractor.generated.h"

/**
 * VREditor default interactor
 */
UCLASS()
class VREDITOR_API UVREditorInteractor : public UViewportInteractor
{
	GENERATED_UCLASS_BODY()

public:

	virtual ~UVREditorInteractor();

	/** Initialize default values */
	virtual void Init( class FVREditorMode* InVRMode );

	/** Gets the owner of this system */
	class FVREditorMode& GetVRMode()
	{
		return *VRMode;
	}

	/** Gets the owner of this system (const) */
	const class FVREditorMode& GetVRMode() const
	{
		return *VRMode;
	}

	// ViewportInteractorInterface overrides
	virtual void Shutdown() override;
	virtual void Tick( const float DeltaTime ) override;
	virtual FHitResult GetHitResultFromLaserPointer( TArray<AActor*>* OptionalListOfIgnoredActors = nullptr, const bool bIgnoreGizmos = false,
		TArray<UClass*>* ObjectsInFrontOfGizmo = nullptr, const bool bEvenIfBlocked = false, const float LaserLengthOverride = 0.0f ) override;
	virtual void ResetHoverState( const float DeltaTime ) override;
	virtual void OnStartDragging( UActorComponent* ClickedComponent, const FVector& HitLocation, const bool bIsPlacingActors ) override;

	/** Returns the slide delta for pushing and pulling objects. Needs to be implemented by derived classes (e.g. touchpad for vive controller or scrollweel for mouse ) */
	virtual float GetSlideDelta();

	//
	// Getters and setters
	//

	/** Gets if this interactor is hovering over UI */
	bool IsHoveringOverUI() const;
	
	/** Sets if there is a docked window on this interactor */
	void SetHasUIInFront( const bool bInHasUIInFront );
	
	/** Check if there is any docked window on this interactor */
	bool HasUIInFront() const;

	/** Sets if the quick menu is on this interactor */
	void SetHasUIOnForearm( const bool bInHasUIOnForearm );
	
	/** Check if the quick menu is on this interactor */
	bool HasUIOnForearm() const;

	/** Gets the current hovered widget component if any */
	UWidgetComponent* GetHoveringOverWidgetComponent() const;

	/** Sets the current hovered widget component */
	void SetHoveringOverWidgetComponent( UWidgetComponent* NewHoveringOverWidgetComponent );

	/** If the modifier key is currently pressed */
	bool IsModifierPressed() const;

	/** Sets if the interactor is clicking on any UI */
	void SetIsClickingOnUI( const bool bInIsClickingOnUI );
	
	/** Gets if the interactor is clicking on any UI */
	bool IsClickingOnUI() const;

	/** Sets if the interactor is hovering over any UI */
	void SetIsHoveringOverUI( const bool bInIsHoveringOverUI );

	/** Sets if the interactor is right  hover over any UI */
	void SetIsRightClickingOnUI( const bool bInIsRightClickingOnUI );

	/** Gets if the interactor is right clicking on UI */
	bool IsRightClickingOnUI() const;

	/** Gets if the interactor is right clicking on UI */
	void SetLastClickReleaseTime( const double InLastClickReleaseTime );

	/** Gets last time the interactor released */
	double GetLastClickReleaseTime() const;

	/** Sets the UI scroll velocity */
	void SetUIScrollVelocity( const float InUIScrollVelocity );

	/** Gets the UI scroll velocity */
	float GetUIScrollVelocity() const;

	/** Gets the trigger value */
	float GetSelectAndMoveTriggerValue() const;
	
	/* ViewportInteractor overrides, checks if the laser is blocked by UI */
	virtual bool GetIsLaserBlocked() override;
	
protected:

	/** The mode that owns this interactor */
	class FVREditorMode* VRMode;

	//
	// General input @todo: VREditor: Should this be general (non-UI) in interactordata ?
	//

	/** Is the Modifier button held down? */
	bool bIsModifierPressed;

	/** Current trigger pressed amount for 'select and move' (0.0 - 1.0) */
	float SelectAndMoveTriggerValue;

private:

	//
	// UI
	//

	/** True if a floating UI panel is attached to the front of the hand, and we shouldn't bother drawing a laser
	pointer or enabling certain other features */
	bool bHasUIInFront;

	/** True if a floating UI panel is attached to our forearm, so we shouldn't bother drawing help labels */
	bool bHasUIOnForearm;

	/** True if we're currently holding the 'SelectAndMove' button down after clicking on UI */
	bool bIsClickingOnUI;

	/** When bIsClickingOnUI is true, this will be true if we're "right clicking".  That is, the Modifier key was held down at the time that the user clicked */
	bool bIsRightClickingOnUI;

	 /** True if we're hovering over UI right now.  When hovering over UI, we don't bother drawing a see-thru laser pointer */
	bool bIsHoveringOverUI;
	
	/** Inertial scrolling -- how fast to scroll the mousewheel over UI */
	float UIScrollVelocity;	

	/** Last real time that we released the 'SelectAndMove' button on UI.  This is used to detect double-clicks. */
	double LastClickReleaseTime;

protected:

	//
	// Help
	//

	/** True if we want help labels to be visible right now, otherwise false */
	bool bWantHelpLabels;

	/** Help labels for buttons on the motion controllers */
	TMap< FKey, class AFloatingText* > HelpLabels;

	/** Time that we either started showing or hiding help labels (for fade transitions) */
	FTimespan HelpLabelShowOrHideStartTime;
};