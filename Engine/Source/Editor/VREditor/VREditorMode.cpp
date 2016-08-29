// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorMode.h"
#include "VREditorUISystem.h"
#include "VREditorWorldInteraction.h"
#include "VREditorTransformGizmo.h"
#include "VREditorFloatingText.h"
#include "VREditorFloatingUI.h"
#include "VREditorAvatarActor.h"
#include "Teleporter/VREditorTeleporter.h"

#include "CameraController.h"
#include "DynamicMeshBuilder.h"
#include "Features/IModularFeatures.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "IMotionController.h"
#include "MotionControllerComponent.h"
#include "EngineAnalytics.h"
#include "IHeadMountedDisplay.h"
#include "EngineAnalytics.h"
#include "IAnalyticsProvider.h"

#include "Editor/LevelEditor/Public/LevelEditorActions.h"
#include "Editor/ViewportInteraction/Public/ViewportInteraction.h"
#include "VREditorInteractor.h"
#include "MouseCursorInteractor.h"
#include "VREditorMotionControllerInteractor.h"

#include "Interactables/VREditorButton.h"

#define LOCTEXT_NAMESPACE "VREditorMode"

namespace VREd
{
	static FAutoConsoleVariable UseMouseAsHandInForcedVRMode( TEXT( "VREd.UseMouseAsHandInForcedVRMode" ), 1, TEXT( "When in forced VR mode, enabling this setting uses the mouse cursor as a virtual hand instead of motion controllers" ) );
	static FAutoConsoleVariable ForceOculusMirrorMode( TEXT( "VREd.ForceOculusMirrorMode" ), 3, TEXT( "Which Oculus display mirroring mode to use (see MirrorWindowModeType in OculusRiftHMD.h)" ) );
}

FEditorModeID FVREditorMode::VREditorModeID( "VREditor" );
bool FVREditorMode::bActuallyUsingVR = true;

// @todo vreditor: Hacky that we have to import these this way. (Plugin has them in a .cpp, not exported)

FVREditorMode::FVREditorMode()
	: bWantsToExitMode( false ),
	  bIsFullyInitialized( false ),
	  AppTimeModeEntered( FTimespan::Zero() ),
	  AvatarActor( nullptr ),
      FlashlightComponent( nullptr ),
	  bIsFlashlightOn( false ),
	  MotionControllerID( 0 ),	// @todo vreditor minor: We only support a single controller, and we assume the first controller are the motion controls
	  UISystem( nullptr ),
	  TeleporterSystem( nullptr ),
	  WorldInteraction( nullptr ),
	  MouseCursorInteractor( nullptr ),
	  LeftHandInteractor( nullptr ),
	  RightHandInteractor( nullptr ),
	  bFirstTick( true ),
	  bWasInWorldSpaceBeforeScaleMode( false )
{
	FEditorDelegates::MapChange.AddRaw( this, &FVREditorMode::OnMapChange );
	FEditorDelegates::BeginPIE.AddRaw( this, &FVREditorMode::OnBeginPIE );
	FEditorDelegates::EndPIE.AddRaw( this, &FVREditorMode::OnEndPIE );
	FEditorDelegates::OnSwitchBeginPIEAndSIE.AddRaw( this, &FVREditorMode::OnSwitchBetweenPIEAndSIE );
}


FVREditorMode::~FVREditorMode()
{
	FEditorDelegates::MapChange.RemoveAll( this );
	FEditorDelegates::BeginPIE.RemoveAll( this );
	FEditorDelegates::EndPIE.RemoveAll( this );
	FEditorDelegates::OnSwitchBeginPIEAndSIE.RemoveAll( this );
}

