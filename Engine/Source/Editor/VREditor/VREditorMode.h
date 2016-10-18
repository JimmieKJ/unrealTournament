// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IVREditorMode.h"
#include "IVREditorModule.h"
#include "HeadMountedDisplayTypes.h"	// For EHMDDeviceType::Type
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"	// For EHMDTrackingOrigin::Type

// Forward declare the GizmoHandleTypes that is defined in VREditorTransformGizmo
enum class EGizmoHandleTypes : uint8;

/** Reason for exiting the VR editor */
enum EVREditorExitType
{
	Normal = 0,
	PIE_VR = 1
};

/**
 * VR Editor Mode.  Extends editor viewports with functionality for VR controls and object manipulation
 */
class FVREditorMode : public IVREditorMode
{
public:

	/** Default constructor for FVREditorMode, called when the editor starts up after the VREditor module is loaded */
	FVREditorMode();

	/** Cleans up this mode, called when the editor is shutting down */
	virtual ~FVREditorMode();

	/** Static: Gets the ID of our editor mode */
	static FEditorModeID GetVREditorModeID()
	{
		return VREditorModeID;
	}

	/** Static: Sets whether we should actually use an HMD.  Call this before activating VR mode */
	static void SetActuallyUsingVR( const bool bShouldActuallyUseVR )
	{
		bActuallyUsingVR = bShouldActuallyUseVR;
	}

	/** Returns true if we're actually using VR, or false if we're faking it */
	static bool IsActuallyUsingVR()
	{
		return bActuallyUsingVR;
	}

	/** Returns true if the user wants to exit this mode */
	bool WantsToExitMode() const
	{
		return bWantsToExitMode;
	}

	/** Call this to start exiting VR mode */
	void StartExitingVRMode( const EVREditorExitType InExitType = EVREditorExitType::Normal );

	/** Gets the reason for exiting the mode */
	EVREditorExitType GetExitType() const
	{
		return ExitType;
	}

	// FEdMode interface
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void Tick( FEditorViewportClient* ViewportClient, float DeltaTime ) override;
	virtual bool InputKey( FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event ) override;
	virtual bool InputAxis( FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime ) override;
	virtual bool IsCompatibleWith( FEditorModeID OtherModeID ) const override;
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual void Render( const FSceneView* SceneView, FViewport* Viewport, FPrimitiveDrawInterface* PDI ) override;	

	// IVREditorMode interface
	virtual AActor* GetAvatarMeshActor() override;
	virtual UWorld* GetWorld() const override;
	virtual FTransform GetRoomTransform() const override;
	virtual void SetRoomTransform( const FTransform& NewRoomTransform ) override;
	virtual FTransform GetRoomSpaceHeadTransform() const override;
	virtual FTransform GetHeadTransform() const override;
	virtual const class UViewportWorldInteraction& GetWorldInteraction() const override;
	virtual class UViewportWorldInteraction& GetWorldInteraction() override;
	virtual bool IsFullyInitialized() const override;
	virtual FOnVRTickHandle& OnTickHandle() override
	{
		return TickHandle;
	}

	/** Returns the Unreal controller ID for the motion controllers we're using */
	int32 GetMotionControllerID() const
	{
		return MotionControllerID;
	}

	/** Returns whether or not the flashlight is visible */
	bool IsFlashlightOn() const
	{
		return bIsFlashlightOn;
	}

	/** Returns the time since the VR Editor mode was last entered */
	inline FTimespan GetTimeSinceModeEntered() const
	{
		return FTimespan::FromSeconds( FApp::GetCurrentTime() ) - AppTimeModeEntered;
	}

	// @todo vreditor: Move this to FMath so we can use it everywhere
	// NOTE: OvershootAmount is usually between 0.5 and 2.0, but can go lower and higher for extreme overshots
	template<class T>
	static T OvershootEaseOut( T Alpha, const float OvershootAmount = 1.0f )
	{
		Alpha--;
		return 1.0f - ( ( Alpha * ( ( OvershootAmount + 1 ) * Alpha + OvershootAmount ) + 1 ) - 1.0f );
	}

