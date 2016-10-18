// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorUISystem.h"
#include "VREditorMode.h"
#include "VREditorFloatingUI.h"
#include "VREditorDockableWindow.h"
#include "VREditorQuickMenu.h"
#include "VREditorRadialMenu.h"
#include "VREditorRadialMenuItem.h"
#include "IHeadMountedDisplay.h"
#include "VREditorWorldInteraction.h"
#include "ViewportInteractor.h"
#include "VREditorInteractor.h"
#include "VREditorMotionControllerInteractor.h"

// UI
#include "WidgetComponent.h"
#include "VREditorWidgetComponent.h"

// Content Browser
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "Editor/ContentBrowser/Public/IContentBrowserSingleton.h"

// World Outliner
#include "Editor/SceneOutliner/Public/SceneOutliner.h"

// Actor Details, Modes
#include "LevelEditor.h"

// World Settings
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"

#include "SLevelViewport.h"
#include "SScaleBox.h"
#include "SDPIScaler.h"
#include "SWidget.h"
#include "SDockTab.h"
#include "SColorPicker.h"

namespace VREd
{
	static FAutoConsoleVariable ContentBrowserUIResolutionX( TEXT( "VREd.ContentBrowserUIResolutionX" ), 1920, TEXT( "Horizontal resolution to use for content browser UI render targets" ) );
	static FAutoConsoleVariable ContentBrowserUIResolutionY( TEXT( "VREd.ContentBrowserUIResolutionY" ), 1200, TEXT( "Vertical resolution to use for content browser UI render targets" ) );
	static FAutoConsoleVariable DefaultEditorUIResolutionX( TEXT( "VREd.DefaultEditorUIResolutionX" ), 1024, TEXT( "Horizontal resolution to use for VR editor UI render targets" ) );
	static FAutoConsoleVariable DefaultEditorUIResolutionY( TEXT( "VREd.DefaultEditorUIResolutionY" ), 1024, TEXT( "Vertical resolution to use for VR editor UI render targets" ) );
	static FAutoConsoleVariable QuickMenuUIResolutionX( TEXT( "VREd.QuickMenuUIResolutionX" ), 1024, TEXT( "Horizontal resolution to use for Quick Menu VR UI render targets" ) );
	static FAutoConsoleVariable QuickMenuUIResolutionY( TEXT( "VREd.QuickMenuUIResolutionY" ), 900, TEXT( "Vertical resolution to use for Quick Menu VR UI render targets" ) );
	static FAutoConsoleVariable ContentBrowserUISize( TEXT( "VREd.ContentBrowserUISize" ), 60.0f, TEXT( "How big content browser UIs should be" ) );
	static FAutoConsoleVariable EditorUISize( TEXT( "VREd.EditorUISize" ), 50.0f, TEXT( "How big editor UIs should be" ) );
	static FAutoConsoleVariable ContentBrowserUIScale( TEXT( "VREd.ContentBrowserUIScale" ), 2.0f, TEXT( "How much to scale up (or down) the content browser for VR" ) );
	static FAutoConsoleVariable EditorUIScale( TEXT( "VREd.EditorUIScale" ), 2.0f, TEXT( "How much to scale up (or down) editor UIs for VR" ) );
	static FAutoConsoleVariable EditorUIOffsetFromHand( TEXT( "VREd.EditorUIOffsetFromHand" ), 12.0f, TEXT( "How far to offset editor UIs from the origin of the hand mesh" ) );
	static FAutoConsoleVariable AssetEditorUIResolutionX( TEXT( "VREd.AssetEditorUIResolutionX" ), 1920, TEXT( "Horizontal resolution to use for VR editor asset editor UI render targets" ) );
	static FAutoConsoleVariable AssetEditorUIResolutionY( TEXT( "VREd.AssetEditorUIResolutionY" ), 1080, TEXT( "Vertical resolution to use for VR editor asset editor UI render targets" ) );
	static FAutoConsoleVariable RadialMenuFadeDelay( TEXT( "VREd.RadialMenuFadeDelay" ), 0.2f, TEXT( "The delay for the radial menu after selecting a button" ) );
	static FAutoConsoleVariable UIAbsoluteScrollSpeed( TEXT( "VREd.UIAbsoluteScrollSpeed" ), 8.0f, TEXT( "How fast the UI scrolls when dragging the touchpad" ) );
	static FAutoConsoleVariable UIRelativeScrollSpeed( TEXT( "VREd.UIRelativeScrollSpeed" ), 0.75f, TEXT( "How fast the UI scrolls when holding an analog stick" ) );
	static FAutoConsoleVariable MinUIScrollDeltaForInertia( TEXT( "VREd.MinUIScrollDeltaForInertia" ), 0.25f, TEXT( "Minimum amount of touch pad input before inertial UI scrolling kicks in" ) );
	static FAutoConsoleVariable MinDockDragDistance( TEXT( "VREd.MinDockDragDistance" ), 10.0f, TEXT( "Minimum amount of distance needed to drag dock from hand" ) );
	static FAutoConsoleVariable DoubleClickTime( TEXT( "VREd.DoubleClickTime" ), 0.25f, TEXT( "Minimum duration between clicks for a double click event to fire" ) );
	static FAutoConsoleVariable UIPressHapticFeedbackStrength( TEXT( "VREd.UIPressHapticFeedbackStrength" ), 0.4f, TEXT( "Strenth of haptic when clicking on the UI" ) );
	static FAutoConsoleVariable UIAssetEditorSummonedOnHandHapticFeedbackStrength( TEXT( "VREd.UIAssetEditorSummonedOnHandHapticFeedbackStrength" ), 1.0f, TEXT( "Strenth of haptic to play to remind a user which hand an asset editor was spawned on" ) );
	static FAutoConsoleVariable MaxDockWindowSize( TEXT( "VREd.MaxDockWindowSize" ), 250, TEXT( "Maximum size for dockable windows" ) );
	static FAutoConsoleVariable MinDockWindowSize( TEXT( "VREd.MinDockWindowSize" ), 40, TEXT( "Minimum size for dockable windows" ) );
	static FAutoConsoleVariable DockSnapTime( TEXT( "VREd.DockSnapTime" ), 0.1f, TEXT( "Lerp time for snapping dock to hand and back" ) );
	static FAutoConsoleVariable DockDragSpeed( TEXT( "VREd.DockDragSpeed" ), 0.05f, TEXT( "Dock dragging time delay" ) );

	// Tutorial UI commands
	static FAutoConsoleVariable TutorialUIResolutionX( TEXT( "VREd.TutorialUI.Resolution.X" ), 1920, TEXT( "The X resolution for the tutorial UI panel" ) );
	static FAutoConsoleVariable TutorialUIResolutionY( TEXT( "VREd.TutorialUI.Resolution.Y" ), 1080, TEXT( "The Y resolution for the tutorial UI panel" ) );
	static FAutoConsoleVariable TutorialUISize( TEXT( "VREd.TutorialUI.Size" ), 200, TEXT( "The room space size for the tutorial UI panel" ) );
	static FAutoConsoleVariable TutorialUIYaw( TEXT( "VREd.TutorialUI.Yaw" ), 180, TEXT( "The yaw rotation for the tutorial UI panel" ) );
	static FAutoConsoleVariable TutorialUIPitch( TEXT( "VREd.TutorialUI.Pitch" ), 45, TEXT( "The pitch rotation for the tutorial UI panel" ) );
	static FAutoConsoleVariable TutorialUILocationX( TEXT( "VREd.TutorialUI.Location.X" ), 200, TEXT( "The X location for the tutorial UI panel" ) );
	static FAutoConsoleVariable TutorialUILocationY( TEXT( "VREd.TutorialUI.Location.Y" ), 0, TEXT( "The Y location for the tutorial UI panel" ) );
	static FAutoConsoleVariable TutorialUILocationZ( TEXT( "VREd.TutorialUI.Location.Z" ), 40, TEXT( "The Z location for the tutorial UI panel" ) );

	static const FTransform DefaultModesTransform(			FRotator( -30,  130, 0 ), FVector( 40, -45,  40 ), FVector( 1 ) );	// Top left
	static const FTransform DefaultWorldOutlinerTransform(	FRotator( -30, -130, 0 ), FVector( 40,  45,  40 ), FVector( 1 ) );	// Top right
	static const FTransform DefaultContentBrowserTransform( FRotator(  20,  160, 0 ), FVector( 50, -65, -40 ), FVector( 1 ) );  // Bottom left
	static const FTransform DefaultActorDetailsTransform(	FRotator(  20, -160, 0 ), FVector( 50,  65, -40 ), FVector( 1 ) );	// Bottom right

	static const FTransform DefaultColorPickerTransform(	FRotator( -10, 180, 0), FVector( 30, 35, 0 ), FVector( 1 ) );
	
}