void FVREditorMode::Enter()
{
	// @todo vreditor urgent: Turn on global editor hacks for VR Editor mode
	GEnableVREditorHacks = true;

	bIsFullyInitialized = false;
	bWantsToExitMode = false;

	AppTimeModeEntered = FTimespan::FromSeconds( FApp::GetCurrentTime() );

	// Take note of VREditor activation
	if( FEngineAnalytics::IsAvailable() )
	{
		FEngineAnalytics::GetProvider().RecordEvent( TEXT( "Editor.Usage.EnterVREditorMode" ) );
	}

	// Setting up colors
	Colors.SetNumZeroed( (int32)EColors::TotalCount );
	{
		Colors[ (int32)EColors::DefaultColor ] = FLinearColor::Red;	
		Colors[ (int32)EColors::SelectionColor ] = FLinearColor::Green;
		Colors[ (int32)EColors::TeleportColor ] = FLinearColor( 1.0f, 0.0f, 0.75f, 1.0f );
		Colors[ (int32)EColors::WorldDraggingColor_OneHand ] = FLinearColor::Yellow;
		Colors[ (int32)EColors::WorldDraggingColor_TwoHands ] = FLinearColor( 0.05f, 0.05f, 0.4f, 1.0f );
		Colors[ (int32)EColors::RedGizmoColor ] = FLinearColor( 0.4f, 0.05f, 0.05f, 1.0f );
		Colors[ (int32)EColors::GreenGizmoColor ] = FLinearColor( 0.05f, 0.4f, 0.05f, 1.0f );
		Colors[ (int32)EColors::BlueGizmoColor ] = FLinearColor( 0.05f, 0.05f, 0.4f, 1.0f );
		Colors[ (int32)EColors::WhiteGizmoColor ] = FLinearColor( 0.7f, 0.7f, 0.7f, 1.0f );
		Colors[ (int32)EColors::HoverGizmoColor ] = FLinearColor( FLinearColor::Yellow );
		Colors[ (int32)EColors::DraggingGizmoColor ] = FLinearColor( FLinearColor::White );
		Colors[ (int32)EColors::UISelectionBarColor ] = FLinearColor( 0.025f, 0.025f, 0.025f, 1.0f );
		Colors[ (int32)EColors::UISelectionBarHoverColor ] = FLinearColor( 0.1f, 0.1f, 0.1f, 1.0f );
		Colors[ (int32)EColors::UICloseButtonColor ] = FLinearColor( 0.1f, 0.1f, 0.1f, 1.0f );
		Colors[ (int32)EColors::UICloseButtonHoverColor ] = FLinearColor( 1.0f, 1.0f, 1.0f, 1.0f );
	}

	// @todo vreditor: We need to make sure the user can never switch to orthographic mode, or activate settings that
	// would disrupt the user's ability to view the VR scene.
		
	// @todo vreditor: Don't bother drawing toolbars in VR, or other things that won't matter in VR

	{
		const TSharedRef< ILevelEditor >& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor" ).GetFirstLevelEditor().ToSharedRef();

		bool bSummonNewWindow = true;

		// Do we have an active perspective viewport that is valid for VR?  If so, go ahead and use that.
		TSharedPtr<SLevelViewport> ExistingActiveLevelViewport;
		{
			TSharedPtr<ILevelViewport> ActiveLevelViewport = LevelEditor->GetActiveViewportInterface();
			if( ActiveLevelViewport.IsValid() )
			{
				ExistingActiveLevelViewport = StaticCastSharedRef< SLevelViewport >( ActiveLevelViewport->AsWidget() );

				// Use the currently active window instead
				bSummonNewWindow = false;
			}
		}
		
		TSharedPtr< SLevelViewport > VREditorLevelViewport;
		if( bSummonNewWindow )
		{
			// @todo vreditor: The resolution we set here doesn't matter, as HMDs will draw at their native resolution
			// no matter what.  We should probably allow the window to be freely resizable by the user
			// @todo vreditor: Should save and restore window position and size settings
			FVector2D WindowSize;
			{
				IHeadMountedDisplay::MonitorInfo HMDMonitorInfo;
				if( bActuallyUsingVR && GEngine->HMDDevice->GetHMDMonitorInfo( HMDMonitorInfo ) )
				{
					WindowSize = FVector2D( HMDMonitorInfo.ResolutionX, HMDMonitorInfo.ResolutionY );
				}
				else
				{
					// @todo vreditor: Hard-coded failsafe window size
					WindowSize = FVector2D( 1920.0f, 1080.0f );
				}
			}

			// @todo vreditor: Use SLevelEditor::GetTableTitle() for the VR window title (needs dynamic update)
			const FText VREditorWindowTitle = NSLOCTEXT( "VREditor", "VRWindowTitle", "Unreal Editor VR" );

			TSharedRef< SWindow > VREditorWindow = SNew( SWindow )
				.Title( VREditorWindowTitle )
				.ClientSize( WindowSize )
				.AutoCenter( EAutoCenter::PreferredWorkArea )
				.UseOSWindowBorder( true )	// @todo vreditor: Allow window to be freely resized?  Shouldn't really hurt anything.  We should save position/size too.
				.SizingRule( ESizingRule::UserSized );
			this->VREditorWindowWeakPtr = VREditorWindow;

			VREditorLevelViewport =
				SNew( SLevelViewport )
				.ViewportType( LVT_Perspective ) // Perspective
				.Realtime( true )
				//				.ParentLayout( AsShared() )	// @todo vreditor: We don't have one and we probably don't need one, right?  Make sure a null parent layout is handled properly everywhere.
				.ParentLevelEditor( LevelEditor )
				//				.ConfigKey( BottomLeftKey )	// @todo vreditor: This is for saving/loading layout.  We would need this in order to remember viewport settings like show flags, etc.
				.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() );

			// Allow the editor to keep track of this editor viewport.  Because it's not inside of a normal tab, 
			// we need to explicitly tell the level editor about it
			LevelEditor->AddStandaloneLevelViewport( VREditorLevelViewport.ToSharedRef() );

			VREditorWindow->SetContent( VREditorLevelViewport.ToSharedRef() );

			// NOTE: We're intentionally not adding this window natively parented to the main frame window, because we don't want it
			// to minimize/restore when the main frame is minimized/restored
			FSlateApplication::Get().AddWindow( VREditorWindow );

			VREditorWindow->SetOnWindowClosed( FOnWindowClosed::CreateRaw( this, &FVREditorMode::OnVREditorWindowClosed ) );

			VREditorWindow->BringToFront();	// @todo vreditor: Not sure if this is needed, especially if we decide the window should be hidden (copied this from PIE code)
		}
		else
		{
			VREditorLevelViewport = ExistingActiveLevelViewport;

			if( bActuallyUsingVR )
			{
				// Switch to immersive mode
				const bool bWantImmersive = true;
				const bool bAllowAnimation = false;
				ExistingActiveLevelViewport->MakeImmersive( bWantImmersive, bAllowAnimation );
			}
		}
		this->VREditorLevelViewportWeakPtr = VREditorLevelViewport;
		
		{
			FLevelEditorViewportClient& VRViewportClient = VREditorLevelViewport->GetLevelViewportClient();
			FEditorViewportClient& VREditorViewportClient = VRViewportClient;

			// Make sure we are in perspective mode
			// @todo vreditor: We should never allow ortho switching while in VR
			SavedEditorState.ViewportType = VREditorViewportClient.GetViewportType();
			VREditorViewportClient.SetViewportType( LVT_Perspective );

			// Set the initial camera location
			// @todo vreditor: This should instead be calculated using the currently active perspective camera's
			// location and orientation, compensating for the current HMD offset from the tracking space origin.
			// Perhaps, we also want to teleport the original viewport's camera back when we exit this mode, too!
			// @todo vreditor: Should save and restore camera position and any other settings we change (viewport type, pitch locking, etc.)
			SavedEditorState.ViewLocation = VRViewportClient.GetViewLocation();
			SavedEditorState.ViewRotation = VRViewportClient.GetViewRotation();
			
			// Don't allow the tracking space to pitch up or down.  People hate that in VR.
			// @todo vreditor: This doesn't seem to prevent people from pitching the camera with RMB drag
			SavedEditorState.bLockedPitch = VRViewportClient.GetCameraController()->GetConfig().bLockedPitch;
			if( bActuallyUsingVR )
			{
				VRViewportClient.GetCameraController()->AccessConfig().bLockedPitch = true;
			}

			// Set "game mode" to be enabled, to get better performance.  Also hit proxies won't work in VR, anyway
			SavedEditorState.bGameView = VREditorViewportClient.IsInGameView();
			VREditorViewportClient.SetGameView( true );

			SavedEditorState.bRealTime = VREditorViewportClient.IsRealtime();
			VREditorViewportClient.SetRealtime( true );

			SavedEditorState.ShowFlags = VREditorViewportClient.EngineShowFlags;

			// Never show the traditional Unreal transform widget.  It doesn't work in VR because we don't have hit proxies.
			VREditorViewportClient.EngineShowFlags.SetModeWidgets( false );

			// Make sure the mode widgets don't come back when users click on things
			VRViewportClient.bAlwaysShowModeWidgetAfterSelectionChanges = false;

			// Force tiny near clip plane distance, because user can scale themselves to be very small.
			// @todo vreditor: Make this automatically change based on WorldToMetersScale?
			SavedEditorState.NearClipPlane = GNearClippingPlane;
			GNearClippingPlane = 1.0f;	// Normally defaults to 10cm (NOTE: If we go too small, skyboxes become affected)

			SavedEditorState.bOnScreenMessages = GAreScreenMessagesEnabled;
			GAreScreenMessagesEnabled = false;

			if( bActuallyUsingVR )
			{
				SavedEditorState.TrackingOrigin = GEngine->HMDDevice->GetTrackingOrigin();
				GEngine->HMDDevice->SetTrackingOrigin( EHMDTrackingOrigin::Floor );
			}

			// Make the new viewport the active level editing viewport right away
			GCurrentLevelEditingViewportClient = &VRViewportClient;

			// Enable selection outline right away
			VREditorViewportClient.EngineShowFlags.SetSelection(true);
			VREditorViewportClient.EngineShowFlags.SetSelectionOutline(true);
		}

		VREditorLevelViewport->EnableStereoRendering( bActuallyUsingVR );
		VREditorLevelViewport->SetRenderDirectlyToWindow( bActuallyUsingVR );

		if( bActuallyUsingVR )
		{
			GEngine->HMDDevice->EnableStereo( true );

			// @todo vreditor: Force single eye, undistorted mirror for demos
			const bool bIsVREditorDemo = FParse::Param( FCommandLine::Get(), TEXT( "VREditorDemo" ) );	// @todo vreditor: Remove this when no longer needed (console variable, too!)
			if( bIsVREditorDemo && GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift )
			{
				// If we're using an Oculus Rift, go ahead and set the mirror mode to a single undistorted eye
				GEngine->DeferredCommands.Add( FString::Printf( TEXT( "HMD MIRROR MODE %i" ), VREd::ForceOculusMirrorMode->GetInt() ) );
			}
		}

		if( bActuallyUsingVR )
		{
			// Tell Slate to require a larger pixel distance threshold before the drag starts.  This is important for things 
			// like Content Browser drag and drop.
			SavedEditorState.DragTriggerDistance = FSlateApplication::Get().GetDragTriggerDistance();
			FSlateApplication::Get().SetDragTriggerDistance( 100.0f );	// @todo vreditor tweak
		}
	}

	// Setup sub systems
	{
		// Setup world interaction
		WorldInteraction = NewObject<UVREditorWorldInteraction>();
		WorldInteraction->SetOwner( this );
		WorldInteraction->Init( VREditorLevelViewportWeakPtr.Pin()->GetViewportClient() );

		// Motion controllers
		{
			LeftHandInteractor = NewObject<UVREditorMotionControllerInteractor>( WorldInteraction );
			LeftHandInteractor->SetControllerHandSide( EControllerHand::Left );
			LeftHandInteractor->Init( this );
			WorldInteraction->AddInteractor( LeftHandInteractor );

			RightHandInteractor = NewObject<UVREditorMotionControllerInteractor>( WorldInteraction );
			RightHandInteractor->SetControllerHandSide( EControllerHand::Right );
			RightHandInteractor->Init( this );
			WorldInteraction->AddInteractor( RightHandInteractor );

			WorldInteraction->PairInteractors( LeftHandInteractor, RightHandInteractor );
		}

		if( !bActuallyUsingVR )
		{
			// Register an interactor for the mouse cursor
			MouseCursorInteractor = NewObject<UMouseCursorInteractor>( WorldInteraction );
			MouseCursorInteractor->Init();
			WorldInteraction->AddInteractor( MouseCursorInteractor );
		}

		// Setup the UI system
		UISystem = NewObject<UVREditorUISystem>();
		UISystem->SetOwner( this );
		UISystem->Init();

		// Setup teleporter
		TeleporterSystem = NewObject<UVREditorTeleporter>();
		TeleporterSystem->Init( this );

	}

	// Call parent implementation
	FEdMode::Enter();

	bFirstTick = true;
	bIsFullyInitialized = true;
}


