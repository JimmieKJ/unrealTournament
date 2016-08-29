// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewportInteractionTypes.h"
#include "ViewportInteractionTypes.h"

/** Represents a single virtual hand */
struct FViewportInteractorData
{
	//
	// Positional data
	//

	/** Your hand in the virtual world in world space, usually driven by VR motion controllers */
	FTransform Transform;

	/** Your hand transform, in the local tracking space */
	FTransform RoomSpaceTransform;

	/** Hand transform in world space from the previous frame */
	FTransform LastTransform;

	/** Room space hand transform from the previous frame */
	FTransform LastRoomSpaceTransform;

	//
	// Hover feedback
	//

	/** Lights up when our laser pointer is close enough to something to click */
	bool bIsHovering;

	/** The widget component we are currently hovering over */
	class UWidgetComponent* HoveringOverWidgetComponent; //@todo: ViewportInteraction: UI should not be in this module

	/** Position the laser pointer impacted an interactive object at (UI, meshes, etc.) */
	FVector HoverLocation;

	/** The current component hovered by the laser pointer of this hand */
	TWeakObjectPtr<class UActorComponent> HoveredActorComponent;

	//
	// General input
	//

	/** True if we're currently holding the 'SelectAndMove' button down after clicking on an actor */
	TWeakObjectPtr<UActorComponent> ClickingOnComponent;

	/** Last real time that we pressed the 'SelectAndMove' button on UI.  This is used to detect double-clicks. */
	double LastClickPressTime;


	//
	// Object/world movement
	// 

	/** What we're currently dragging with this hand, if anything */
	EViewportInteractionDraggingMode DraggingMode;

	/** What we were doing last.  Used for inertial movement. */
	EViewportInteractionDraggingMode LastDraggingMode;

	/** True if this is the first update since we started dragging */
	bool bIsFirstDragUpdate;

	/** True if we were assisting the other hand's drag the last time we did anything.  This is used for inertial movement */
	bool bWasAssistingDrag;

	/** Length of the ray that's dragging */
	float DragRayLength;

	/** Laser pointer start location the last frame */
	FVector LastLaserPointerStart;

	/** Location that we dragged to last frame (end point of the ray) */
	FVector LastDragToLocation;

	/** Laser pointer impact location at the drag start */
	FVector LaserPointerImpactAtDragStart;

	/** How fast to move selected objects every frame for inertial translation */
	FVector DragTranslationVelocity;

	/** How fast to adjust ray length every frame for inertial ray length changes */
	float DragRayLengthVelocity;

	/** While dragging, true if we're dragging at least one simulated object that we're driving the velocities of.  When this
	    is true, our default inertia system is disabled and we rely on the physics engine to take care of inertia */
	bool bIsDrivingVelocityOfSimulatedTransformables;


	//
	// Transform gizmo interaction
	//

	/** Where the gizmo was placed at the beginning of the current interaction */
	FTransform GizmoStartTransform;

	/** Our gizmo bounds at the start of the interaction, in actor local space. */
	FBox GizmoStartLocalBounds;

	/** For a single axis drag, this is the cached local offset where the laser pointer ray intersected the axis line on the first frame of the drag */
	FVector GizmoSpaceFirstDragUpdateOffsetAlongAxis;

	/** When dragging with an axis/plane constraint applied, this is the difference between the actual "delta from start" and the constrained "delta from start".
		This is used when the user releases the object and inertia kicks in */
	FVector GizmoSpaceDragDeltaFromStartOffset;

	/** The gizmo interaction we're doing with this hand */
	ETransformGizmoInteractionType TransformGizmoInteractionType;

	/** Which handle on the gizmo we're interacting with, if any */
	TOptional<FTransformGizmoHandlePlacement> OptionalHandlePlacement;

	/** The gizmo component we're dragging right now */
	TWeakObjectPtr<class USceneComponent> DraggingTransformGizmoComponent;

	/** Gizmo component that we're hovering over, or nullptr if not hovering over any */
	TWeakObjectPtr<class USceneComponent> HoveringOverTransformGizmoComponent;

	/** Gizmo handle that we hovered over last (used only for avoiding spamming of hover haptics!) */
	TWeakObjectPtr<class USceneComponent> HoverHapticCheckLastHoveredGizmoComponent;

	//@todo ViewportInteraction: Light press should not have to be here, ViewportInteractor needs it
	/** True if we allow locking of the lightly pressed state for the current event.  This can be useful to
	turn off in cases where you always want a full press to override the light press, even if it was
	a very slow press */
	bool bAllowTriggerLightPressLocking;

	/** This can be set to false when handling SelectAndMove_LightlyPressed to prevent a full press event from
	    being possible at all.  Useful when you're not planning to use FullyPressed for anything, but you
		don't want a full press to cancel your light press. */
	bool bAllowTriggerFullPress;

	/** Default constructor for FVirtualHand that initializes safe defaults */
	FViewportInteractorData()
	{
		bIsHovering = false;
		HoveringOverWidgetComponent = nullptr;
		HoverLocation = FVector::ZeroVector;
		HoveredActorComponent = nullptr;

		ClickingOnComponent = nullptr;

		DraggingMode = EViewportInteractionDraggingMode::Nothing;
		LastDraggingMode = EViewportInteractionDraggingMode::Nothing;
		bWasAssistingDrag = false;
		bIsFirstDragUpdate = false;
		DragRayLength = 0.0f;
		LastLaserPointerStart = FVector::ZeroVector;
		LastDragToLocation = FVector::ZeroVector;
		LaserPointerImpactAtDragStart = FVector::ZeroVector;
		DragTranslationVelocity = FVector::ZeroVector;
		DragRayLengthVelocity = 0.0f;
		bIsDrivingVelocityOfSimulatedTransformables = false;

		GizmoStartTransform = FTransform::Identity;
		GizmoStartLocalBounds = FBox( 0 );
		GizmoSpaceFirstDragUpdateOffsetAlongAxis = FVector::ZeroVector;
		GizmoSpaceDragDeltaFromStartOffset = FVector::ZeroVector;
		TransformGizmoInteractionType = ETransformGizmoInteractionType::None;
		OptionalHandlePlacement.Reset();
		DraggingTransformGizmoComponent = nullptr;
		HoveringOverTransformGizmoComponent = nullptr;
		HoverHapticCheckLastHoveredGizmoComponent = nullptr;

		bAllowTriggerLightPressLocking = true;
		bAllowTriggerFullPress = true;
	}
};


