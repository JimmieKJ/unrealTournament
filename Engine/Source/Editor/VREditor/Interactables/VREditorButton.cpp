// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorButton.h"
#include "ViewportInteractor.h"

AVREditorButton::AVREditorButton()
{
	const bool bAllowGizmoLighting = false;

	// Setup uniform scaling
	UStaticMesh* ButtonMesh = nullptr;
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ObjectFinder( TEXT( "/Engine/VREditor/TransformGizmo/UniformScaleHandle" ) );
		ButtonMesh = ObjectFinder.Object;
		check( ButtonMesh != nullptr );
	}

	ButtonMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>( TEXT( "ButtonMeshComponent" ) );
	check( ButtonMeshComponent != nullptr );

	ButtonMeshComponent->SetStaticMesh( ButtonMesh );
	ButtonMeshComponent->SetMobility( EComponentMobility::Movable );

	ButtonMeshComponent->SetCollisionEnabled( ECollisionEnabled::QueryOnly );
	ButtonMeshComponent->SetCollisionResponseToAllChannels( ECR_Ignore );
	ButtonMeshComponent->SetCollisionResponseToChannel( COLLISION_GIZMO, ECollisionResponse::ECR_Block );
	ButtonMeshComponent->SetCollisionObjectType( COLLISION_GIZMO );

	ButtonMeshComponent->bGenerateOverlapEvents = false;
	ButtonMeshComponent->SetCanEverAffectNavigation( false );
	ButtonMeshComponent->bCastDynamicShadow = bAllowGizmoLighting;
	ButtonMeshComponent->bCastStaticShadow = false;
	ButtonMeshComponent->bAffectDistanceFieldLighting = bAllowGizmoLighting;
	ButtonMeshComponent->bAffectDynamicIndirectLighting = bAllowGizmoLighting;
}

void AVREditorButton::OnPressed( UViewportInteractor* Interactor, const FHitResult& InHitResult, bool& bOutResultedInDrag  )
{
	OnPressedEvent.Broadcast( this, Interactor );
}

void AVREditorButton::OnHover( UViewportInteractor* Interactor )
{
	OnHoverEvent.Broadcast( this, Interactor );
}

void AVREditorButton::OnHoverEnter( UViewportInteractor* Interactor, const FHitResult& InHitResult  )
{
	OnHoverEnterEvent.Broadcast( this, Interactor );
}

void AVREditorButton::OnHoverLeave( UViewportInteractor* Interactor, const UActorComponent* NewComponent )
{
	OnHoverLeaveEvent.Broadcast( this, Interactor );
}

void AVREditorButton::OnDragRelease( UViewportInteractor* Interactor )
{
	OnReleaseEvent.Broadcast( this, Interactor );
}