void FVREditorMode::Exit()
{
	bIsFullyInitialized = false;

	FSlateApplication::Get().SetInputPreProcessor(false);

	//Destroy the avatar
	{
		DestroyTransientActor( AvatarActor );
		AvatarActor = nullptr;

		FlashlightComponent = nullptr;
	}

	{
		if( GEngine->HMDDevice.IsValid() )
		{
			// @todo vreditor switch: We don't want to do this if a VR PIE session is somehow active.  Is that even possible while immersive?
			GEngine->HMDDevice->EnableStereo( false );
		}

		if( bActuallyUsingVR )
		{ 
			// Restore Slate drag trigger distance
			FSlateApplication::Get().SetDragTriggerDistance( SavedEditorState.DragTriggerDistance );
		}

		TSharedPtr<SLevelViewport> VREditorLevelViewport( VREditorLevelViewportWeakPtr.Pin() );
		if( VREditorLevelViewport.IsValid() )
		{
			VREditorLevelViewport->EnableStereoRendering( false );
			VREditorLevelViewport->SetRenderDirectlyToWindow( false );

			{
				FLevelEditorViewportClient& VRViewportClient = VREditorLevelViewport->GetLevelViewportClient();
				FEditorViewportClient& VREditorViewportClient = VRViewportClient;

				// Restore settings that we changed on the viewport
				VREditorViewportClient.SetViewportType( SavedEditorState.ViewportType );
				VRViewportClient.GetCameraController()->AccessConfig().bLockedPitch = SavedEditorState.bLockedPitch;
				VRViewportClient.bAlwaysShowModeWidgetAfterSelectionChanges = SavedEditorState.bAlwaysShowModeWidgetAfterSelectionChanges;
				VRViewportClient.EngineShowFlags = SavedEditorState.ShowFlags;
				VRViewportClient.SetGameView( SavedEditorState.bGameView );
				VRViewportClient.SetViewLocation( GetHeadTransform().GetLocation() );
				
				FRotator HeadRotationNoRoll = GetHeadTransform().GetRotation().Rotator();
				HeadRotationNoRoll.Roll = 0.0f;
				VRViewportClient.SetViewRotation(HeadRotationNoRoll); // Use SavedEditorState.ViewRotation to go back to start rot
				
				VRViewportClient.SetRealtime( SavedEditorState.bRealTime );

				GNearClippingPlane = SavedEditorState.NearClipPlane;
				GAreScreenMessagesEnabled = SavedEditorState.bOnScreenMessages;

				if( bActuallyUsingVR )
				{
					GEngine->HMDDevice->SetTrackingOrigin( SavedEditorState.TrackingOrigin );
				}
			}

			if( bActuallyUsingVR )
			{
				// Leave immersive mode
				const bool bWantImmersive = false;
				const bool bAllowAnimation = false;
				VREditorLevelViewport->MakeImmersive( bWantImmersive, bAllowAnimation );
			}

			VREditorLevelViewportWeakPtr.Reset();
		}

		// Kill the VR editor window
		TSharedPtr<SWindow> VREditorWindow( VREditorWindowWeakPtr.Pin() );
		if( VREditorWindow.IsValid() )
		{
			VREditorWindow->RequestDestroyWindow();
			VREditorWindow.Reset();
		}
	}

	// Call parent implementation
	FEdMode::Exit();

	// Kill subsystems
	if ( UISystem != nullptr )
	{
		UISystem->Shutdown();
		UISystem->MarkPendingKill();
		UISystem = nullptr;
	}

	if ( TeleporterSystem != nullptr )
	{
		TeleporterSystem->Shutdown();
		TeleporterSystem->MarkPendingKill();
		TeleporterSystem = nullptr;
	}

	if ( WorldInteraction != nullptr )
	{
		WorldInteraction->Shutdown();
		WorldInteraction->MarkPendingKill();
		WorldInteraction = nullptr;
	}

	// @todo vreditor urgent: Disable global editor hacks for VR Editor mode
	GEnableVREditorHacks = false;
}

