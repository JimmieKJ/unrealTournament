// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ViewportInteractionModule.h"
#include "MouseCursorInteractor.h"
#include "ViewportWorldInteraction.h"


UMouseCursorInteractor::UMouseCursorInteractor( const FObjectInitializer& Initializer ) 
	: UViewportInteractor( Initializer )
{
}


void UMouseCursorInteractor::Init()
{
	KeyToActionMap.Reset();

	// Setup keys
	{
		AddKeyAction( EKeys::LeftMouseButton, FViewportActionKeyInput( ViewportWorldActionTypes::SelectAndMove ) );
		AddKeyAction( EKeys::MiddleMouseButton, FViewportActionKeyInput( ViewportWorldActionTypes::SelectAndMove_LightlyPressed ) );
	}
}


void UMouseCursorInteractor::PollInput()
{
	FEditorViewportClient& ViewportClient = *WorldInteraction->GetViewportClient();

	InteractorData.LastTransform = InteractorData.Transform;
	InteractorData.LastRoomSpaceTransform = InteractorData.RoomSpaceTransform;

	// Only if we're not tracking (RMB looking)
	if( !ViewportClient.IsTracking() )
	{
		FViewport* Viewport = ViewportClient.Viewport;

		// Make sure we have a valid viewport, otherwise we won't be able to construct an FSceneView.  The first time we're ticked we might not be properly setup. (@todo mesheditor)
		if( Viewport != nullptr && Viewport->GetSizeXY().GetMin() > 0 )
		{
			const int32 ViewportInteractX = Viewport->GetMouseX();
			const int32 ViewportInteractY = Viewport->GetMouseY();

			FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues(
				Viewport,
				ViewportClient.GetScene(),
				ViewportClient.EngineShowFlags )
				.SetRealtimeUpdate( ViewportClient.IsRealtime() ) );
			FSceneView* SceneView = ViewportClient.CalcSceneView( &ViewFamily );

			const FViewportCursorLocation MouseViewportRay( SceneView, &ViewportClient, ViewportInteractX, ViewportInteractY );

			InteractorData.Transform = FTransform( MouseViewportRay.GetDirection().ToOrientationQuat(), MouseViewportRay.GetOrigin(), FVector( 1.0f ) );
		}
	}
	InteractorData.RoomSpaceTransform = InteractorData.Transform * WorldInteraction->GetRoomTransform().Inverse();
}
