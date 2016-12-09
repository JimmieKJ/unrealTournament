// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "VREditorAutoScaler.h"
#include "VREditorMode.h"
#include "ViewportInteractionTypes.h"
#include "ViewportWorldInteraction.h"
#include "ViewportInteractor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/WorldSettings.h"
#include "Sound/SoundCue.h"

UVREditorAutoScaler::UVREditorAutoScaler( const FObjectInitializer& ObjectInitializer ) :
	Super( ObjectInitializer ),
	VRMode( nullptr )
{
	// Load sounds
	ScaleSound = LoadObject<USoundCue>( nullptr, TEXT( "/Engine/VREditor/Sounds/VR_teleport_Cue" ) );
	check( ScaleSound != nullptr );
}


void UVREditorAutoScaler::Init( class UVREditorMode* InVRMode )
{
	VRMode = InVRMode;
	VRMode->GetWorldInteraction().OnViewportInteractionInputAction().AddUObject( this, &UVREditorAutoScaler::OnInteractorAction );
}


void UVREditorAutoScaler::Shutdown()
{
	VRMode->GetWorldInteraction().OnViewportInteractionInputAction().RemoveAll( this );
	VRMode = nullptr;
}


void UVREditorAutoScaler::Scale( const float NewWorldToMetersScale )
{
	UViewportWorldInteraction* WorldInteraction = &VRMode->GetWorldInteraction();

	// Set the new world to meters scale.
	const float OldWorldToMetersScale = WorldInteraction->GetViewportWorld()->GetWorldSettings()->WorldToMeters;
	WorldInteraction->SetWorldToMetersScale( NewWorldToMetersScale );
	WorldInteraction->SkipInteractiveWorldMovementThisFrame();

	FTransform RoomTransform = WorldInteraction->GetRoomTransform();

	// Use the HMD location as pivot to scale of, this will cause the view to be unchanged. 
	const FVector RoomSpacePivotLocation = WorldInteraction->GetRoomSpaceHeadTransform().GetLocation();
	const FVector WorldSpacePivotLocation = WorldInteraction->GetHeadTransform().GetLocation();// *( OldWorldToMetersScale / 100.0f );
		
	//The rest of this calculations is a duplicate from UViewportWorldInteraction.
	const FVector NewRoomSpacePivotLocation = ( RoomSpacePivotLocation / OldWorldToMetersScale ) * NewWorldToMetersScale;
	const FVector NewWorldSpacePivotLocation = RoomTransform.TransformPosition( NewRoomSpacePivotLocation );
	const FVector WorldSpacePivotDelta = NewWorldSpacePivotLocation - WorldSpacePivotLocation;
	const FVector NewWorldSpaceRoomLocation = RoomTransform.GetLocation() - WorldSpacePivotDelta;
	RoomTransform.SetLocation( NewWorldSpaceRoomLocation );

	WorldInteraction->SetRoomTransform( RoomTransform );

	UGameplayStatics::PlaySound2D( VRMode->GetWorld(), ScaleSound );
}


void UVREditorAutoScaler::OnInteractorAction( class FEditorViewportClient& ViewportClient, UViewportInteractor* Interactor, const struct FViewportActionKeyInput& Action, bool& bOutIsInputCaptured, bool& bWasHandled )
{
	if( Action.ActionType == VRActionTypes::ConfirmRadialSelection )
	{
		if( Action.Event == IE_Pressed )
		{
			const EViewportInteractionDraggingMode DraggingMode = Interactor->GetDraggingMode();
			UViewportInteractor* OtherInteractor = Interactor->GetOtherInteractor();

			// Start dragging at laser impact when already dragging actors freely
			if( DraggingMode == EViewportInteractionDraggingMode::World ||
				( DraggingMode == EViewportInteractionDraggingMode::AssistingDrag && OtherInteractor != nullptr && OtherInteractor->GetDraggingMode() == EViewportInteractionDraggingMode::World ) )
			{
				const float DefaultWorldToMetersScale = VRMode->GetSavedEditorState().WorldToMetersScale;
				Scale( DefaultWorldToMetersScale );

				bWasHandled = true;
				bOutIsInputCaptured = true;
			}
		}
		else if( Action.Event == IE_Released )
		{
			bOutIsInputCaptured = false;
		}
	}
}