void FVREditorMode::StartExitingVRMode( const EVREditorExitType InExitType /*= EVREditorExitType::To_Editor */ )
{
	ExitType = InExitType;
	bWantsToExitMode = true;
}

void FVREditorMode::SpawnAvatarMeshActor()
{
	// Setup our avatar
	if( AvatarActor == nullptr )
	{
		{
			const bool bWithSceneComponent = true;
			AvatarActor = SpawnTransientSceneActor<AVREditorAvatarActor>( TEXT( "AvatarActor" ), bWithSceneComponent );
			AvatarActor->Init( this );
		}

		//@todo VREditor: Hardcoded interactors
		LeftHandInteractor->SetupComponent( AvatarActor );
		RightHandInteractor->SetupComponent( AvatarActor );
	}
}


void FVREditorMode::OnVREditorWindowClosed( const TSharedRef<SWindow>& ClosedWindow )
{
	StartExitingVRMode();
}


void FVREditorMode::Tick( FEditorViewportClient* ViewportClient, float DeltaTime )
{
	// Call parent implementation
	FEdMode::Tick( ViewportClient, DeltaTime );

	// Only if this is our VR viewport. Remember, editor modes get a chance to tick and receive input for each active viewport.
	if ( ViewportClient != GetLevelViewportPossessedForVR().GetViewportClient().Get() || bWantsToExitMode )
	{
		return;
	}

	//Setting the initial position and rotation based on the editor viewport when going into VR mode
	if ( bFirstTick )
	{
		const FTransform RoomToWorld = GetRoomTransform();
		const FTransform WorldToRoom = RoomToWorld.Inverse();
		FTransform ViewportToWorld = FTransform( SavedEditorState.ViewRotation, SavedEditorState.ViewLocation );
		FTransform ViewportToRoom = ( ViewportToWorld * WorldToRoom );

		FTransform ViewportToRoomYaw = ViewportToRoom;
		ViewportToRoomYaw.SetRotation( FQuat( FRotator( 0.0f, ViewportToRoomYaw.GetRotation().Rotator().Yaw, 0.0f ) ) );

		FTransform HeadToRoomYaw = GetRoomSpaceHeadTransform();
		HeadToRoomYaw.SetRotation( FQuat( FRotator( 0.0f, HeadToRoomYaw.GetRotation().Rotator().Yaw, 0.0f ) ) );

		FTransform RoomToWorldYaw = RoomToWorld;
		RoomToWorldYaw.SetRotation( FQuat( FRotator( 0.0f, RoomToWorldYaw.GetRotation().Rotator().Yaw, 0.0f ) ) );

		FTransform ResultToWorld = ( HeadToRoomYaw.Inverse() * ViewportToRoomYaw ) * RoomToWorldYaw;
		SetRoomTransform( ResultToWorld );
	}

	if ( AvatarActor == nullptr )
	{
		SpawnAvatarMeshActor();
	}

	TickHandle.Broadcast( DeltaTime );

	WorldInteraction->Tick( ViewportClient, DeltaTime );

	UISystem->Tick( ViewportClient, DeltaTime );

	// Update avatar meshes
	{
		// Move our avatar mesh along with the room.  We need our hand components to remain the same coordinate space as the 
		AvatarActor->SetActorTransform( GetRoomTransform() );
		AvatarActor->TickManually( DeltaTime );
	}

	// Updating the scale and intensity of the flashlight according to the world scale
	if (FlashlightComponent)
	{
		float CurrentFalloffExponent = FlashlightComponent->LightFalloffExponent;
		//@todo vreditor tweak
		float UpdatedFalloffExponent = FMath::Clamp(CurrentFalloffExponent / GetWorldScaleFactor(), 2.0f, 16.0f);
		FlashlightComponent->SetLightFalloffExponent(UpdatedFalloffExponent);
	}

	StopOldHapticEffects();

	bFirstTick = false;
}