UVREditorUISystem::UVREditorUISystem( const FObjectInitializer& Initializer ) : 
	Super( Initializer ),
	VRMode( nullptr ),
	FloatingUIs(),
	QuickMenuUI( nullptr ),
	QuickRadialMenu( nullptr ),
	RadialMenuHideDelayTime( 0.0f ),
	QuickMenuWidgetClass( nullptr ),
	QuickRadialWidgetClass( nullptr ),
	TutorialWidgetClass( nullptr ),
	InteractorDraggingUI( nullptr ),
	DraggingUIOffsetTransform( FTransform::Identity ),
	DraggingUI( nullptr ),
	bRefocusViewport(false),
	StartDragUISound( nullptr ),
	StopDragUISound( nullptr ),
	HideUISound( nullptr ),
	ShowUISound( nullptr ),
	bDraggedDockFromHandPassedThreshold( false ),
	LastDraggingHoverLocation( FVector::ZeroVector ),
	bSetDefaultLayout( true )
{
	EditorUIPanels.SetNumZeroed( (int32)EEditorUIPanel::TotalCount );
	
	// Set default layout transform in correct order of enum
	DefaultWindowTransforms.Add( VREd::DefaultContentBrowserTransform );
	DefaultWindowTransforms.Add( VREd::DefaultWorldOutlinerTransform );
	DefaultWindowTransforms.Add( VREd::DefaultActorDetailsTransform );
	DefaultWindowTransforms.Add( VREd::DefaultModesTransform );
}

void UVREditorUISystem::Init()
{
	// Register to find out about VR events
	GetOwner().GetWorldInteraction().OnViewportInteractionInputAction().AddUObject( this, &UVREditorUISystem::OnVRAction );
	GetOwner().GetWorldInteraction().OnViewportInteractionHoverUpdate().AddUObject( this, &UVREditorUISystem::OnVRHoverUpdate );

	QuickMenuWidgetClass = LoadClass<UVREditorQuickMenu>( nullptr, TEXT( "/Engine/VREditor/UI/VRQuickMenu.VRQuickMenu_C" ) );
	check( QuickMenuWidgetClass != nullptr );

	QuickRadialWidgetClass = LoadClass<UVREditorRadialMenu>( nullptr, TEXT( "/Engine/VREditor/UI/VRRadialQuickMenu.VRRadialQuickMenu_C" ) );
	check( QuickRadialWidgetClass != nullptr );

	TutorialWidgetClass = LoadClass<UVREditorBaseUserWidget>( nullptr, TEXT( "/Engine/VREditor/Tutorial/UI_VR_Tutorial_00.UI_VR_Tutorial_00_C" ) );
	check( TutorialWidgetClass != nullptr );

	// Load sounds
	StartDragUISound = LoadObject<USoundCue>( nullptr, TEXT( "/Engine/VREditor/Sounds/VR_negative_Cue" ) );
	check( StartDragUISound != nullptr );

	StopDragUISound = LoadObject<USoundCue>( nullptr, TEXT( "/Engine/VREditor/Sounds/VR_negative_Cue" ) );
	check( StopDragUISound != nullptr );

	HideUISound = LoadObject<USoundCue>( nullptr, TEXT( "/Engine/VREditor/Sounds/VR_close_Cue" ) );
	check( HideUISound != nullptr );

	ShowUISound = LoadObject<USoundCue>( nullptr, TEXT( "/Engine/VREditor/Sounds/VR_open_Cue" ) );
	check( ShowUISound != nullptr );

	
	// Create all of our UI panels
	CreateUIs();


	// Bind the color picker creation & destruction overrides
	SColorPicker::OnColorPickerNonModalCreateOverride.BindUObject(this, &UVREditorUISystem::CreateVRColorPicker);
	SColorPicker::OnColorPickerDestroyOverride.BindUObject(this, &UVREditorUISystem::DestroyVRColorPicker);
}


void UVREditorUISystem::Shutdown()
{
	if ( VRMode != nullptr && &VRMode->GetWorldInteraction() != nullptr )
	{
		GetOwner().GetWorldInteraction().OnViewportInteractionInputAction().RemoveAll( this );
		GetOwner().GetWorldInteraction().OnViewportInteractionHoverUpdate().RemoveAll( this );
	}

	// Unbind the color picker creation & destruction overrides
	SColorPicker::OnColorPickerNonModalCreateOverride.Unbind();
	SColorPicker::OnColorPickerDestroyOverride.Unbind();

	CleanUpActorsBeforeMapChangeOrSimulate();

	DefaultWindowTransforms.Empty();

	QuickMenuWidgetClass = nullptr;
	QuickRadialWidgetClass = nullptr;
	TutorialWidgetClass = nullptr;
	StartDragUISound = nullptr;
	StopDragUISound = nullptr;
	HideUISound = nullptr;
	ShowUISound = nullptr;
	VRMode = nullptr;
}

void UVREditorUISystem::OnVRAction( FEditorViewportClient& ViewportClient, UViewportInteractor* Interactor,
	const FViewportActionKeyInput& Action, bool& bOutIsInputCaptured, bool& bWasHandled )
{
	UVREditorMotionControllerInteractor* VREditorInteractor = Cast<UVREditorMotionControllerInteractor>( Interactor );
	if ( VREditorInteractor )
	{
		if ( !bWasHandled )
		{
			if ( Action.ActionType == VRActionTypes::ConfirmRadialSelection )
			{
				if ( Action.Event == IE_Pressed )
				{
					UVREditorRadialMenu* RadialMenu = QuickRadialMenu->GetUserWidget<UVREditorRadialMenu>();
					if ( IsShowingRadialMenu( VREditorInteractor ) )
					{
						RadialMenu->Update( VREditorInteractor );
					}

					RadialMenu->SelectCurrentItem();
				}

				bWasHandled = true;
			}
			else if ( Action.ActionType == ViewportWorldActionTypes::SelectAndMove_LightlyPressed )
			{
				FVector LaserPointerStart, LaserPointerEnd;
				if ( VREditorInteractor->GetLaserPointer( LaserPointerStart, LaserPointerEnd ) )
				{
					FHitResult HitResult = VREditorInteractor->GetHitResultFromLaserPointer();
					if ( HitResult.Actor.IsValid() )
					{
						// Only allow clicks to our own widget components
						UWidgetComponent* WidgetComponent = Cast<UWidgetComponent>( HitResult.GetComponent() );
						if ( WidgetComponent != nullptr && IsWidgetAnEditorUIWidget( WidgetComponent ) )
						{
							// Always mark the event as handled so that the editor doesn't try to select the widget component
							bWasHandled = true;

							if ( Action.Event != IE_Repeat )
							{
								// If the Modifier button is held down, treat this like a right click instead of a left click
								const bool bIsRightClicking =
									( Action.Event == IE_Pressed && VREditorInteractor->IsModifierPressed() ) ||
									( Action.Event == IE_Released && VREditorInteractor->IsRightClickingOnUI() );

								FVector2D LastLocalHitLocation = WidgetComponent->GetLastLocalHitLocation();

								FVector2D LocalHitLocation;
								WidgetComponent->GetLocalHitLocation( HitResult.ImpactPoint, LocalHitLocation );

								// If we weren't already hovering over this widget, then we'll reset the last hit location
								if ( WidgetComponent != VREditorInteractor->GetHoveringOverWidgetComponent() )
								{
									LastLocalHitLocation = LocalHitLocation;

									if ( UVREditorWidgetComponent* VRWidgetComponent = Cast<UVREditorWidgetComponent>( VREditorInteractor->GetHoveringOverWidgetComponent() ) )
									{
										VRWidgetComponent->SetIsHovering( false );
									}
								}

								FWidgetPath WidgetPathUnderFinger = FWidgetPath( WidgetComponent->GetHitWidgetPath( HitResult.ImpactPoint, /*bIgnoreEnabledStatus*/ false ) );
								if ( WidgetPathUnderFinger.IsValid() )
								{
									TSet<FKey> PressedButtons;
									if ( Action.Event == IE_Pressed )
									{
										PressedButtons.Add( bIsRightClicking ? EKeys::RightMouseButton : EKeys::LeftMouseButton );
									}

									FPointerEvent PointerEvent(
										1 + (uint8)VREditorInteractor->GetControllerSide(),
										LocalHitLocation,
										LastLocalHitLocation,
										PressedButtons,
										bIsRightClicking ? EKeys::RightMouseButton : EKeys::LeftMouseButton,
										0.0f,	// Wheel delta
										FModifierKeysState() );

									VREditorInteractor->SetHoveringOverWidgetComponent( WidgetComponent );

									if ( UVREditorWidgetComponent* VRWidgetComponent = Cast<UVREditorWidgetComponent>( VREditorInteractor->GetHoveringOverWidgetComponent() ) )
									{
										VRWidgetComponent->SetIsHovering( true );
									}

									FReply Reply = FReply::Unhandled();
									if ( Action.Event == IE_Pressed )
									{
										const double CurrentTime = FPlatformTime::Seconds();
										if (CurrentTime - VREditorInteractor->GetLastClickReleaseTime() <= VREd::DoubleClickTime->GetFloat())
										{
											// Trigger a double click event!
											Reply = FSlateApplication::Get().RoutePointerDoubleClickEvent(WidgetPathUnderFinger, PointerEvent);
										}
										else
										{
											Reply = FSlateApplication::Get().RoutePointerDownEvent(WidgetPathUnderFinger, PointerEvent);
										}
										
										VREditorInteractor->SetIsClickingOnUI( true );
										VREditorInteractor->SetIsRightClickingOnUI( bIsRightClicking );
										VREditorInteractor->SetLastClickReleaseTime( CurrentTime );
										bOutIsInputCaptured = true;

										// Play a haptic effect on press
										VREditorInteractor->PlayHapticEffect( VREd::UIPressHapticFeedbackStrength->GetFloat() );
									}
									else if ( Action.Event == IE_Released )
									{
										Reply = FSlateApplication::Get().RoutePointerUpEvent( WidgetPathUnderFinger, PointerEvent );
									}
								}
							}
						}
					}
				}

				if ( Action.Event == IE_Released )
				{
					bool bWasRightClicking = false;
					if ( VREditorInteractor->IsClickingOnUI() )
					{
						if ( VREditorInteractor->IsRightClickingOnUI() )
						{
							bWasRightClicking = true;
						}
						VREditorInteractor->SetIsClickingOnUI( false );
						VREditorInteractor->SetIsRightClickingOnUI( false );
						bOutIsInputCaptured = false;
					}

					if ( !bWasHandled )
					{
						TSet<FKey> PressedButtons;
						FPointerEvent PointerEvent(
							1 + (uint8)VREditorInteractor->GetControllerSide(),
							FVector2D::ZeroVector,
							FVector2D::ZeroVector,
							PressedButtons,
							bWasRightClicking ? EKeys::RightMouseButton : EKeys::LeftMouseButton,
							0.0f,	// Wheel delta
							FModifierKeysState() );

						FWidgetPath EmptyWidgetPath;

						VREditorInteractor->SetIsClickingOnUI( false );
						VREditorInteractor->SetIsRightClickingOnUI( false );
						FReply Reply = FSlateApplication::Get().RoutePointerUpEvent( EmptyWidgetPath, PointerEvent );
					}
				}
			}
		}
	}
}


