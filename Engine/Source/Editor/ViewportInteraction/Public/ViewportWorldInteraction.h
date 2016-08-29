// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewportWorldInteractionInterface.h"
#include "ViewportInteractionTypes.h"
#include "ViewportInteractionInputProcessor.h"
#include "ViewportWorldInteraction.generated.h"

namespace ViewportWorldActionTypes
{
	static const FName NoAction( "NoAction" );
	static const FName WorldMovement( "WorldMovement" );
	static const FName SelectAndMove( "SelectAndMove" );
	static const FName SelectAndMove_LightlyPressed( "SelectAndMove_LightlyPressed" );
	static const FName Undo( "Undo" );
	static const FName Redo( "Redo" );
	static const FName Delete( "Delete" );
}

// Forward declare the GizmoHandleTypes
enum class EGizmoHandleTypes : uint8;
class IViewportInteractableInterface;
class UViewportInteractor;

/**
* Represents an object that we're actively interacting with, such as a selected actor
*/
class FViewportTransformable
{

public:

	/** Sets up safe defaults */
	FViewportTransformable()
		: Object(),
		StartTransform( FTransform::Identity ),
		LastTransform( FTransform::Identity ),
		TargetTransform( FTransform::Identity ),
		UnsnappedTargetTransform( FTransform::Identity ),
		InterpolationSnapshotTransform( FTransform::Identity )
	{
	}

	/** The actual object */
	FWeakObjectPtr Object;

	/** The object's position when we started the action */
	FTransform StartTransform;

	/** The last position we were at.  Used for smooth interpolation when snapping is enabled */
	FTransform LastTransform;

	/** The target position we want to be at.  Used for smooth interpolation when snapping is enabled */
	FTransform TargetTransform;

	/** The target position we'd be at if snapping wasn't enabled.  This is used for 'elastic' smoothing when
	snapping is turned on.  Basically it allows us to 'reach' toward the desired position */
	FTransform UnsnappedTargetTransform;

	/** A "snapshot" transform, used for certain interpolation effects */
	FTransform InterpolationSnapshotTransform;
};

UCLASS()
class VIEWPORTINTERACTION_API UViewportWorldInteraction : public UObject, public IViewportWorldInteractionInterface
{
	GENERATED_UCLASS_BODY()
	
public:

	~UViewportWorldInteraction();

	// IViewportWorldInteraction overrides
	virtual void Init( const TSharedPtr<class FEditorViewportClient>& InEditorViewportClient ) override;
	virtual void Shutdown() override;
	virtual void Tick( class FEditorViewportClient* ViewportClient, const float DeltaTime ) override;
	virtual void AddInteractor( UViewportInteractor* Interactor ) override;
	virtual void RemoveInteractor( UViewportInteractor* Interactor ) override;
	virtual FOnVIHoverUpdate& OnViewportInteractionHoverUpdate() override
	{
		return OnHoverUpdateEvent;
	}
	virtual FOnVIActionHandle& OnViewportInteractionInputAction() override
	{
		return OnInputActionEvent;
	}

	/** Pairs to interactors by setting	the other interactor for each interactor */
	void PairInteractors( UViewportInteractor* FirstInteractor, UViewportInteractor* SecondInteractor );
	
	//
	// Input
 	//

	/** Handles the key input from the InputProcessor*/
	bool HandleInputKey( const FKey Key, const EInputEvent Event );

	/** Handles the axis input from the InputProcessor*/
	bool HandleInputAxis( const int32 ControllerId, const FKey Key, const float Delta, const float DeltaTime );

	/** Gets the world space transform of the calibrated VR room origin.  When using a seated VR device, this will feel like the
	camera's world transform (before any HMD positional or rotation adjustments are applied.) */
	FTransform GetRoomTransform() const;
	
	/** Gets the transform of the viewport / user's HMD in room space */
	FTransform GetRoomSpaceHeadTransform() const;
	
	/** Gets the transform of the viewport / user's HMD in world space */
	FTransform GetHeadTransform() const;

	/** Sets a new transform for the room, in world space.  This is basically setting the editor's camera transform for the viewport */
	void SetRoomTransform( const FTransform& NewRoomTransform );
	
	/** Gets the world scale factor, which can be multiplied by a scale vector to convert to room space */
	float GetWorldScaleFactor() const;

	/** Gets the world of the viewport used for this worldinteraction */
	UWorld* GetViewportWorld() const;

	/** Gets the currently used viewport */
	FEditorViewportClient* GetViewportClient() const;