bool FVREditorMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	// Only if this is our VR viewport. Remember, editor modes get a chance to tick and receive input for each active viewport.
	if ( ViewportClient != GetLevelViewportPossessedForVR().GetViewportClient().Get() )
	{
		return InputKey(GetLevelViewportPossessedForVR().GetViewportClient().Get(), Viewport, Key, Event);
	}

	if ( Key == EKeys::Escape )
	{
		// User hit escape, so bail out of VR mode
		StartExitingVRMode();
	}
	else if( Key.IsMouseButton() )	// Input preprocessor cannot handle mouse buttons, so we'll route those the normal way
	{
		return WorldInteraction->HandleInputKey( Key, Event );
	}

	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}


bool FVREditorMode::InputAxis(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime)
{
	// Only if this is our VR viewport. Remember, editor modes get a chance to tick and receive input for each active viewport.
	if ( ViewportClient != GetLevelViewportPossessedForVR().GetViewportClient().Get() )
	{
		return InputAxis(GetLevelViewportPossessedForVR().GetViewportClient().Get(), Viewport, ControllerId, Key, Delta, DeltaTime);
	}

	return FEdMode::InputAxis(ViewportClient, Viewport, ControllerId, Key, Delta, DeltaTime);
}

bool FVREditorMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	// We are compatible with all other modes!
	return true;
}