	/** Spawns a transient actor that we can use in the editor world (templated for convenience) */
	template<class T>
	T* SpawnTransientSceneActor( const FString& ActorName, const bool bWithSceneComponent ) const
	{
		return CastChecked<T>( SpawnTransientSceneActor( T::StaticClass(), ActorName, bWithSceneComponent ) );
	}

	/** Spawns a transient actor that we can use in the editor world */
	AActor* SpawnTransientSceneActor( TSubclassOf<class AActor> ActorClass, const FString& ActorName, const bool bWithSceneComponent ) const;

	/** Destroys a transient actor we created earlier and nulls out the pointer */
	void DestroyTransientActor( AActor* Actor ) const;

	/** Gets access to the VR UI system (const) */
	const class UVREditorUISystem& GetUISystem() const
	{
		return *UISystem;
	}

	/** Gets access to the VR UI system */
	class UVREditorUISystem& GetUISystem()
	{
		return *UISystem;
	}

	/** Gets the viewport that VR Mode is activated in.  Even though editor modes are available in all
	    level viewports simultaneously, only one viewport is "possessed" by the HMD.  Generally try to avoid using
		this function and instead use the ViewportClient that is passed around through various FEdMode overrides */
	const class SLevelViewport& GetLevelViewportPossessedForVR() const;

	/** Mutable version of above. */
	class SLevelViewport& GetLevelViewportPossessedForVR();

	/** Gets the world scale factor, which can be multiplied by a scale vector to convert to room space */
	float GetWorldScaleFactor() const;

	/** Sets up our avatar mesh, if not already spawned */
	void SpawnAvatarMeshActor();

	/** Called internally when the user changes maps, enters/exits PIE or SIE, or switched between PIE/SIE */
	void CleanUpActorsBeforeMapChangeOrSimulate();

	/** Spawns a flashlight on the specified hand */
	void ToggleFlashlight( class UVREditorInteractor* Interactor );

	/** Will update the TransformGizmo Actor with the next Gizmo type  */
	void CycleTransformGizmoHandleType();

	/** Gets the current Gizmo handle type */
	EGizmoHandleTypes GetCurrentGizmoType() const;

	/** @return Returns the type of HMD we're dealing with */
	EHMDDeviceType::Type GetHMDDeviceType() const;

	/** @return Checks to see if the specified hand is aiming roughly toward the specified capsule */
	bool IsHandAimingTowardsCapsule( class UViewportInteractor* Interactor, const FTransform& CapsuleTransform, const FVector CapsuleStart, const FVector CapsuleEnd, const float CapsuleRadius, const float MinDistanceToCapsule, const FVector CapsuleFrontDirection, const float MinDotForAimingAtCapsule ) const;
	
	/** Gets the hand interactor  */
	class UVREditorInteractor* GetHandInteractor( const EControllerHand ControllerHand ) const;

	/** Stops the haptic feedback for the left and right hand interactors for the oculus */
	void StopOldHapticEffects();

private:

	/** Called when someone closes a standalone VR Editor window */
	void OnVREditorWindowClosed( const TSharedRef<SWindow>& ClosedWindow );

	/** FEditorDelegates callbacks */
	void OnMapChange( uint32 MapChangeFlags );
	void OnBeginPIE( const bool bIsSimulatingInEditor );
	void OnEndPIE( const bool bIsSimulatingInEditor );
	void OnSwitchBetweenPIEAndSIE( const bool bIsSimulatingInEditor );

protected:

	/** Static ID name for our editor mode */
	static FEditorModeID VREditorModeID;

	//
	// Startup/Shutdown
	//

	/** The VR editor window, if it's open right now */
	TWeakPtr< class SWindow > VREditorWindowWeakPtr;

	/** The VR level viewport, if we're in VR mode */
	TWeakPtr< class SLevelViewport > VREditorLevelViewportWeakPtr;