	/** Returns the Unreal controller ID for the motion controllers we're using */
	int32 GetMotionControllerID() const
	{
		return MotionControllerID;
	}

	/** Sets that the worldinteractor is used in VR, to give a correct head transform */
	void SetIsUsingVR( const bool bInIsUsingVR );

	/** Invokes the editor's undo system to undo the last change */
	void Undo();

	/** Redoes the last change */
	void Redo();

	/** Deletes all the selected objects */
	void DeleteSelectedObjects();

	/** Copies selected objects to the clipboard */
	void Copy();

	/** Pastes the clipboard contents into the scene */
	void Paste();

	/** Duplicates the selected objects */
	void Duplicate();

	/** Deselects all the current selected objects */
	void Deselect();
	
	/** Called to finish a drag action with the specified interactor */
	virtual void StopDragging( class UViewportInteractor* Interactor );

	/** Starts dragging selected actors around, changing selection if needed.  Called when clicking in the world, or when placing new objects */
	void StartDraggingActors( UViewportInteractor* Interactor, const FViewportActionKeyInput& Action, UActorComponent* ClickedComponent, const FVector& HitLocation, const bool bIsPlacingActors );
	
	/** Sets which transform gizmo coordinate space is used */
	void SetTransformGizmoCoordinateSpace( const ECoordSystem NewCoordSystem );

	/** Returns which transform gizmo coordinate space we're using, world or local */
	ECoordSystem GetTransformGizmoCoordinateSpace() const;

	/** Returns the maximum user scale */
	float GetMaxScale();

	/** Returns the minimum user scale */
	float GetMinScale();

	/** Sets GNewWorldToMetersScale */
	void SetWorldToMetersScale( const float NewWorldToMetersScale );

	/** True if we're actively interacting with the specified actor */
	bool IsTransformingActor( AActor* Actor ) const;

	/** Checks to see if we're allowed to interact with the specified component */
	virtual bool IsInteractableComponent( const UActorComponent* Component ) const;

	/** Gets the transform gizmo actor, or returns null if we currently don't have one */
	class ABaseTransformGizmo* GetTransformGizmoActor();

	/** Sets whether the transform gizmo should be visible at all */
	void SetTransformGizmoVisible( const bool bShouldBeVisible );

	/** Returns whether the transform gizmo is visible */
	bool IsTransformGizmoVisible() const;

	/** Sets if objects are dragged with either hand since last time selecting something */
	void SetDraggedSinceLastSelection( const bool bNewDraggedSinceLastSelection );

	/** Sets the transform of the gizmo when starting drag */
	void SetLastDragGizmoStartTransform( const FTransform NewLastDragGizmoStartTransform );

	/** Gets all the interactors */
	TArray<UViewportInteractor*>& GetInteractors();

	/** Given a world space velocity vector, applies inertial damping to it to slow it down */
	void ApplyVelocityDamping( FVector& Velocity, const bool bVelocitySensitive );

	/** Gets the current Gizmo handle type */
	EGizmoHandleTypes GetCurrentGizmoType() const;

	/** Sets the current gizmo handle type */
	void SetGizmoHandleType( const EGizmoHandleTypes InGizmoHandleType );

	/** Changes the transform gizmo class that it will change to next refresh */
	void SetTransformGizmoClass( const TSubclassOf<ABaseTransformGizmo>& NewTransformGizmoClass );

	/** Sets the currently dragged interactavle */
	void SetDraggedInteractable( IViewportInteractableInterface* InDraggedInteractable );	

	/** Check if there is another interactor hovering over the component */
	bool IsOtherInteractorHoveringOverComponent( UViewportInteractor* Interactor, UActorComponent* Component ) const;
	
	/** Switch which transform gizmo coordinate space we're using. */
	void CycleTransformGizmoCoordinateSpace();

	/** Destroys a transient actor we created earlier and nulls out the pointer */
	void DestroyTransientActor(AActor* Actor) const;

protected:

	/** Allow base class to refresh on selection changed */
	virtual void RefreshOnSelectionChanged( const bool bNewObjectsSelected, bool bAllHandlesVisible, class UActorComponent* SingleVisibleHandle, const TArray< UActorComponent* >& HoveringOverHandles );

	/** Creates a Transformable for each selected actor, wiping out the existing transformables */
	void SetupTransformablesForSelectedActors();

	/** Figures out where to place an object when tracing it against the scene using a laser pointer */
	bool FindPlacementPointUnderLaser( UViewportInteractor* Interactor, FVector& OutHitLocation );

private:

	/** Called every frame to update hover state */
	void HoverTick( class FEditorViewportClient* ViewportClient, const float DeltaTime );

	/** Updates the interaction */
	void InteractionTick( FEditorViewportClient* ViewportClient, const float DeltaTime );
	
	/** Called by the world interaction system when one of our components is dragged by the user.  If null is
	passed in then we'll treat it as dragging the whole object (rather than a specific axis/handle) */
	void UpdateDragging( 
		const float DeltaTime,
		bool& bIsFirstDragUpdate, 
		const EViewportInteractionDraggingMode DraggingMode, 
		const ETransformGizmoInteractionType InteractionType, 
		const bool bWithTwoHands, 
		class FEditorViewportClient& ViewportClient, 
		const TOptional<FTransformGizmoHandlePlacement> OptionalHandlePlacement, 
		const FVector& DragDelta, 
		const FVector& OtherHandDragDelta, 
		const FVector& DraggedTo, 
		const FVector& OtherHandDraggedTo, 
		const FVector& DragDeltaFromStart, 
		const FVector& OtherHandDragDeltaFromStart, 
		const FVector& LaserPointerStart, 
		const FVector& LaserPointerDirection, 
		const bool bIsLaserPointerValid, 
		const FTransform& GizmoStartTransform, 
		const FBox& GizmoStartLocalBounds, 
		const USceneComponent* const DraggingTransformGizmoComponent,
		FVector& GizmoSpaceFirstDragUpdateOffsetAlongAxis,
		FVector& DragDeltaFromStartOffset,
		bool& bIsDrivingVelocityOfSimulatedTransformables,
		FVector& OutUnsnappedDraggedTo);
	
	/** Given a drag delta from a starting point, contrains that delta based on a gizmo handle axis */
	FVector ComputeConstrainedDragDeltaFromStart( 
		const bool bIsFirstDragUpdate, 
		const ETransformGizmoInteractionType InteractionType, 
		const TOptional<FTransformGizmoHandlePlacement> OptionalHandlePlacement, 
		const FVector& DragDeltaFromStart, 
		const FVector& LaserPointerStart, 
		const FVector& LaserPointerDirection, 
		const bool bIsLaserPointerValid, 
		const FTransform& GizmoStartTransform, 
		FVector& GizmoSpaceFirstDragUpdateOffsetAlongAxis, 
		FVector& DragDeltaFromStartOffset,
		FVector& OutClosestPointOnLaser) const;

	/** Checks to see if smooth snapping is currently enabled */
	bool IsSmoothSnappingEnabled() const;

	/** Returns the time since this was last entered */
	inline FTimespan GetTimeSinceEntered() const
	{
		return FTimespan::FromSeconds( FApp::GetCurrentTime() ) - AppTimeEntered;
	}

	/** Updates the transforma */
	void UpdateTransformables( const float DeltaTime );

	/** Conditionally polls input from interactors, if we haven't done that already this frame */
	void PollInputIfNeeded();

	/** Called when selection changes in the level */
	void OnActorSelectionChanged( UObject* ChangedObject );

	/** Refreshes the transform gizmo to match the currently selected actor set */
	void RefreshTransformGizmo( const bool bNewObjectsSelected, bool bAllHandlesVisible, class UActorComponent* SingleVisibleHandle, const TArray< UActorComponent* >& HoveringOverHandles );

	/** Spawn new transform gizmo when refreshing and at start if there is none yet or the current one is different from the TransformGizmoClass */
	void SpawnTransformGizmoIfNeeded();

	/** Gets the inertie contribution of another interactor */
	UViewportInteractor* GetOtherInteractorIntertiaContribute( UViewportInteractor* Interactor );

	/** Spawns a transient actor that we can use in the editor world (templated for convenience) */
	template<class T>
	T* SpawnTransientSceneActor( const FString& ActorName, const bool bWithSceneComponent ) const
	{
		return CastChecked<T>( SpawnTransientSceneActor( T::StaticClass(), ActorName, bWithSceneComponent ) );
	}

	/** Spawns a transient actor that we can use in the editor world */
	AActor* SpawnTransientSceneActor( TSubclassOf<AActor> ActorClass, const FString& ActorName, const bool bWithSceneComponent ) const;

	/** Gets the snap grid mesh actor */
	AActor* GetSnapGridActor()
	{
		SpawnGridMeshActor();
		return SnapGridActor;
	}