void FVREditorMode::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( AvatarActor );
	Collector.AddReferencedObject( FlashlightComponent );	
	Collector.AddReferencedObject( UISystem );
	Collector.AddReferencedObject( WorldInteraction );
	Collector.AddReferencedObject( TeleporterSystem );
	Collector.AddReferencedObject( MouseCursorInteractor );
	Collector.AddReferencedObject( LeftHandInteractor );
	Collector.AddReferencedObject( RightHandInteractor );
}


void FVREditorMode::Render( const FSceneView* SceneView, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
	//StopOldHapticEffects(); //@todo vreditor

	FEdMode::Render( SceneView, Viewport, PDI );

	if( bIsFullyInitialized )
	{
		// Let our subsystems render, too
		UISystem->Render( SceneView, Viewport, PDI );
	}
}

/************************************************************************/
/* IVREditorMode interface                                              */
/************************************************************************/

AActor* FVREditorMode::GetAvatarMeshActor()
{
	return AvatarActor;
}

UWorld* FVREditorMode::GetWorld() const
{
	return WorldInteraction->GetViewportWorld();
}

FTransform FVREditorMode::GetRoomTransform() const
{
	return WorldInteraction->GetRoomTransform();
}

void FVREditorMode::SetRoomTransform( const FTransform& NewRoomTransform )
{
	WorldInteraction->SetRoomTransform( NewRoomTransform );
}

FTransform FVREditorMode::GetRoomSpaceHeadTransform() const
{
	return WorldInteraction->GetRoomSpaceHeadTransform();
}

FTransform FVREditorMode::GetHeadTransform() const
{
	return WorldInteraction->GetHeadTransform();
}

const UViewportWorldInteraction& FVREditorMode::GetWorldInteraction() const
{
	return *WorldInteraction;
}

UViewportWorldInteraction& FVREditorMode::GetWorldInteraction()
{
	return *WorldInteraction;
}

bool FVREditorMode::IsFullyInitialized() const
{
	return bIsFullyInitialized;
}

AActor* FVREditorMode::SpawnTransientSceneActor( TSubclassOf<AActor> ActorClass, const FString& ActorName, const bool bWithSceneComponent ) const
{
	const bool bWasWorldPackageDirty = GetWorld()->GetOutermost()->IsDirty();

	// @todo vreditor: Needs respawn if world changes (map load, etc.)  Will that always restart the editor mode anyway?
	FActorSpawnParameters ActorSpawnParameters;
	ActorSpawnParameters.Name = MakeUniqueObjectName( GetWorld(), ActorClass, *ActorName );	// @todo vreditor: Without this, SpawnActor() can return us an existing PendingKill actor of the same name!  WTF?
	ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActorSpawnParameters.ObjectFlags = EObjectFlags::RF_Transient;

	check( ActorClass != nullptr );
	AActor* NewActor = GetWorld()->SpawnActor< AActor >( ActorClass, ActorSpawnParameters );
	NewActor->SetActorLabel( ActorName );

	if( bWithSceneComponent )
	{
		// Give the new actor a root scene component, so we can attach multiple sibling components to it
		USceneComponent* SceneComponent = NewObject<USceneComponent>( NewActor );
		NewActor->AddOwnedComponent( SceneComponent );
		NewActor->SetRootComponent( SceneComponent );
		SceneComponent->RegisterComponent();
	}

	// Don't dirty the level file after spawning a transient actor
	if( !bWasWorldPackageDirty )
	{
		GetWorld()->GetOutermost()->SetDirtyFlag( false );
	}

	return NewActor;
}