void UVREditorUISystem::OnVRHoverUpdate( FEditorViewportClient& ViewportClient, UViewportInteractor* Interactor, FVector& HoverImpactPoint, bool& bWasHandled )
{
	UVREditorMotionControllerInteractor* VREditorInteractor = Cast<UVREditorMotionControllerInteractor>( Interactor );
	if( VREditorInteractor != nullptr )
	{
		if( !bWasHandled && Interactor->GetDraggingMode() != EViewportInteractionDraggingMode::Interactable )
		{
			FVector LaserPointerStart, LaserPointerEnd;
			if( Interactor->GetLaserPointer( LaserPointerStart, LaserPointerEnd ) )
			{
				FHitResult HitResult = Interactor->GetHitResultFromLaserPointer();
				if( HitResult.Actor.IsValid() )
				{
					// Only allow clicks to our own widget components
					UWidgetComponent* WidgetComponent = Cast<UWidgetComponent>( HitResult.GetComponent() );
					if( WidgetComponent != nullptr && IsWidgetAnEditorUIWidget( WidgetComponent ) )
					{
						FVector2D LastLocalHitLocation = WidgetComponent->GetLastLocalHitLocation();

						FVector2D LocalHitLocation;
						WidgetComponent->GetLocalHitLocation( HitResult.ImpactPoint, LocalHitLocation );

						// If we weren't already hovering over this widget, then we'll reset the last hit location
						if( WidgetComponent != VREditorInteractor->GetHoveringOverWidgetComponent() )
						{
							LastLocalHitLocation = LocalHitLocation;

							if ( UVREditorWidgetComponent* VRWidgetComponent = Cast<UVREditorWidgetComponent>( VREditorInteractor->GetHoveringOverWidgetComponent() ) )
							{
								VRWidgetComponent->SetIsHovering(false);
							}
						}

						// @todo vreditor UI: There is a CursorRadius optional arg that we migth want to make use of wherever we call GetHitWidgetPath()!
						FWidgetPath WidgetPathUnderFinger = FWidgetPath( WidgetComponent->GetHitWidgetPath( HitResult.ImpactPoint, /*bIgnoreEnabledStatus*/ false ) );
						if ( WidgetPathUnderFinger.IsValid() )
						{
							TSet<FKey> PressedButtons;
							FPointerEvent PointerEvent(
								1 + (uint8)VREditorInteractor->GetControllerSide(),
								LocalHitLocation,
								LastLocalHitLocation,
								LocalHitLocation - LastLocalHitLocation,
								PressedButtons,
								FModifierKeysState());

							FSlateApplication::Get().RoutePointerMoveEvent(WidgetPathUnderFinger, PointerEvent, false);

							bWasHandled = true;
							HoverImpactPoint = HitResult.ImpactPoint;
							VREditorInteractor->SetHoveringOverWidgetComponent( WidgetComponent );
							VREditorInteractor->SetIsHoveringOverUI( true );

							if ( UVREditorWidgetComponent* VRWidgetComponent = Cast<UVREditorWidgetComponent>(WidgetComponent) )
							{
								VRWidgetComponent->SetIsHovering(true);
							}

							// Route the mouse scrolling
							if ( VREditorInteractor->IsTrackpadPositionValid( 1 ) )
							{
								const bool bIsAbsolute = ( GetOwner().GetHMDDeviceType() == EHMDDeviceType::DT_SteamVR );

								float ScrollDelta = 0.0f;
								if( VREditorInteractor->IsTouchingTrackpad() || !bIsAbsolute )
								{
									if( bIsAbsolute )
									{
										const float ScrollSpeed = VREd::UIAbsoluteScrollSpeed->GetFloat();
										ScrollDelta = ( VREditorInteractor->GetTrackpadPosition().Y - VREditorInteractor->GetLastTrackpadPosition().Y ) * ScrollSpeed;
									}
									else
									{
										const float ScrollSpeed = VREd::UIRelativeScrollSpeed->GetFloat();
										ScrollDelta = VREditorInteractor->GetTrackpadPosition().Y * ScrollSpeed;
									}
								}

								// If using a trackpad (Vive), invert scroll direction so that it feels more like scrolling on a mobile device
								if( GetOwner().GetHMDDeviceType() == EHMDDeviceType::DT_SteamVR )
								{
									ScrollDelta *= -1.0f;
								}

								if( !FMath::IsNearlyZero( ScrollDelta ) )
								{
									FPointerEvent MouseWheelEvent(
										1 + (uint8)VREditorInteractor->GetControllerSide(),
										LocalHitLocation,
										LastLocalHitLocation,
										PressedButtons,
										EKeys::MouseWheelAxis,
										ScrollDelta,
										FModifierKeysState() );

									FSlateApplication::Get().RouteMouseWheelOrGestureEvent(WidgetPathUnderFinger, MouseWheelEvent, nullptr);

									VREditorInteractor->SetUIScrollVelocity( 0.0f );

									// Don't apply inertia unless the user dragged a decent amount this frame
									if( bIsAbsolute && FMath::Abs( ScrollDelta ) >= VREd::MinUIScrollDeltaForInertia->GetFloat() )
									{
										// Don't apply inertia if our data is sort of old
										const FTimespan CurrentTime = FTimespan::FromSeconds( FPlatformTime::Seconds() );
										if( CurrentTime - VREditorInteractor->GetLastTrackpadPositionUpdateTime() < FTimespan::FromSeconds( 1.0f / 30.0f ) )
										{
											//GWarn->Logf( TEXT( "INPUT: UIScrollVelocity=%0.2f" ), ScrollDelta );
											VREditorInteractor->SetUIScrollVelocity( ScrollDelta );
										}
									}
								}
							}
							else
							{
								if( !FMath::IsNearlyZero( VREditorInteractor->GetUIScrollVelocity() ) )
								{
									// Apply UI scrolling inertia
									const float ScrollDelta = VREditorInteractor->GetUIScrollVelocity();
									{
										FPointerEvent MouseWheelEvent(
											1 + (uint8)VREditorInteractor->GetControllerSide(),
											LocalHitLocation,
											LastLocalHitLocation,
											PressedButtons,
											EKeys::MouseWheelAxis,
											ScrollDelta,
											FModifierKeysState() );

										FSlateApplication::Get().RouteMouseWheelOrGestureEvent( WidgetPathUnderFinger, MouseWheelEvent, nullptr );
									}

									// Apply damping
									FVector ScrollVelocityVector( VREditorInteractor->GetUIScrollVelocity(), 0.0f, 0.0f );
									const bool bVelocitySensitive = false;
									GetOwner().GetWorldInteraction().ApplyVelocityDamping( ScrollVelocityVector, bVelocitySensitive );
									VREditorInteractor->SetUIScrollVelocity( ScrollVelocityVector.X );
								}
								else
								{
									VREditorInteractor->SetUIScrollVelocity( 0.0f );
								}
							}
						}
					}
				}
			}
		}

		// If nothing was hovered, make sure we tell Slate about that
		if( !bWasHandled && VREditorInteractor->GetHoveringOverWidgetComponent() != nullptr )
		{
			if ( UVREditorWidgetComponent* VRWidgetComponent = Cast<UVREditorWidgetComponent>( VREditorInteractor->GetHoveringOverWidgetComponent() ) )
			{
				VRWidgetComponent->SetIsHovering( false );
			}

			const FVector2D LastLocalHitLocation = VREditorInteractor->GetHoveringOverWidgetComponent()->GetLastLocalHitLocation();
			VREditorInteractor->SetHoveringOverWidgetComponent( nullptr );

			TSet<FKey> PressedButtons;
			FPointerEvent PointerEvent(
				1 + (uint8)VREditorInteractor->GetControllerSide(),
				LastLocalHitLocation,
				LastLocalHitLocation,
				FVector2D::ZeroVector,
				PressedButtons,
				FModifierKeysState() );

			FSlateApplication::Get().RoutePointerMoveEvent( FWidgetPath(), PointerEvent, false );
		}
	}
}

