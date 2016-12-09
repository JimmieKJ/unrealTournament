// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "VREditorTeleporter.h"
#include "HAL/IConsoleManager.h"
#include "Engine/EngineTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "VREditorMode.h"
#include "ViewportInteractionTypes.h"
#include "ViewportWorldInteraction.h"
#include "VREditorInteractor.h"


namespace VREd
{
	static FAutoConsoleVariable TeleportLerpTime( TEXT( "VREd.TeleportLerpTime" ), 0.1f, TEXT( "The lerp time to teleport" ) );
	static FAutoConsoleVariable TeleportOffset( TEXT( "VREd.TeleportOffset" ), 100.0f, TEXT( "The offset from the hitresult towards the controller" ) );
	static FAutoConsoleVariable TeleportLaserPointerLength( TEXT( "VREd.TeleportLaserPointerLength" ), 500000.0f, TEXT( "Distance of the LaserPointer for teleporting" ) );
	static FAutoConsoleVariable TeleportDistance( TEXT( "VREd.TeleportDistance" ), 500.0f, TEXT( "Default distance for teleporting when not hitting anything" ) );
}

UVREditorTeleporter::UVREditorTeleporter( const FObjectInitializer& Initializer ) :
	Super( Initializer ),
	VRMode( nullptr ),
	bIsTeleporting( false ),
	TeleportLerpAlpha( 0 ),
	TeleportStartLocation( FVector::ZeroVector ),
	TeleportGoalLocation( FVector::ZeroVector ),
	bJustTeleported( false )
{
	// Load sounds
	TeleportSound = LoadObject<USoundCue>( nullptr, TEXT( "/Engine/VREditor/Sounds/VR_teleport_Cue" ) );
	check( TeleportSound != nullptr );
}

void UVREditorTeleporter::Init( UVREditorMode* InMode )
{
	VRMode = InMode;
	VRMode->OnTickHandle().AddUObject( this, &UVREditorTeleporter::Tick );
	VRMode->GetWorldInteraction().OnViewportInteractionInputAction().AddUObject( this, &UVREditorTeleporter::OnInteractorAction );
}

void UVREditorTeleporter::Shutdown()
{
	VRMode->OnTickHandle().RemoveAll( this );
	VRMode->GetWorldInteraction().OnViewportInteractionInputAction().RemoveAll( this );
	TeleportSound = nullptr;
	VRMode = nullptr;
}

void UVREditorTeleporter::Tick( const float DeltaTime )
{
	if ( bIsTeleporting )
	{
		Teleport( DeltaTime );
	}
}

void UVREditorTeleporter::StartTeleport( UViewportInteractor* Interactor )
{
	UVREditorInteractor* VREditorInteractor = Cast<UVREditorInteractor>( Interactor );
	if ( VREditorInteractor && !VREditorInteractor->IsHoveringOverUI() )
	{
		FVector LaserPointerStart, LaserPointerEnd;
		if ( Interactor->GetLaserPointer( LaserPointerStart, LaserPointerEnd ) )
		{
			FVector EndLocation;
			FHitResult HitResult = Interactor->GetHitResultFromLaserPointer( nullptr, true, nullptr, false, VREd::TeleportLaserPointerLength->GetFloat() );
			if( HitResult.bBlockingHit )
			{
				EndLocation = HitResult.Location;
			}
			else
			{
				FVector Direction = LaserPointerEnd - LaserPointerStart;
				Direction.Normalize();
				EndLocation = LaserPointerStart + ( Direction * ( VREd::TeleportDistance->GetFloat() * VRMode->GetWorldScaleFactor() ) );
			}

			const FVector HeadLocation = VRMode->GetHeadTransform().GetLocation();
			const FVector RoomLocation = VRMode->GetRoomTransform().GetLocation();

			FVector NewHandLocation = EndLocation;
			if( HitResult.bBlockingHit )
			{
				const FVector Offset = ( LaserPointerStart - EndLocation ).GetSafeNormal() *
					VREd::TeleportOffset->GetFloat() * VRMode->GetWorldInteraction().GetWorldScaleFactor(); // Offset from the hitresult, TeleportOffset is in centimeter
				NewHandLocation = EndLocation + Offset;
			}

			const FVector HandHeadOffset = HeadLocation - Interactor->GetTransform().GetLocation();		// The offset between the hand and the head
			const FVector NewHeadLocation = NewHandLocation + HandHeadOffset;							// The location in world space where the head is going to be
																										// The new roomspace location in world space
			const FVector HeadRoomOffset = HeadLocation - RoomLocation;									// Where the head is going to be in world space
			const FVector NewRoomSpaceLocation = NewHeadLocation - HeadRoomOffset;						// Result, where the roomspace is going to be in world space
																										// Set values to start teleporting
			bIsTeleporting = true;
			TeleportLerpAlpha = 0.0f;
			TeleportStartLocation = RoomLocation;
			TeleportGoalLocation = NewRoomSpaceLocation;

			UGameplayStatics::PlaySound2D( VRMode->GetWorld(), TeleportSound );
		}
	}
}

void UVREditorTeleporter::OnInteractorAction( class FEditorViewportClient& ViewportClient, UViewportInteractor* Interactor, const struct FViewportActionKeyInput& Action, bool& bOutIsInputCaptured, bool& bWasHandled )
{
	if ( ( Action.ActionType == ViewportWorldActionTypes::SelectAndMove ||
		Action.ActionType == ViewportWorldActionTypes::SelectAndMove_LightlyPressed ) )
	{
		if ( Action.Event == IE_Pressed )
		{
			if ( Action.ActionType == ViewportWorldActionTypes::SelectAndMove_LightlyPressed )
			{
				// Check to teleport before checking on actor selection, otherwise objects will be selected when teleporting
				if	( Interactor->GetDraggingMode() == EViewportInteractionDraggingMode::World &&
					( Interactor->GetOtherInteractor() == nullptr || Interactor->GetOtherInteractor()->GetDraggingMode() != EViewportInteractionDraggingMode::AssistingDrag ) )
				{
					StartTeleport( Interactor );
					bJustTeleported = true;
					bWasHandled = true;
				}
			}
			else if ( Action.ActionType == ViewportWorldActionTypes::SelectAndMove && bJustTeleported )
			{
				//Block the input
				bJustTeleported = false;
				bWasHandled = true;
			}

			// Nothing should happen when the hand has a UI in front, otherwise it will deselect everything
			if ( Interactor->GetIsLaserBlocked() )
			{
				bWasHandled = true;
			}
		}
	}
}

void UVREditorTeleporter::Teleport( const float DeltaTime )
{
	FTransform RoomTransform = VRMode->GetRoomTransform();
	TeleportLerpAlpha += DeltaTime;

	if ( TeleportLerpAlpha > VREd::TeleportLerpTime->GetFloat() )
	{
		// Teleporting is finished
		TeleportLerpAlpha = VREd::TeleportLerpTime->GetFloat();
		bIsTeleporting = false;
	}

	// Calculate the new position of the roomspace
	FVector NewLocation = FMath::Lerp( TeleportStartLocation, TeleportGoalLocation, TeleportLerpAlpha / VREd::TeleportLerpTime->GetFloat() );
	RoomTransform.SetLocation( NewLocation );
	VRMode->SetRoomTransform( RoomTransform );
}