	/** Saved information about the editor and viewport we possessed, so we can restore it after exiting VR mode */
	struct FSavedEditorState
	{
		ELevelViewportType ViewportType;
		FVector ViewLocation;
		FRotator ViewRotation;
		FEngineShowFlags ShowFlags;
		bool bLockedPitch;
		bool bGameView;
		bool bAlwaysShowModeWidgetAfterSelectionChanges;
		float NearClipPlane;
		bool bRealTime;
		float DragTriggerDistance;
		bool bOnScreenMessages;
		EHMDTrackingOrigin::Type TrackingOrigin;

		FSavedEditorState()
			: ViewportType( LVT_Perspective ),
			  ViewLocation( FVector::ZeroVector ),
			  ViewRotation( FRotator::ZeroRotator ),
			  ShowFlags( ESFIM_Editor ),
			  bLockedPitch( false ),
			  bGameView( false ),
			  bAlwaysShowModeWidgetAfterSelectionChanges( false ),
			  NearClipPlane( 0.0f ),
			  bRealTime( false ),
			  DragTriggerDistance( 0.0f ),
			  bOnScreenMessages( false ),
			  TrackingOrigin( EHMDTrackingOrigin::Eye )
		{
		}
		
	} SavedEditorState;

	/** Static: True if we're in using an actual HMD in this mode, or false if we're "faking" VR mode for testing */
	static bool bActuallyUsingVR;

	/** True if we currently want to exit VR mode.  This is used to defer exiting until it is safe to do that */
	bool bWantsToExitMode;

	/** True if VR mode is fully initialized and ready to render */
	bool bIsFullyInitialized;

	/** App time that we entered this mode */
	FTimespan AppTimeModeEntered;


	//
	// Avatar visuals
	//

	/** Actor with components to represent the VR avatar in the world, including motion controller meshes */
	class AVREditorAvatarActor* AvatarActor;


	//
	// Flashlight
	//

	/** Spotlight for the flashlight */
	class USpotLightComponent* FlashlightComponent;

	/** If there is currently a flashlight in the scene */
	bool bIsFlashlightOn;

	//
	// Input
	//

	/** The Unreal controller ID for the motion controllers we're using */
	int32 MotionControllerID;


	//
	// Subsystems registered
	//

	FOnVRTickHandle TickHandle;

	//
	// Subsystems
	//

	/** VR UI system */
	class UVREditorUISystem* UISystem;

	/** Teleporter system */
	class UVREditorTeleporter* TeleporterSystem;

	//
	// World interaction
	//

	/** World interaction manager */
	class UVREditorWorldInteraction* WorldInteraction;

	/** The current Gizmo type that is used for the TransformGizmo Actor */
	EGizmoHandleTypes CurrentGizmoType;

	//
	// Interactors
	//

	/** The mouse cursor interactor (currently only available when not in VR) */
	class UMouseCursorInteractor* MouseCursorInteractor;

	/** The right motion controller */
	class UVREditorMotionControllerInteractor* LeftHandInteractor; //@todo vreditor: Hardcoded interactors
	
	/** The right motion controller */
	class UVREditorMotionControllerInteractor* RightHandInteractor; 

	//
	// Colors
	//
public:
	// Color types
	enum EColors
	{
		DefaultColor,
		SelectionColor,
		TeleportColor,
		WorldDraggingColor_OneHand,
		WorldDraggingColor_TwoHands,
		RedGizmoColor,
		GreenGizmoColor,
		BlueGizmoColor,
		WhiteGizmoColor,
		HoverGizmoColor,
		DraggingGizmoColor,
		UISelectionBarColor,
		UISelectionBarHoverColor,
		UICloseButtonColor,
		UICloseButtonHoverColor,
		TotalCount
	};

	// Gets the color
	FLinearColor GetColor( const EColors Color ) const;

private:

	// All the colors for this mode
	TArray<FLinearColor> Colors;

	/** If this is the first tick or before */
	bool bFirstTick;

	/** If the coordinate system was in world space before entering (local) scale mode */
	bool bWasInWorldSpaceBeforeScaleMode;

	/** The reason for exiting the mode, so the module can close the mode and take further actions on what should happen next */
	EVREditorExitType ExitType;

};