void UVREditorUISystem::Tick( FEditorViewportClient* ViewportClient, const float DeltaTime )
{
	if ( bRefocusViewport )
	{
		bRefocusViewport = false;
		FSlateApplication::Get().SetUserFocus(0, ViewportClient->GetEditorViewportWidget());
	}

	// Figure out if one hand is "aiming toward" the other hand.  We'll fade in a UI on the hand being
	// aimed at when the user does this.
	if( QuickMenuUI != nullptr )
	{
		UVREditorInteractor* HandInteractorWithQuickMenu = nullptr;
		if( QuickMenuUI->IsUIVisible() )
		{
			HandInteractorWithQuickMenu = GetOwner().GetHandInteractor( QuickMenuUI->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftArm ? EControllerHand::Left : EControllerHand::Right );
		}

		const float WorldScaleFactor = GetOwner().GetWorldScaleFactor();

		UViewportInteractor* HandInteractorThatNeedsQuickMenu = nullptr;
		for( UViewportInteractor* Interactor : GetOwner().GetWorldInteraction().GetInteractors() )
		{
			bool bShowQuickMenuOnArm = false;

			UViewportInteractor* OtherInteractor = Interactor->GetOtherInteractor();
			if( OtherInteractor != nullptr )
			{
				// @todo vreditor tweak: Weird to hard code this here. Probably should be an accessor on the hand itself, and based on the actual device type
				const FTransform UICapsuleTransform = OtherInteractor->GetTransform();
				const FVector UICapsuleStart = FVector( -9.0f, 0.0f, 0.0f ) * WorldScaleFactor;
				const FVector UICapsuleEnd = FVector( -18.0f, 0.0f, 0.0f ) * WorldScaleFactor;
				const float UICapsuleLocalRadius = 6.0f * WorldScaleFactor;
				const float MinDistanceToUICapsule = 8.0f * WorldScaleFactor; // @todo vreditor tweak
				const FVector UIForwardVector = FVector::UpVector;
				const float MinDotForAimingAtOtherHand = 0.25f;	// @todo vreditor tweak

				if( GetOwner().IsHandAimingTowardsCapsule( Interactor, UICapsuleTransform, UICapsuleStart, UICapsuleEnd, UICapsuleLocalRadius, MinDistanceToUICapsule, UIForwardVector, MinDotForAimingAtOtherHand ) )
				{
					bShowQuickMenuOnArm = true;
				}

				if( bShowQuickMenuOnArm )
				{
					HandInteractorThatNeedsQuickMenu = OtherInteractor;
				}
			}
		}
		
		// If we don't need a quick menu, or if a different hand needs to spawn it, then kill the existing menu
		if( QuickMenuUI->IsUIVisible() && HandInteractorWithQuickMenu != nullptr && 
			( HandInteractorThatNeedsQuickMenu == nullptr || HandInteractorThatNeedsQuickMenu != HandInteractorWithQuickMenu )  )
		{
			// Despawn
			HandInteractorWithQuickMenu->SetHasUIOnForearm( false );
			QuickMenuUI->ShowUI( false );
		}

		if( HandInteractorThatNeedsQuickMenu != nullptr && !QuickMenuUI->IsUIVisible() )
		{
			UVREditorMotionControllerInteractor* MotionControllerHandInteractorThatNeedsQuickMenu = Cast<UVREditorMotionControllerInteractor>( HandInteractorThatNeedsQuickMenu );
			if ( MotionControllerHandInteractorThatNeedsQuickMenu )
			{
				const AVREditorFloatingUI::EDockedTo DockTo = ( MotionControllerHandInteractorThatNeedsQuickMenu->GetControllerSide() == EControllerHand::Left ) ? AVREditorFloatingUI::EDockedTo::LeftArm : AVREditorFloatingUI::EDockedTo::RightArm;
				QuickMenuUI->SetDockedTo( DockTo );
				QuickMenuUI->ShowUI( true );
				MotionControllerHandInteractorThatNeedsQuickMenu->SetHasUIOnForearm( true );
			}
		}
	}

	// Close the radial menu if it was not updated for a while
	RadialMenuHideDelayTime += DeltaTime;
	if( RadialMenuHideDelayTime >= VREd::RadialMenuFadeDelay->GetFloat() )
	{
		UVREditorInteractor* Interactor = GetOwner().GetHandInteractor( QuickRadialMenu->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand ? EControllerHand::Left : EControllerHand::Right );
		HideRadialMenu( Interactor );
	}
	
	// Tick all of our floating UIs
	for( AVREditorFloatingUI* FloatingUIPtr : FloatingUIs )
	{
		if( FloatingUIPtr != nullptr )
		{
			FloatingUIPtr->TickManually( DeltaTime );
		}
	}
}

void UVREditorUISystem::Render( const FSceneView* SceneView, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
	// ...
}

