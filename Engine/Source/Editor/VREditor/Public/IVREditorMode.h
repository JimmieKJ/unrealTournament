// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Types of actions that can be performed with VR controller devices
 */
namespace VRActionTypes
{
	static const FName Touch( "Touch" );
	static const FName Modifier( "Modifier" );
	static const FName ConfirmRadialSelection( "ConfirmRadialSelection" );
	static const FName TrackpadPositionX( "TrackpadPositionX" );
	static const FName TrackpadPositionY( "TrackpadPositionY" );
	static const FName TriggerAxis( "TriggerAxis" );
}

/**
 * VR Editor Mode public interface
 */
class IVREditorMode : public FEdMode
{

public:

	/**
	 * Gets the world space transform of the HMD (head)
	 *
	 * @return	World space space HMD transform
	 */
	virtual FTransform GetHeadTransform() const = 0;

	/**
	 * Gets our avatar's mesh actor
	 *
	 * @return	The mesh actor
	 */
	virtual class AActor* GetAvatarMeshActor() = 0;

	/** Gets the world currently used by the worldinteraction of this mode */
	virtual UWorld* GetWorld() const = 0;

	/** Gets the world space transform of the calibrated VR room origin.  When using a seated VR device, this will feel like the
	camera's world transform (before any HMD positional or rotation adjustments are applied.) */
	virtual FTransform GetRoomTransform() const = 0;

	/** Sets a new transform for the room, in world space.  This is basically setting the editor's camera transform for the viewport */
	virtual void SetRoomTransform( const FTransform& NewRoomTransform ) = 0;

	/** Gets the transform of the user's HMD in room space */
	virtual FTransform GetRoomSpaceHeadTransform() const = 0;

	/** Gets access to the world interaction system (const) */
	virtual const class UViewportWorldInteraction& GetWorldInteraction() const = 0;

	/** Gets access to the world interaction system */
	virtual class UViewportWorldInteraction& GetWorldInteraction() = 0;

	/** If the mode was completely initialized */
	virtual bool IsFullyInitialized() const = 0;

	/** Gets the event for update which is broadcasted by the mode itself after getting latest input states */
	DECLARE_EVENT_OneParam( IVREditorMode, FOnVRTickHandle, const float /* DeltaTime */ );
	virtual FOnVRTickHandle& OnTickHandle() = 0;

};