void FVREditorMode::DestroyTransientActor( AActor* Actor ) const
{
	WorldInteraction->DestroyTransientActor( Actor );
}

const SLevelViewport& FVREditorMode::GetLevelViewportPossessedForVR() const
{
	return *VREditorLevelViewportWeakPtr.Pin();
}

SLevelViewport& FVREditorMode::GetLevelViewportPossessedForVR()
{
	return *VREditorLevelViewportWeakPtr.Pin();
}


float FVREditorMode::GetWorldScaleFactor() const
{
	return WorldInteraction->GetViewportWorld()->GetWorldSettings()->WorldToMeters / 100.0f;
}

void FVREditorMode::OnMapChange( uint32 MapChangeFlags )
{
	CleanUpActorsBeforeMapChangeOrSimulate();
}


void FVREditorMode::OnBeginPIE( const bool bIsSimulatingInEditor )
{
	CleanUpActorsBeforeMapChangeOrSimulate();
}


void FVREditorMode::OnEndPIE( const bool bIsSimulatingInEditor )
{
	CleanUpActorsBeforeMapChangeOrSimulate();
}


void FVREditorMode::OnSwitchBetweenPIEAndSIE( const bool bIsSimulatingInEditor )
{
	CleanUpActorsBeforeMapChangeOrSimulate();
}


void FVREditorMode::CleanUpActorsBeforeMapChangeOrSimulate()
{
	if ( WorldInteraction != nullptr )
	{
		// NOTE: This will be called even when this mode is not currently active!
		DestroyTransientActor( AvatarActor );
		AvatarActor = nullptr;
		FlashlightComponent = nullptr;

		if ( UISystem != nullptr )
		{
			UISystem->CleanUpActorsBeforeMapChangeOrSimulate();
		}

		WorldInteraction->Shutdown();
	}
}

void FVREditorMode::ToggleFlashlight( UVREditorInteractor* Interactor )
{
	UVREditorMotionControllerInteractor* MotionControllerInteractor = Cast<UVREditorMotionControllerInteractor>( Interactor );
	if ( MotionControllerInteractor )
	{
		if ( FlashlightComponent == nullptr )
		{
			FlashlightComponent = NewObject<USpotLightComponent>( AvatarActor );
			AvatarActor->AddOwnedComponent( FlashlightComponent );
			FlashlightComponent->RegisterComponent();
			FlashlightComponent->SetMobility( EComponentMobility::Movable );
			FlashlightComponent->SetCastShadows( false );
			FlashlightComponent->bUseInverseSquaredFalloff = false;
			//@todo vreditor tweak
			FlashlightComponent->SetLightFalloffExponent( 8.0f );
			FlashlightComponent->SetIntensity( 20.0f );
			FlashlightComponent->SetOuterConeAngle( 25.0f );
			FlashlightComponent->SetInnerConeAngle( 25.0f );

		}

		const FAttachmentTransformRules AttachmentTransformRules = FAttachmentTransformRules( EAttachmentRule::KeepRelative, true );
		FlashlightComponent->AttachToComponent( MotionControllerInteractor->GetMotionControllerComponent(), AttachmentTransformRules );
		bIsFlashlightOn = !bIsFlashlightOn;
		FlashlightComponent->SetVisibility( bIsFlashlightOn );
	}
}

void FVREditorMode::CycleTransformGizmoHandleType()
{
	EGizmoHandleTypes NewGizmoType = (EGizmoHandleTypes)( (uint8)WorldInteraction->GetCurrentGizmoType() + 1 );
	
	if( NewGizmoType > EGizmoHandleTypes::Scale )
	{
		NewGizmoType = EGizmoHandleTypes::All;
	}

	// Set coordinate system to local if the next gizmo will be for non-uniform scaling 
	if ( NewGizmoType == EGizmoHandleTypes::Scale )
	{
		const ECoordSystem CurrentCoordSystem = WorldInteraction->GetTransformGizmoCoordinateSpace();
		if ( CurrentCoordSystem == COORD_World )
		{
			GLevelEditorModeTools().SetCoordSystem( COORD_Local );
			// Remember if coordinate system was in world space before scaling
			bWasInWorldSpaceBeforeScaleMode = true;
		}
		else if ( CurrentCoordSystem == COORD_Local )
		{
			bWasInWorldSpaceBeforeScaleMode = false;
		}
	} 
	else if ( WorldInteraction->GetCurrentGizmoType() == EGizmoHandleTypes::Scale && bWasInWorldSpaceBeforeScaleMode )
	{
		// Set the coordinate system to world space if the coordinate system was world before scaling
		WorldInteraction->SetTransformGizmoCoordinateSpace( COORD_World );
	}
	
	WorldInteraction->SetGizmoHandleType( NewGizmoType );
}

EHMDDeviceType::Type FVREditorMode::GetHMDDeviceType() const
{
	return GEngine->HMDDevice.IsValid() ? GEngine->HMDDevice->GetHMDDeviceType() : EHMDDeviceType::DT_SteamVR;
}