void UVREditorUISystem::CreateUIs()
{
	const FIntPoint DefaultResolution( VREd::DefaultEditorUIResolutionX->GetInt(), VREd::DefaultEditorUIResolutionY->GetInt() );

	const bool bIsVREditorDemo = FParse::Param( FCommandLine::Get(), TEXT( "VREditorDemo" ) );	// @todo vreditor: Remove this when no longer needed
																								
	// @todo vreditor: Tweak
	if ( QuickMenuWidgetClass != nullptr )
	{
		const FIntPoint Resolution( VREd::QuickMenuUIResolutionX->GetInt(), VREd::QuickMenuUIResolutionY->GetInt() );

		const bool bWithSceneComponent = false;
		QuickMenuUI = GetOwner().SpawnTransientSceneActor< AVREditorFloatingUI >(TEXT("QuickMenu"), bWithSceneComponent);
		QuickMenuUI->SetUMGWidget( *this, QuickMenuWidgetClass, Resolution, 30.0f, AVREditorFloatingUI::EDockedTo::Nothing );
		QuickMenuUI->ShowUI( false );
		QuickMenuUI->SetRelativeOffset( FVector( -11.0f, 0.0f, 3.0f ) );
		FloatingUIs.Add( QuickMenuUI );
	}

	// Create the radial UI
	if ( QuickRadialWidgetClass != nullptr )
	{
		TSharedPtr<SWidget> WidgetToDraw;
		QuickRadialMenu = GetOwner().SpawnTransientSceneActor< AVREditorFloatingUI >(TEXT("QuickRadialmenu"), false);
		QuickRadialMenu->SetUMGWidget( *this, QuickRadialWidgetClass, DefaultResolution, 40.0f, AVREditorFloatingUI::EDockedTo::Nothing );
		QuickRadialMenu->ShowUI( false );
		QuickRadialMenu->SetActorEnableCollision( false );
		QuickRadialMenu->SetRelativeOffset( FVector( 10.0f, 0.0f, 10.f ) );
		QuickRadialMenu->SetCollisionOnShow( false );
		FloatingUIs.Add(QuickRadialMenu);
	}

	// Make some editor UIs!
	{
		{
			const FIntPoint Resolution( VREd::ContentBrowserUIResolutionX->GetInt(), VREd::ContentBrowserUIResolutionY->GetInt() );

			IContentBrowserSingleton& ContentBrowserSingleton = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>( "ContentBrowser" ).Get();;

			// @todo vreditor UI: We've turned off a LOT of content browser features that are clunky to use in VR right (pop ups, text fields, etc.)
			FContentBrowserConfig Config;
			Config.bCanShowClasses = false;
			Config.bCanShowRealTimeThumbnails = false;
			Config.InitialAssetViewType = EAssetViewType::Tile;
			Config.bCanShowDevelopersFolder = false;
			Config.bCanShowFolders = false;
			Config.bUsePathPicker = false;
			Config.bCanShowFilters = false;
			Config.bCanShowAssetSearch = false;
			Config.bUseSourcesView = true;
			Config.bExpandSourcesView = true;
			Config.ThumbnailLabel = EThumbnailLabel::NoLabel;
			Config.ThumbnailScale = 0.4f;

			if( bIsVREditorDemo )
			{
				Config.bUsePathPicker = false;
				Config.bCanShowFilters = false;
				Config.bShowAssetPathTree = false;
				Config.bAlwaysShowCollections = true;
				Config.SelectedCollectionName.Name = FName( "Meshes" );	// @todo vreditor: hard-coded collection name and type
				Config.SelectedCollectionName.Type = ECollectionShareType::CST_Shared;
			}
			else
			{
				Config.bCanShowFilters = true;
				Config.bUsePathPicker = true;
				Config.bShowAssetPathTree = true;
				Config.bAlwaysShowCollections = false;
			}

			Config.bShowBottomToolbar = false;
			Config.bCanShowLockButton = false;
			TSharedRef<SWidget> ContentBrowser = ContentBrowserSingleton.CreateContentBrowser( "VRContentBrowser", nullptr, &Config );

			TSharedRef<SWidget> WidgetToDraw =
				SNew( SDPIScaler )
				.DPIScale( VREd::ContentBrowserUIScale->GetFloat() )
				[
					ContentBrowser
				]
			;

			const bool bWithSceneComponent = false;
			AVREditorFloatingUI* ContentBrowserUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >( TEXT( "ContentBrowserUI" ), bWithSceneComponent );
			ContentBrowserUI->SetSlateWidget( *this, WidgetToDraw, Resolution, VREd::ContentBrowserUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Nothing );
			ContentBrowserUI->ShowUI( false );
			FloatingUIs.Add( ContentBrowserUI );

			EditorUIPanels[ (int32)EEditorUIPanel::ContentBrowser ] = ContentBrowserUI;
		}

		{
			FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::Get().LoadModuleChecked<FSceneOutlinerModule>( "SceneOutliner" );

			SceneOutliner::FInitializationOptions InitOptions;
			InitOptions.Mode = ESceneOutlinerMode::ActorBrowsing;

			TSharedRef<ISceneOutliner> SceneOutliner = SceneOutlinerModule.CreateSceneOutliner( InitOptions, FOnActorPicked() /* Not used for outliner when in browsing mode */ );
			TSharedRef<SWidget> WidgetToDraw =
				SNew( SDPIScaler )
				.DPIScale( VREd::EditorUIScale->GetFloat() )
				[
					SceneOutliner
				]
			;

			const bool bWithSceneComponent = false;
			AVREditorFloatingUI* WorldOutlinerUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >( TEXT( "WorldOutlinerUI" ), bWithSceneComponent );
			WorldOutlinerUI->SetSlateWidget( *this, WidgetToDraw, DefaultResolution, VREd::EditorUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Nothing );
			WorldOutlinerUI->ShowUI( false );
			FloatingUIs.Add( WorldOutlinerUI );

			EditorUIPanels[ (int32)EEditorUIPanel::WorldOutliner ] = WorldOutlinerUI;
		}

		{
			const TSharedRef< ILevelEditor >& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor" ).GetFirstLevelEditor().ToSharedRef();

			const FName TabIdentifier = NAME_None;	// No tab for us!
			TSharedRef<SWidget> ActorDetails = LevelEditor->CreateActorDetails( TabIdentifier );

			TSharedRef<SWidget> WidgetToDraw =
				SNew( SDPIScaler )
				.DPIScale( VREd::EditorUIScale->GetFloat() )
				[
					ActorDetails
				]
			;

			const bool bWithSceneComponent = false;
			AVREditorFloatingUI* ActorDetailsUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >( TEXT( "ActorDetailsUI" ), bWithSceneComponent );
			ActorDetailsUI->SetSlateWidget( *this, WidgetToDraw, DefaultResolution, VREd::EditorUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Nothing );
			ActorDetailsUI->ShowUI( false );
			FloatingUIs.Add( ActorDetailsUI );

			EditorUIPanels[ (int32)EEditorUIPanel::ActorDetails ] = ActorDetailsUI;
		}

		{
			const TSharedRef< ILevelEditor >& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor" ).GetFirstLevelEditor().ToSharedRef();

			TSharedRef<SWidget> Modes = LevelEditor->CreateToolBox();

			TSharedRef<SWidget> WidgetToDraw =
				SNew( SDPIScaler )
				.DPIScale( VREd::EditorUIScale->GetFloat() )
				[
					Modes
				]
			;

			const bool bWithSceneComponent = false;
			AVREditorFloatingUI* ModesUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >( TEXT( "ModesUI" ), bWithSceneComponent );
			ModesUI->SetSlateWidget( *this, WidgetToDraw, DefaultResolution, VREd::EditorUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Nothing );
			ModesUI->ShowUI( false );
			FloatingUIs.Add( ModesUI );

			// @todo vreditor placement: This is required to force the modes UI to refresh -- otherwise it looks empty
			GLevelEditorModeTools().ActivateMode( FBuiltinEditorModes::EM_Placement );

			EditorUIPanels[ (int32)EEditorUIPanel::Modes ] = ModesUI;			
		}

		// Create the tutorial dockable window
		{
			const bool bShowAtStart = 
				!bIsVREditorDemo && 
				GetOwner().IsActuallyUsingVR();	// Don't show tutorial at startup when in "forced VR" mode, because you can't interact with UI using the mouse currently

			AVREditorFloatingUI* TutorialUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >( TEXT( "TutorialUI" ), false );
			TutorialUI->SetUMGWidget( *this, TutorialWidgetClass, FIntPoint( VREd::TutorialUIResolutionX->GetFloat(), VREd::TutorialUIResolutionY->GetFloat() ), VREd::TutorialUISize->GetFloat(), bShowAtStart ? AVREditorFloatingUI::EDockedTo::Room : AVREditorFloatingUI::EDockedTo::Nothing );
			if( bShowAtStart )
			{
				TutorialUI->SetRelativeOffset( FVector( VREd::TutorialUILocationX->GetFloat(), VREd::TutorialUILocationY->GetFloat(), VREd::TutorialUILocationZ->GetFloat() ) );
				TutorialUI->SetLocalRotation( FRotator( VREd::TutorialUIPitch->GetFloat(), VREd::TutorialUIYaw->GetFloat(), 0 ) );
			}
			
			TutorialUI->ShowUI( bShowAtStart );

			FloatingUIs.Add( TutorialUI );

			EditorUIPanels[ (int32)EEditorUIPanel::Tutorial ] = TutorialUI;		
		}

		{
			const FIntPoint Resolution(VREd::AssetEditorUIResolutionX->GetInt(), VREd::AssetEditorUIResolutionY->GetInt());

			const bool bWithSceneComponent = false;
			AVREditorDockableWindow* TabManagerUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >( TEXT( "AssetEditor" ), bWithSceneComponent );
			TabManagerUI->SetSlateWidget( *this, SNullWidget::NullWidget, Resolution, VREd::EditorUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Nothing );
			TabManagerUI->ShowUI( false );

			// @todo vreditor: Could use "Hovering" instead for better performance with many UIs, but needs to be manually refreshed in some cases
			TabManagerUI->GetWidgetComponent()->SetDrawingPolicy( EVREditorWidgetDrawingPolicy::Always );

			FloatingUIs.Add( TabManagerUI );

			EditorUIPanels[ (int32)EEditorUIPanel::AssetEditor ] = TabManagerUI;

			TSharedPtr<SWindow> TabManagerWindow = TabManagerUI->GetWidgetComponent()->GetSlateWindow();
			TSharedRef<SWindow> TabManagerWindowRef = TabManagerWindow.ToSharedRef();
			ProxyTabManager = MakeShareable( new FProxyTabmanager( TabManagerWindowRef ) );

			ProxyTabManager->OnTabOpened.AddUObject( this, &UVREditorUISystem::OnProxyTabLaunched );
			ProxyTabManager->OnAttentionDrawnToTab.AddUObject( this, &UVREditorUISystem::OnAttentionDrawnToTab );

			// We're going to start stealing tabs from the global tab manager inserting them into the world instead.
			FGlobalTabmanager::Get()->SetProxyTabManager( ProxyTabManager );

			FAssetEditorManager::Get().OnAssetEditorOpened().AddUObject( this, &UVREditorUISystem::OnAssetEditorOpened );
		}
		// create the world settings menu
		{
			FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked< FPropertyEditorModule >( "PropertyEditor" );

			FDetailsViewArgs DetailsViewArgs( false, false, true, FDetailsViewArgs::HideNameArea, false, GUnrealEd );
			DetailsViewArgs.bShowActorLabel = false;

			TSharedRef<IDetailsView> WorldSettings = PropertyEditorModule.CreateDetailView( DetailsViewArgs );

			UWorld* World = GetOwner().GetWorld();
			if ( World != NULL )
			{
				WorldSettings->SetObject( World->GetWorldSettings() );
			}

			TSharedRef<SWidget> WidgetToDraw = 
				SNew( SDPIScaler )
				.DPIScale( VREd::EditorUIScale->GetFloat() )
				[
					WorldSettings
				]
			;

			const bool bWithSceneComponent = false;
			AVREditorFloatingUI* WorldSettingsUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >( TEXT( "WorldSettingsUI" ), bWithSceneComponent );
			WorldSettingsUI->SetSlateWidget( *this, WidgetToDraw, DefaultResolution, VREd::EditorUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Nothing );
			WorldSettingsUI->ShowUI( false );
			FloatingUIs.Add( WorldSettingsUI );

			EditorUIPanels[ (int32)EEditorUIPanel::WorldSettings ] = WorldSettingsUI;
		}
	}
}

void UVREditorUISystem::CleanUpActorsBeforeMapChangeOrSimulate()
{
	for( AVREditorFloatingUI* FloatingUIPtr : FloatingUIs )
	{
		if( FloatingUIPtr != nullptr )
		{
			FloatingUIPtr->Destroy( false, false );
			FloatingUIPtr = nullptr;
		}
	}

	FloatingUIs.Reset();
	EditorUIPanels.Reset();
	QuickRadialMenu = nullptr;
	QuickMenuUI = nullptr;

	ProxyTabManager.Reset();

	// Remove the proxy tab manager, we don't want to steal tabs any more.
	FGlobalTabmanager::Get()->SetProxyTabManager(TSharedPtr<FProxyTabmanager>());
	FAssetEditorManager::Get().OnAssetEditorOpened().RemoveAll(this);
}

void UVREditorUISystem::OnAssetEditorOpened(UObject* Asset)
{
	// We need to disable drag drop on the tabs spawned in VR mode.
	TArray<IAssetEditorInstance*> Editors = FAssetEditorManager::Get().FindEditorsForAsset(Asset);
	for ( IAssetEditorInstance* Editor : Editors )
	{
		Editor->GetAssociatedTabManager()->SetCanDoDragOperation(false);
	}
}