	/** Gets the snap grid mesh MID */
	class UMaterialInstanceDynamic* GetSnapGridMID()
	{
		SpawnGridMeshActor();
		return SnapGridMID;
	}

	/** Spawns the grid actor */
	void SpawnGridMeshActor();

	//
	// Colors
	//
public:
	enum EColors
	{
		DefaultColor,
		Forward,
		Right,
		Up,
		Hover,
		Dragging,
		TotalCount
	};

	/** Gets the color from color type */
	FLinearColor GetColor( const EColors Color ) const;

private:

	// All the colors for this mode
	TArray<FLinearColor> Colors;

protected:
	
	/** All of the objects we're currently interacting with, such as selected actors */
	TArray< TUniquePtr< class FViewportTransformable > > Transformables;

	/** True if we've dragged objects with either hand since the last time we selected something */
	bool bDraggedSinceLastSelection;

	/** Gizmo start transform of the last drag we did with either hand.  Only valid if bDraggedSinceLastSelection is true */
	FTransform LastDragGizmoStartTransform;

	//
	// Undo/redo
	//

	/** Manages saving undo for selected actors while we're dragging them around */
	FTrackingTransaction TrackingTransaction;

private:

	/** App time that we entered this */
	FTimespan AppTimeEntered;

	/** All the interactors registered to modify the world */
	UPROPERTY()
	TArray< class UViewportInteractor* > Interactors;

	//
	// Viewport
	//

	/** The viewport that the worldinteraction is used for */
	TSharedPtr<class FEditorViewportClient> EditorViewportClient;

	/** The last frame that input was polled.  We keep track of this so that we can make sure to poll for latest input right before
	its first needed each frame */
	uint32 LastFrameNumberInputWasPolled;

	//
	// Input
	//

	/** Slate Input Processor */
	TSharedPtr<class FViewportInteractionInputProcessor> InputProcessor;

	/** The Unreal controller ID for the motion controllers we're using */
	int32 MotionControllerID;


	//
	// World movement
	//

	/** The world to meters scale before we changed it (last frame's world to meters scale) */
	float LastWorldToMetersScale;

	//
	// Hover state
	//

	/** Set of objects that are currently hovered over */
	TSet<FViewportHoverTarget> HoveredObjects;

	//
	// Object movement/placement
	//

	/** True if our transformables are currently interpolating to their target location */
	bool bIsInterpolatingTransformablesFromSnapshotTransform;

	/** Time that we started interpolating at */
	FTimespan TransformablesInterpolationStartTime;

	/** Duration to interpolate transformables over */
	float TransformablesInterpolationDuration;

	//
	// Transform gizmo
	//

	/** Transform gizmo actor, for manipulating selected actor(s) */
	UPROPERTY()
	class ABaseTransformGizmo* TransformGizmoActor;

	/** Class used for when refreshing the transform gizmo */
	TSubclassOf<ABaseTransformGizmo> TransformGizmoClass;

	/** Current gizmo bounds in local space.  Updated every frame.  This is not the same as the gizmo actor's bounds. */
	FBox GizmoLocalBounds;

	/** Starting angle when rotating an object with  ETransformGizmoInteractionType::RotateOnAngle */
	TOptional<float> StartDragAngleOnRotation;

	/** The direction of where the rotation handle is facing when starting rotation */
	TOptional<FVector> StartDragHandleDirection;

	/** The current Gizmo type that is used for the TransformGizmo Actor */
	EGizmoHandleTypes CurrentGizmoType;

	/** Whether the gizmo should be visible or not */
	bool bIsTransformGizmoVisible;


	//
	// Snap grid
	//

	/** Actor for the snap grid */
	UPROPERTY()
	class AActor* SnapGridActor;

	/** The plane mesh we use to draw a snap grid under selected objects */
	UPROPERTY()
	class UStaticMeshComponent* SnapGridMeshComponent;

	/** MID for the snap grid, so we can update snap values on the fly */
	UPROPERTY()
	class UMaterialInstanceDynamic* SnapGridMID;

	//
	// Interactable
	//

	/** The current dragged interactable */
	class IViewportInteractableInterface* DraggedInteractable;		
	
	/** Event that fires every frame to update hover state based on what's under the cursor.  Set bWasHandled to true if you detect something to hover. */
	FOnVIHoverUpdate OnHoverUpdateEvent;

	/** Event that is fired for when a key is pressed by an interactor */
	FOnVIActionHandle OnInputActionEvent;
};