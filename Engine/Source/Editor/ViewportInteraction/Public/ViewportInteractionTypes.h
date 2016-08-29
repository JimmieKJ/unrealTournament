// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewportInteractionTypes.generated.h"

/**
 * Represents a generic action
*/
USTRUCT()
struct VIEWPORTINTERACTION_API FViewportActionKeyInput
{
	GENERATED_BODY()
	
	FViewportActionKeyInput() :
		ActionType( NAME_None ),
		bIsInputCaptured( false )
	{}

	FViewportActionKeyInput( const FName& InActionType ) : 
		ActionType( InActionType ),
		bIsInputCaptured( false )
	{}

	/** The name of this action */
	FName ActionType;

	/** Input event */
	EInputEvent Event;

	/** True if this action owned by an interactor is "captured" for each possible action type, meaning that only the active captor should 
	handle input events until it is no longer captured.  It's the captors responsibility to set this using OnVRAction(), or clear it when finished with capturing. */
	bool bIsInputCaptured;
};


/** Methods of dragging objects around in VR */
UENUM()
enum class EViewportInteractionDraggingMode : uint8
{
	/** Not dragging right now with this hand */
	Nothing,

	/** Dragging actors around using the transform gizmo */
	ActorsWithGizmo,

	/** Actors locked to the impact point under the laser */
	ActorsAtLaserImpact,

	/** We're grabbing an object (or the world) that was already grabbed by the other hand */
	AssistingDrag,

	/** Freely moving, rotating and scaling objects with one or two hands */
	ActorsFreely,

	/** Moving the world itself around (actually, moving the camera in such a way that it feels like you're moving the world) */
	World,

	/** Moving a custom interactable */
	Interactable,

	/** Custom implementation for dragging */
	Custom,

	/** Dragging a material */
	Material
};

/**
* Things the transform gizmo can do to objects
*/
UENUM()
enum class ETransformGizmoInteractionType : uint8
{
	/** No interaction */
	None,

	/** Translate the object(s), either in any direction or confined to one axis.  No rotation. */
	Translate,

	/** Translate the object, constrained to a plane.  No rotation. */
	TranslateOnPlane,

	/** Stretch the object non-uniformly while simultaneously repositioning it so account for it's new size */
	StretchAndReposition,

	/** Scaling the object non-uniformly while	 */
	Scale,

	/** Rotate the object(s) around a specific axis relative to the gizmo's pivot, translating as needed */
	Rotate,

	/** Rotate the object(s) around the pivot based on start angle */
	RotateOnAngle,

	/** Uniform scale the object(s) */
	UniformScale,
};


/* Directions that a transform handle can face along any given axis */
enum class ETransformGizmoHandleDirection
{
	Negative,
	Center,
	Positive,
};


/** Placement of a handle in pivot space */
USTRUCT()
struct VIEWPORTINTERACTION_API FTransformGizmoHandlePlacement
{
	GENERATED_BODY()

	/* Handle direction in X, Y and Z */
	ETransformGizmoHandleDirection Axes[ 3 ];


	/** Finds the center handle count and facing axis index.  The center handle count is simply the number
	of axes where the handle would be centered on the bounds along that axis.  The facing axis index is
	the index (0-2) of the axis where the handle would be facing, or INDEX_NONE for corners or edges.
	The center axis index is valid for edges, and defines the axis perpendicular to that edge direction,
	or INDEX_NONE if it's not an edge */
	void GetCenterHandleCountAndFacingAxisIndex( int32& OutCenterHandleCount, int32& OutFacingAxisIndex, int32& OutCenterAxisIndex ) const;
};