bool UVREditorUISystem::IsWidgetAnEditorUIWidget( const UActorComponent* WidgetComponent ) const
{
	if( WidgetComponent != nullptr && WidgetComponent->IsA( UWidgetComponent::StaticClass() ) )
	{
		for( AVREditorFloatingUI* FloatingUIPtr : FloatingUIs )
		{
			if( FloatingUIPtr != nullptr )
			{
				if( WidgetComponent == FloatingUIPtr->GetWidgetComponent() )
				{
					return true;
				}
			}
		}
	}

	return false;
}


bool UVREditorUISystem::IsShowingEditorUIPanel( const EEditorUIPanel EditorUIPanel ) const
{
	AVREditorFloatingUI* Panel = EditorUIPanels[ (int32)EditorUIPanel ];
	if( Panel != nullptr )
	{
		return Panel->IsUIVisible();
	}

	return false;
}


void UVREditorUISystem::ShowEditorUIPanel( const UWidgetComponent* WidgetComponent, UVREditorInteractor* Interactor, const bool bShouldShow, const bool OnHand, const bool bRefreshQuickMenu, const bool bPlaySound )
{
	AVREditorFloatingUI* Panel = nullptr;
	for( AVREditorFloatingUI* CurrentPanel : EditorUIPanels )
	{
		if( CurrentPanel && CurrentPanel->GetWidgetComponent() == WidgetComponent )
		{
			Panel = CurrentPanel;
			break;
		}
	}

	ShowEditorUIPanel( Panel, Interactor, bShouldShow, OnHand, bRefreshQuickMenu, bPlaySound );
}


void UVREditorUISystem::ShowEditorUIPanel( const EEditorUIPanel EditorUIPanel, UVREditorInteractor* Interactor, const bool bShouldShow, const bool OnHand, const bool bRefreshQuickMenu, const bool bPlaySound )
{
	AVREditorFloatingUI* Panel = EditorUIPanels[ (int32)EditorUIPanel ];
	ShowEditorUIPanel( Panel, Interactor, bShouldShow, OnHand, bRefreshQuickMenu, bPlaySound );
}

void UVREditorUISystem::ShowEditorUIPanel( AVREditorFloatingUI* Panel, UVREditorInteractor* Interactor, const bool bShouldShow, const bool OnHand, const bool bRefreshQuickMenu, const bool bPlaySound )
{
	if( Panel != nullptr )
	{
		AVREditorFloatingUI::EDockedTo DockedTo = Panel->GetDockedTo();
		if( OnHand || DockedTo == AVREditorFloatingUI::EDockedTo::Nothing )
		{
			// Hide any panels that are already shown on this hand
			for( int32 PanelIndex = 0; PanelIndex < (int32)EEditorUIPanel::TotalCount; ++PanelIndex )
			{
				AVREditorFloatingUI* OtherPanel = EditorUIPanels[PanelIndex];
				if( OtherPanel != nullptr && OtherPanel->IsUIVisible() && OtherPanel->GetDockedTo() != AVREditorFloatingUI::EDockedTo::Room )
				{
					UVREditorInteractor* OtherPanelDockToVREditorInteractor = GetOwner().GetHandInteractor( OtherPanel->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand ? EControllerHand::Left : EControllerHand::Right );
					if( OtherPanelDockToVREditorInteractor == Interactor )
					{
						OtherPanel->ShowUI( false );
						OtherPanel->SetDockedTo( AVREditorFloatingUI::EDockedTo::Nothing );
						OtherPanelDockToVREditorInteractor->SetHasUIInFront( false );
					}
				}
			}
			
			UVREditorMotionControllerInteractor* MotionControllerInteractor = Cast<UVREditorMotionControllerInteractor>( Interactor );
			if ( MotionControllerInteractor )
			{
				const AVREditorFloatingUI::EDockedTo NewDockedTo = MotionControllerInteractor->GetControllerSide() == EControllerHand::Left ? AVREditorFloatingUI::EDockedTo::LeftHand : AVREditorFloatingUI::EDockedTo::RightHand;
				Panel->SetDockedTo( NewDockedTo );
			}
			
			if ( bShouldShow )
			{
				Panel->SetScale( Panel->GetInitialScale() );
			}

			const FVector EditorUIRelativeOffset( Panel->GetSize().Y * 0.5f + VREd::EditorUIOffsetFromHand->GetFloat(), 0.0f, 0.0f );
			Panel->SetRelativeOffset( EditorUIRelativeOffset );
			Panel->SetLocalRotation( FRotator( 90.0f, 180.0f, 0.0f ) ); // @todo vreditor: needs initial rotation
		}

		Panel->ShowUI( bShouldShow );

		if ( Panel->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand || Panel->GetDockedTo() == AVREditorFloatingUI::EDockedTo::RightHand )
		{
			UVREditorInteractor* PanelDockedToVREditorInteractor = GetOwner().GetHandInteractor( Panel->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand ? EControllerHand::Left : EControllerHand::Right );
			PanelDockedToVREditorInteractor->SetHasUIInFront( bShouldShow );
		}

		if( bPlaySound )
		{
			UGameplayStatics::PlaySound2D( GetOwner().GetWorld(), bShouldShow ? ShowUISound : HideUISound );
		}

		if ( bRefreshQuickMenu && QuickMenuUI )
		{
			QuickMenuUI->GetUserWidget<UVREditorQuickMenu>()->RefreshUI();
		}
	}
}


bool UVREditorUISystem::IsShowingRadialMenu( UVREditorInteractor* Interactor ) const
{
	const EControllerHand DockedToHand = QuickRadialMenu->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand ? EControllerHand::Left : EControllerHand::Right;
	const UVREditorInteractor* DockedToInteractor = GetOwner().GetHandInteractor( DockedToHand );
	return DockedToInteractor == Interactor && !QuickRadialMenu->bHidden;
}


void UVREditorUISystem::UpdateRadialMenu( UVREditorInteractor* Interactor )
{
	if( QuickRadialMenu->bHidden )
	{
		QuickRadialMenu->ShowUI( true, false );
	}

	if( !QuickRadialMenu->bHidden )
	{
		RadialMenuHideDelayTime = 0.0f;
		QuickRadialMenu->GetUserWidget<UVREditorRadialMenu>()->Update( Interactor );
	}
}


void UVREditorUISystem::TryToSpawnRadialMenu( UVREditorInteractor* Interactor )
{
	UVREditorMotionControllerInteractor* MotionControllerInteractor = Cast<UVREditorMotionControllerInteractor>( Interactor );
	if( MotionControllerInteractor )
	{
		UVREditorInteractor* DockedToInteractor = nullptr;
		if ( !QuickRadialMenu->bHidden )
		{
			const EControllerHand ControllerHand = QuickRadialMenu->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand ? EControllerHand::Left : EControllerHand::Right;
			DockedToInteractor = GetOwner().GetHandInteractor( ControllerHand );
		}

		EViewportInteractionDraggingMode DraggingMode = Interactor->GetDraggingMode();

		bool bNeedsSpawn =
			( QuickRadialMenu->bHidden || DockedToInteractor != Interactor ) &&
			DraggingMode != EViewportInteractionDraggingMode::ActorsAtLaserImpact &&	// Don't show radial menu if the hand is busy dragging something around
			DraggingMode != EViewportInteractionDraggingMode::ActorsFreely &&
			DraggingMode != EViewportInteractionDraggingMode::ActorsWithGizmo &&
			DraggingMode != EViewportInteractionDraggingMode::AssistingDrag &&
			( InteractorDraggingUI == nullptr || InteractorDraggingUI != Interactor ) &&
			!Interactor->IsHoveringOverUI();	// Don't show radial menu when aiming at a UI  (too much clutter)

		UVREditorRadialMenu* RadialMenu = QuickRadialMenu->GetUserWidget<UVREditorRadialMenu>();
		// We need to update the trackpad position in the radialmenu before checking if it can be used
		RadialMenu->Update( Interactor );
		if ( RadialMenu && RadialMenu->IsInMenuRadius() )
		{
			bNeedsSpawn = false;
		}

		if ( bNeedsSpawn )
		{
			const AVREditorFloatingUI::EDockedTo DockedTo = MotionControllerInteractor->GetControllerSide() == EControllerHand::Left ? AVREditorFloatingUI::EDockedTo::LeftHand : AVREditorFloatingUI::EDockedTo::RightHand;
			QuickRadialMenu->SetDockedTo( DockedTo );
			QuickRadialMenu->ShowUI( true );
		}
	}
}


void UVREditorUISystem::HideRadialMenu( UVREditorInteractor* Interactor )
{
	// Only hide the radial menu if the passed interactor is actually the interactor with the radial menu
	if( IsShowingRadialMenu( Interactor ) )
	{
		QuickRadialMenu->ShowUI( false, true, VREd::RadialMenuFadeDelay->GetFloat() );
	}
}


FTransform UVREditorUISystem::MakeDockableUITransformOnLaser( AVREditorDockableWindow* InitDraggingDockUI, UVREditorInteractor* Interactor, const float DockSelectDistance ) const
{
	FTransform HandTransform;
	FVector HandForward;
	Interactor->GetTransformAndForwardVector( HandTransform, HandForward );
	const FTransform InteractorTransform = Interactor->GetTransform();

	const FVector NewLocation = InteractorTransform.GetLocation() + ( HandForward.GetSafeNormal() * DockSelectDistance );
	FRotator NewRotation = ( InteractorTransform.GetLocation() - NewLocation ).ToOrientationRotator();
	NewRotation.Roll = -InteractorTransform.GetRotation().Rotator().Roll;

	const FTransform LaserImpactToWorld( NewRotation, NewLocation );
	return LaserImpactToWorld;
}