FLinearColor FVREditorMode::GetColor( const EColors Color ) const
{
	return Colors[ (int32)Color ];
}

bool FVREditorMode::IsHandAimingTowardsCapsule( UViewportInteractor* Interactor, const FTransform& CapsuleTransform, FVector CapsuleStart, const FVector CapsuleEnd, const float CapsuleRadius, const float MinDistanceToCapsule, const FVector CapsuleFrontDirection, const float MinDotForAimingAtCapsule ) const
{
	bool bIsAimingTowards = false;
	const float WorldScaleFactor = GetWorldScaleFactor();

	FVector LaserPointerStart, LaserPointerEnd;
	if( Interactor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd ) )
	{
		const FVector LaserPointerStartInCapsuleSpace = CapsuleTransform.InverseTransformPosition( LaserPointerStart );
		const FVector LaserPointerEndInCapsuleSpace = CapsuleTransform.InverseTransformPosition( LaserPointerEnd );

		FVector ClosestPointOnLaserPointer, ClosestPointOnUICapsule;
		FMath::SegmentDistToSegment(
			LaserPointerStartInCapsuleSpace, LaserPointerEndInCapsuleSpace,
			CapsuleStart, CapsuleEnd,
			/* Out */ ClosestPointOnLaserPointer,
			/* Out */ ClosestPointOnUICapsule );

		const bool bIsClosestPointInsideCapsule = ( ClosestPointOnLaserPointer - ClosestPointOnUICapsule ).Size() <= CapsuleRadius;

		const FVector TowardLaserPointerVector = ( ClosestPointOnLaserPointer - ClosestPointOnUICapsule ).GetSafeNormal();

		// Apply capsule radius
		ClosestPointOnUICapsule += TowardLaserPointerVector * CapsuleRadius;

		if( false )	// @todo vreditor debug
		{
			const float RenderCapsuleLength = ( CapsuleEnd - CapsuleStart ).Size() + CapsuleRadius * 2.0f;
			// @todo vreditor:  This capsule draws with the wrong orientation
			if( false )
			{
				DrawDebugCapsule( GetWorld(), CapsuleTransform.TransformPosition( CapsuleStart + ( CapsuleEnd - CapsuleStart ) * 0.5f ), RenderCapsuleLength * 0.5f, CapsuleRadius, CapsuleTransform.GetRotation() * FRotator( 90.0f, 0, 0 ).Quaternion(), FColor::Green, false, 0.0f );
			}
			DrawDebugLine( GetWorld(), CapsuleTransform.TransformPosition( ClosestPointOnLaserPointer ), CapsuleTransform.TransformPosition( ClosestPointOnUICapsule ), FColor::Green, false, 0.0f );
			DrawDebugSphere( GetWorld(), CapsuleTransform.TransformPosition( ClosestPointOnLaserPointer ), 1.5f * WorldScaleFactor, 32, FColor::Red, false, 0.0f );
			DrawDebugSphere( GetWorld(), CapsuleTransform.TransformPosition( ClosestPointOnUICapsule ), 1.5f * WorldScaleFactor, 32, FColor::Green, false, 0.0f );
		}

		// If we're really close to the capsule
		if( bIsClosestPointInsideCapsule ||
			( ClosestPointOnUICapsule - ClosestPointOnLaserPointer ).Size() <= MinDistanceToCapsule )
		{
			const FVector LaserPointerDirectionInCapsuleSpace = ( LaserPointerEndInCapsuleSpace - LaserPointerStartInCapsuleSpace ).GetSafeNormal();

			if( false )	// @todo vreditor debug
			{
				DrawDebugLine( GetWorld(), CapsuleTransform.TransformPosition( FVector::ZeroVector ), CapsuleTransform.TransformPosition( CapsuleFrontDirection * 5.0f ), FColor::Yellow, false, 0.0f );
				DrawDebugLine( GetWorld(), CapsuleTransform.TransformPosition( FVector::ZeroVector ), CapsuleTransform.TransformPosition( -LaserPointerDirectionInCapsuleSpace * 5.0f ), FColor::Purple, false, 0.0f );
			}

			const float Dot = FVector::DotProduct( CapsuleFrontDirection, -LaserPointerDirectionInCapsuleSpace );
			if( Dot >= MinDotForAimingAtCapsule )
			{
				bIsAimingTowards = true;
			}
		}
	}

	return bIsAimingTowards;
}

UVREditorInteractor* FVREditorMode::GetHandInteractor( const EControllerHand ControllerHand ) const 
{
	UVREditorInteractor* ResultInteractor = ControllerHand == EControllerHand::Left ? LeftHandInteractor : RightHandInteractor;
	check( ResultInteractor != nullptr );
	return ResultInteractor;
}

void FVREditorMode::StopOldHapticEffects()
{
	LeftHandInteractor->StopOldHapticEffects();
	RightHandInteractor->StopOldHapticEffects();
}

#undef LOCTEXT_NAMESPACE