FTransform UVREditorUISystem::MakeDockableUITransform( AVREditorDockableWindow* InitDraggingDockUI, UVREditorInteractor* Interactor, const float DockSelectDistance )
{
	const FTransform UIOnLaserToWorld = MakeDockableUITransformOnLaser( DraggingUI, Interactor, DockSelectDistance );
	const FTransform UIToUIOnLaser = DraggingUIOffsetTransform;
	
	const FTransform UIToWorld = UIToUIOnLaser * UIOnLaserToWorld;

	if( DraggingUI != nullptr &&  DraggingUI->GetDockedTo() == AVREditorFloatingUI::EDockedTo::Dragging && InteractorDraggingUI && Interactor == InteractorDraggingUI )
	{
		UVREditorMotionControllerInteractor* OtherInteractor = Cast<UVREditorMotionControllerInteractor>( InteractorDraggingUI->GetOtherInteractor() );
		if ( OtherInteractor )
		{
			const float WorldScaleFactor = GetOwner().GetWorldScaleFactor();
			const FVector DraggingUILocation = InteractorDraggingUI->GetHoverLocation();
			const FVector OtherHandLocation = OtherInteractor->GetTransform().GetLocation();
			LastDraggingHoverLocation = UIOnLaserToWorld.GetLocation();

			const float Distance = FVector::Dist( UIOnLaserToWorld.GetLocation(), OtherHandLocation ) / WorldScaleFactor;

			// if dragged passed the threshold since starting dragging
			if ( Distance > VREd::MinDockDragDistance->GetFloat() && !bDraggedDockFromHandPassedThreshold &&
				( DraggingUI->GetPreviouslyDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand || DraggingUI->GetPreviouslyDockedTo() == AVREditorFloatingUI::EDockedTo::RightHand ) )
			{
				bDraggedDockFromHandPassedThreshold = true;
			}

			// Snapping to a hand when in range
			if ( Distance <= VREd::MinDockDragDistance->GetFloat() && ( bDraggedDockFromHandPassedThreshold || DraggingUI->GetPreviouslyDockedTo() == AVREditorFloatingUI::EDockedTo::Room ) )
			{
				const FVector EditorUIRelativeOffset( DraggingUI->GetSize().Y * 0.5f + VREd::EditorUIOffsetFromHand->GetFloat(), 0.0f, 0.0f );
				const FTransform MovingUIGoalTransform = DraggingUI->MakeUITransformLockedToHand( OtherInteractor, false, EditorUIRelativeOffset, FRotator( 90.0f, 180.0f, 0.0f ) );
				const AVREditorFloatingUI::EDockedTo NewDockedTo = OtherInteractor->GetControllerSide() == EControllerHand::Left ? AVREditorFloatingUI::EDockedTo::LeftHand : AVREditorFloatingUI::EDockedTo::RightHand;
				DraggingUI->MoveTo( MovingUIGoalTransform, VREd::DockDragSpeed->GetFloat(), NewDockedTo );
			}
			else
			{
				DraggingUI->MoveTo( UIToWorld, VREd::DockDragSpeed->GetFloat(), AVREditorFloatingUI::EDockedTo::Room );
			}
		}
	}

	return UIToWorld;
}


void UVREditorUISystem::StartDraggingDockUI( AVREditorDockableWindow* InitDraggingDockUI, UVREditorInteractor* Interactor, const float DockSelectDistance )
{
	AVREditorFloatingUI::EDockedTo DockTo = InitDraggingDockUI->GetDockedTo();
	if( DockTo == AVREditorFloatingUI::EDockedTo::LeftHand || DockTo == AVREditorFloatingUI::EDockedTo::RightHand )
	{
		bDraggedDockFromHandPassedThreshold = false;

		// Set the laser of the interactor the dockable window was pulled from
		UViewportInteractor* OtherInteractor = Interactor->GetOtherInteractor();
		if (OtherInteractor)
		{
			UVREditorInteractor* OtherVREditorInteractor = Cast<UVREditorInteractor>(OtherInteractor);
			if (OtherVREditorInteractor)
			{
				OtherVREditorInteractor->SetHasUIInFront(false);
			}
		}
	}

	

	InteractorDraggingUI = Interactor;

	FTransform UIToWorld = InitDraggingDockUI->GetActorTransform();
	UIToWorld.SetScale3D( FVector( 1.0f ) );
	const FTransform WorldToUI = UIToWorld.Inverse();

	const FTransform UIOnLaserToWorld = MakeDockableUITransformOnLaser( InitDraggingDockUI, Interactor, DockSelectDistance );
	const FTransform UIOnLaserToUI = UIOnLaserToWorld * WorldToUI;
	const FTransform UIToUIOnLaser = UIOnLaserToUI.Inverse();
	DraggingUIOffsetTransform = UIToUIOnLaser;

	DraggingUI = InitDraggingDockUI;
	DraggingUI->SetDockedTo( AVREditorFloatingUI::EDockedTo::Dragging );

	UGameplayStatics::PlaySound2D( GetOwner().GetWorld(), StartDragUISound );
}

AVREditorDockableWindow* UVREditorUISystem::GetDraggingDockUI() const
{
	return DraggingUI;
}

void UVREditorUISystem::StopDraggingDockUI( UVREditorInteractor* VREditorInteractor )
{
	if ( IsInteractorDraggingDockUI( VREditorInteractor ) )
	{
		// Put the Dock back on the hand it came from or leave it where it is in the room
		UViewportInteractor* OtherInteractor = VREditorInteractor->GetOtherInteractor();
		if ( OtherInteractor )
		{
			UVREditorMotionControllerInteractor* MotionControllerOtherInteractor = Cast<UVREditorMotionControllerInteractor>( OtherInteractor );
			if ( MotionControllerOtherInteractor )
			{
				bool bOnHand = false;
				const float Distance = FVector::Dist( LastDraggingHoverLocation, MotionControllerOtherInteractor->GetTransform().GetLocation() ) / GetOwner().GetWorldScaleFactor();
				if ( Distance > VREd::MinDockDragDistance->GetFloat() )
				{
					DraggingUI->SetDockedTo( AVREditorFloatingUI::EDockedTo::Room );
				}
				else
				{
					bOnHand = true;
					const FVector EditorUIRelativeOffset( DraggingUI->GetSize().Y * 0.5f + VREd::EditorUIOffsetFromHand->GetFloat(), 0.0f, 0.0f );
					const FTransform MovingUIGoalTransform = DraggingUI->MakeUITransformLockedToHand( MotionControllerOtherInteractor, false, EditorUIRelativeOffset, FRotator( 90.0f, 180.0f, 0.0f ) );
					const AVREditorFloatingUI::EDockedTo NewDockedTo = MotionControllerOtherInteractor->GetControllerSide() == EControllerHand::Left ? AVREditorFloatingUI::EDockedTo::LeftHand : AVREditorFloatingUI::EDockedTo::RightHand;
					DraggingUI->MoveTo( MovingUIGoalTransform, VREd::DockSnapTime->GetFloat(), NewDockedTo );
				}

				ShowEditorUIPanel( DraggingUI, MotionControllerOtherInteractor, true, bOnHand );

				DraggingUI = nullptr;
				InteractorDraggingUI->SetDraggingMode( EViewportInteractionDraggingMode::Nothing );
				InteractorDraggingUI = nullptr;

				UGameplayStatics::PlaySound2D( GetOwner().GetWorld(), StopDragUISound );
			}
		}
	}
}

bool UVREditorUISystem::IsDraggingDockUI()
{
	return DraggingUI != nullptr && DraggingUI->GetDockedTo() == AVREditorFloatingUI::EDockedTo::Dragging;
}

bool UVREditorUISystem::IsInteractorDraggingDockUI( const UVREditorInteractor* Interactor ) const
{
	return InteractorDraggingUI && InteractorDraggingUI == Interactor;
}

void UVREditorUISystem::TogglePanelsVisibility()
{
	bool bAnyPanelsVisible = false;
	AVREditorFloatingUI* PanelOnHand = nullptr;

	// Check if there is any panel visible and if any is docked to a hand
	for (AVREditorFloatingUI* Panel : EditorUIPanels)
	{
		if (Panel && Panel->IsUIVisible())
		{
			bAnyPanelsVisible = true;

			const AVREditorFloatingUI::EDockedTo DockedTo = Panel->GetDockedTo();
			if (DockedTo == AVREditorFloatingUI::EDockedTo::LeftHand || DockedTo == AVREditorFloatingUI::EDockedTo::RightHand)
			{
				PanelOnHand = Panel;
				break;
			}
		}
	}

	// Hide if there is any UI visible
	const bool bShowUI = !bAnyPanelsVisible;
	
	UGameplayStatics::PlaySound2D( GetOwner().GetWorld(), bShowUI ? ShowUISound : HideUISound );

	if ( bSetDefaultLayout)
	{
		bSetDefaultLayout = false;
		SetDefaultWindowLayout();
	}
	else
	{
		for ( AVREditorFloatingUI* Panel : EditorUIPanels )
		{
			if ( Panel != nullptr && Panel->IsUIVisible() != bShowUI )
			{
				if ( bShowUI )
				{
					bool SetNewVisibility = true;
					const AVREditorFloatingUI::EDockedTo DockedTo = Panel->GetDockedTo();
					if ( DockedTo == AVREditorFloatingUI::EDockedTo::LeftHand || DockedTo == AVREditorFloatingUI::EDockedTo::RightHand )
					{
						if ( !PanelOnHand )
						{
							UVREditorInteractor* VREditorInteractor = GetOwner().GetHandInteractor( Panel->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand ? EControllerHand::Left : EControllerHand::Right );
							VREditorInteractor->SetHasUIInFront( true );
							PanelOnHand = Panel;
						}
						else
						{
							SetNewVisibility = false;
						}
					}
					else if ( DockedTo == AVREditorFloatingUI::EDockedTo::Nothing )
					{
						SetNewVisibility = false;
					}

					if ( SetNewVisibility )
					{
						Panel->ShowUI( true );
					}
				}
				else
				{
					Panel->ShowUI( false );
				}
			}
		}
	}

	if( !bShowUI && PanelOnHand )
	{
		UVREditorInteractor* VREditorInteractor = GetOwner().GetHandInteractor( PanelOnHand->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand ? EControllerHand::Left : EControllerHand::Right );
		VREditorInteractor->SetHasUIInFront( false );
	}

	if (QuickMenuUI)
	{
		QuickMenuUI->GetUserWidget<UVREditorQuickMenu>()->RefreshUI();
	}
}

float UVREditorUISystem::GetMaxDockWindowSize() const
{
	return VREd::MaxDockWindowSize->GetFloat();
}

float UVREditorUISystem::GetMinDockWindowSize() const
{
	return VREd::MinDockWindowSize->GetFloat();
}

void UVREditorUISystem::OnProxyTabLaunched(TSharedPtr<SDockTab> NewTab)
{
	ShowAssetEditor();
}

void UVREditorUISystem::OnAttentionDrawnToTab(TSharedPtr<SDockTab> NewTab)
{
	// @todo vreditor: When clicking on Modes icons in the Modes panel, the LevelEditor tab is invoked which causes an empty asset editor to pop-up
	static FName LevelEditorTabID( "LevelEditor" );
	if( NewTab->GetLayoutIdentifier().TabType != LevelEditorTabID )
	{
		ShowAssetEditor();
	}
}

void UVREditorUISystem::ShowAssetEditor()
{
	bRefocusViewport = true;

	// A tab was opened, so make sure the "Asset" UI is visible.  That's where the user can interact
	// with the newly-opened tab
	if ( !IsShowingEditorUIPanel(EEditorUIPanel::AssetEditor) )
	{
		// Always spawn on a hand.  But which hand?  Well, we'll choose the hand that isn't actively clicking on something using a laser.
		UVREditorInteractor* VREditorInteractor = GetOwner().GetHandInteractor( EControllerHand::Left ); // Hand that did not clicked with a laser
		//@todo VREditor: This should include a check on VREditorInteractor->IsClickingOnUI(), however that is broken because of double click on UI.
		if( !VREditorInteractor->HasUIInFront() )
		{
			VREditorInteractor = GetOwner().GetHandInteractor( EControllerHand::Right );
		}
		
		const bool bShouldShow = true;
		const bool bShowOnHand = true;
		ShowEditorUIPanel( EEditorUIPanel::AssetEditor, VREditorInteractor, bShouldShow, bShowOnHand );

		// Play haptic effect so user knows to look at their hand that now has UI on it!
		VREditorInteractor->PlayHapticEffect( VREd::UIAssetEditorSummonedOnHandHapticFeedbackStrength->GetFloat() );
	}
}

void UVREditorUISystem::TogglePanelVisibility( const EEditorUIPanel EditorUIPanel )
{
	AVREditorFloatingUI* Panel = EditorUIPanels[(int32)EditorUIPanel];
	if (Panel != nullptr)
	{
		const bool bIsShowing = Panel->IsUIVisible();
		UVREditorInteractor* InteractorWithQuickMenu = GetOwner().GetHandInteractor( QuickMenuUI->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftArm ? EControllerHand::Left : EControllerHand::Right );
		if( Panel->GetDockedTo() == AVREditorFloatingUI::EDockedTo::Room )
		{
			Panel->ShowUI( false );
			Panel->SetDockedTo( AVREditorFloatingUI::EDockedTo::Nothing );
		}
		else
		{
			ShowEditorUIPanel( Panel, InteractorWithQuickMenu, !bIsShowing, true );
		}
	}
}

void UVREditorUISystem::SetDefaultWindowLayout()
{
	for ( int32 PanelIndex = 0; PanelIndex < DefaultWindowTransforms.Num(); ++PanelIndex )
	{
		AVREditorFloatingUI* Panel = EditorUIPanels[PanelIndex];
		if ( Panel )
		{
			const AVREditorFloatingUI::EDockedTo DockedTo = Panel->GetDockedTo();
			if (DockedTo == AVREditorFloatingUI::EDockedTo::LeftHand || DockedTo == AVREditorFloatingUI::EDockedTo::RightHand)
			{
				const EControllerHand ControllerHand = DockedTo == AVREditorFloatingUI::EDockedTo::LeftHand ? EControllerHand::Left : EControllerHand::Right;
				UVREditorInteractor* Interactor = GetOwner().GetHandInteractor( ControllerHand );
				Interactor->SetHasUIInFront( false );
			}

			Panel->SetDockedTo( AVREditorFloatingUI::EDockedTo::Room );
			Panel->ShowUI( true );

			// Make sure the UIs are centered around the direction your head is looking (yaw only!)
			const FVector RoomSpaceHeadLocation = GetOwner().GetRoomSpaceHeadTransform().GetLocation() / GetOwner().GetWorldScaleFactor();
			FRotator RoomSpaceHeadYawRotator = GetOwner().GetRoomSpaceHeadTransform().GetRotation().Rotator();
			RoomSpaceHeadYawRotator.Pitch = 0.0f;
			RoomSpaceHeadYawRotator.Roll = 0.0f;

			FTransform NewTransform = DefaultWindowTransforms[PanelIndex];
			NewTransform *= FTransform( RoomSpaceHeadYawRotator.Quaternion(), FVector::ZeroVector );
			Panel->SetLocalRotation( NewTransform.GetRotation().Rotator() );
			Panel->SetRelativeOffset( RoomSpaceHeadLocation + NewTransform.GetTranslation() );
		}
	}
}

void UVREditorUISystem::CreateVRColorPicker(const TSharedRef<SColorPicker>& ColorPicker)
{
	DestroyVRColorPicker();

	// Check that the Color Picker Panel isn't currently showing. Also handles the EEditorUIPanel::ColorPicker being null.
	if (!IsShowingEditorUIPanel(EEditorUIPanel::ColorPicker))
	{
		const FIntPoint DefaultResolution(VREd::DefaultEditorUIResolutionX->GetInt(), VREd::DefaultEditorUIResolutionY->GetInt());

		TSharedRef<SWidget> WidgetToDraw =
			SNew(SDPIScaler)
			.DPIScale(VREd::EditorUIScale->GetFloat())
			[
				ColorPicker
			]
		;

		const bool bWithSceneComponent = false;
		AVREditorFloatingUI* ColorPickerUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >(TEXT("ColorPickerUI"), bWithSceneComponent);
		ColorPickerUI->ShowUI(true);
		FloatingUIs.Add(ColorPickerUI);

		EditorUIPanels[(int32)EEditorUIPanel::ColorPicker] = ColorPickerUI;

		// Always spawn based on the location of the menu you are hovering over. To get this information, find the hand hovering over the UI.
		UVREditorInteractor* VREditorInteractor = GetOwner().GetHandInteractor(EControllerHand::Left);
		if (!VREditorInteractor->IsHoveringOverUI())
		{
			VREditorInteractor = GetOwner().GetHandInteractor(EControllerHand::Right);

		}

		ColorPickerUI->SetSlateWidget(*this, WidgetToDraw, DefaultResolution, VREd::EditorUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Room);

		// Make sure the UIs are centered around the direction your head is looking (yaw only!)
		const FVector RoomSpaceHeadLocation = GetOwner().GetRoomSpaceHeadTransform().GetLocation() / GetOwner().GetWorldScaleFactor();
		FRotator RoomSpaceHeadYawRotator = GetOwner().GetRoomSpaceHeadTransform().GetRotation().Rotator();
		RoomSpaceHeadYawRotator.Pitch = 0.0f;
		RoomSpaceHeadYawRotator.Roll = 0.0f;

	
		FTransform NewTransform = VREd::DefaultColorPickerTransform;
		NewTransform *= FTransform(RoomSpaceHeadYawRotator.Quaternion(), FVector::ZeroVector);
		ColorPickerUI->SetLocalRotation(NewTransform.GetRotation().Rotator());
		ColorPickerUI->SetRelativeOffset(RoomSpaceHeadLocation + NewTransform.GetTranslation());

		const bool bShouldShow = true;
		const bool bShowOnHand = false;
		ShowEditorUIPanel(EEditorUIPanel::ColorPicker, VREditorInteractor, bShouldShow, bShowOnHand);
	}

}

void UVREditorUISystem::DestroyVRColorPicker()
{

	UVREditorInteractor* VREditorInteractor = nullptr;
	const bool bShouldShow = false;
	const bool bShowOnHand = false;
	const bool bRefreshQuickMenu = true;
	ShowEditorUIPanel(EEditorUIPanel::ColorPicker, VREditorInteractor, bShouldShow, bShowOnHand, bRefreshQuickMenu);
}
