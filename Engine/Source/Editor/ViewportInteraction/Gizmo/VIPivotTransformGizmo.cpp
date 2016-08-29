// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ViewportInteractionModule.h"
#include "VIPivotTransformGizmo.h"
#include "VIStretchGizmoHandle.h"
#include "VIUniformScaleGizmoHandle.h"

namespace VREd //@todo VREditor: Duplicates of TransformGizmo
{
	// @todo vreditor tweak: Tweak out console variables
	static FAutoConsoleVariable PivotGizmoDistanceScaleFactor( TEXT( "VREd.PivotGizmoDistanceScaleFactor" ), 0.003f, TEXT( "How much the gizmo handles should increase in size with distance from the camera, to make it easier to select"));
	static FAutoConsoleVariable PivotGizmoTranslationPivotOffsetX( TEXT("VREd.PivotGizmoTranslationPivotOffsetX" ), 20.0f, TEXT( "How much the translation cylinder is offsetted from the pivot" ) );
	static FAutoConsoleVariable PivotGizmoScalePivotOffsetX( TEXT( "VREd.PivotGizmoScalePivotOffsetX" ), 120.0f, TEXT( "How much the non-uniform scale is offsetted from the pivot" ) );
	static FAutoConsoleVariable PivotGizmoPlaneTranslationPivotOffsetYZ(TEXT("VREd.PivotGizmoPlaneTranslationPivotOffsetYZ" ), 40.0f, TEXT( "How much the plane translation is offsetted from the pivot" ) );
	static FAutoConsoleVariable PivotGizmoTranslationScaleMultiply( TEXT( "VREd.PivotGizmoTranslationScaleMultiply" ), 3.0f, TEXT( "Multiplies translation handles scale" ) );
	static FAutoConsoleVariable PivotGizmoTranslationHoverScaleMultiply( TEXT( "VREd.PivotGizmoTranslationHoverScaleMultiply" ), 1.2f, TEXT( "Multiplies translation handles hover scale" ) );
}

APivotTransformGizmo::APivotTransformGizmo() :
	Super()
{
	UniformScaleGizmoHandleGroup = CreateDefaultSubobject<UUniformScaleGizmoHandleGroup>( TEXT( "UniformScaleHandles" ), true );
	UniformScaleGizmoHandleGroup->SetTranslucentGizmoMaterial( TranslucentGizmoMaterial );
	UniformScaleGizmoHandleGroup->SetGizmoMaterial( GizmoMaterial );
	UniformScaleGizmoHandleGroup->SetupAttachment( SceneComponent );
	AllHandleGroups.Add( UniformScaleGizmoHandleGroup );

	TranslationGizmoHandleGroup = CreateDefaultSubobject<UPivotTranslationGizmoHandleGroup>(TEXT("TranslationHandles"), true);
	TranslationGizmoHandleGroup->SetTranslucentGizmoMaterial(TranslucentGizmoMaterial);
	TranslationGizmoHandleGroup->SetGizmoMaterial(GizmoMaterial);
	TranslationGizmoHandleGroup->SetupAttachment( SceneComponent );
	AllHandleGroups.Add(TranslationGizmoHandleGroup);

	ScaleGizmoHandleGroup = CreateDefaultSubobject<UPivotScaleGizmoHandleGroup>( TEXT( "ScaleHandles" ), true );
	ScaleGizmoHandleGroup->SetTranslucentGizmoMaterial( TranslucentGizmoMaterial );
	ScaleGizmoHandleGroup->SetGizmoMaterial( GizmoMaterial );
	ScaleGizmoHandleGroup->SetupAttachment( SceneComponent );
	AllHandleGroups.Add( ScaleGizmoHandleGroup );

	PlaneTranslationGizmoHandleGroup = CreateDefaultSubobject<UPivotPlaneTranslationGizmoHandleGroup>( TEXT( "PlaneTranslationHandles" ), true );
	PlaneTranslationGizmoHandleGroup->SetTranslucentGizmoMaterial( TranslucentGizmoMaterial );
	PlaneTranslationGizmoHandleGroup->SetGizmoMaterial( GizmoMaterial );
	PlaneTranslationGizmoHandleGroup->SetupAttachment( SceneComponent );
	AllHandleGroups.Add( PlaneTranslationGizmoHandleGroup );

	RotationGizmoHandleGroup = CreateDefaultSubobject<UPivotRotationGizmoHandleGroup>( TEXT( "RotationHandles" ), true );
	RotationGizmoHandleGroup->SetTranslucentGizmoMaterial( TranslucentGizmoMaterial );
	RotationGizmoHandleGroup->SetGizmoMaterial( GizmoMaterial );
	RotationGizmoHandleGroup->SetupAttachment( SceneComponent );
	AllHandleGroups.Add( RotationGizmoHandleGroup );

	StretchGizmoHandleGroup = CreateDefaultSubobject<UStretchGizmoHandleGroup>( TEXT( "StretchHandles" ), true );
	StretchGizmoHandleGroup->SetTranslucentGizmoMaterial( TranslucentGizmoMaterial );
	StretchGizmoHandleGroup->SetGizmoMaterial( GizmoMaterial );
	StretchGizmoHandleGroup->SetShowOnUniversalGizmo( false );
	StretchGizmoHandleGroup->SetupAttachment( SceneComponent );
	AllHandleGroups.Add( StretchGizmoHandleGroup );

	// There may already be some objects selected as we switch into VR mode, so we'll pretend that just happened so
	// that we can make sure all transitions complete properly
	OnNewObjectsSelected();
}

void APivotTransformGizmo::UpdateGizmo( const EGizmoHandleTypes GizmoType, const ECoordSystem GizmoCoordinateSpace, const FTransform& LocalToWorld, const FBox& LocalBounds, const FVector ViewLocation, bool bAllHandlesVisible, 
	UActorComponent* DraggingHandle, const TArray< UActorComponent* >& HoveringOverHandles, const float GizmoHoverScale, const float GizmoHoverAnimationDuration )
{
	const float WorldScaleFactor = GetWorld()->GetWorldSettings()->WorldToMeters / 100.0f;

	// Position the gizmo at the location of the first selected actor
	const bool bSweep = false;
	this->SetActorTransform( LocalToWorld, bSweep );

	// Increase scale with distance, to make gizmo handles easier to click on
	// @todo vreditor: Should probably be a curve, not linear
	// @todo vreditor: Should take FOV into account (especially in non-stereo/HMD mode)
	const float WorldSpaceDistanceToToPivot = FMath::Sqrt( FVector::DistSquared( GetActorLocation(), ViewLocation ) );
	const float GizmoScale( ( ( WorldSpaceDistanceToToPivot / WorldScaleFactor ) * VREd::PivotGizmoDistanceScaleFactor->GetFloat() ) * WorldScaleFactor );

	// Update animation
	float AnimationAlpha = GetAnimationAlpha();

	// Update all the handles
	bool bIsHoveringOrDraggingScaleGizmo = false;
	for ( UGizmoHandleGroup* HandleGroup : AllHandleGroups )
	{
		if ( HandleGroup != nullptr && 
		   ( DraggingHandle == nullptr || HandleGroup == StretchGizmoHandleGroup ) )
		{
			bool bIsHoveringOrDraggingThisHandleGroup = false;
			HandleGroup->UpdateGizmoHandleGroup( LocalToWorld, LocalBounds, ViewLocation, bAllHandlesVisible, DraggingHandle,
				HoveringOverHandles, AnimationAlpha, GizmoScale, GizmoHoverScale, GizmoHoverAnimationDuration, /* Out */ bIsHoveringOrDraggingThisHandleGroup );
		}
	}

	UpdateHandleVisibility( GizmoType, GizmoCoordinateSpace, bAllHandlesVisible, DraggingHandle );
}

/************************************************************************/
/* Translation                                                          */
/************************************************************************/
UPivotTranslationGizmoHandleGroup::UPivotTranslationGizmoHandleGroup() :
	Super()
{
	UStaticMesh* TranslationHandleMesh = nullptr;
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ObjectFinder( TEXT( "/Engine/VREditor/TransformGizmo/TranslateHandleLong" ) );
		TranslationHandleMesh = ObjectFinder.Object;
		check( TranslationHandleMesh != nullptr );
	}

	CreateHandles( TranslationHandleMesh, FString( "PivotTranslationHandle" ) );
}


void UPivotTranslationGizmoHandleGroup::UpdateGizmoHandleGroup( const FTransform& LocalToWorld, const FBox& LocalBounds, const FVector ViewLocation, bool bAllHandlesVisible, class UActorComponent* DraggingHandle, 
	const TArray< UActorComponent* >& HoveringOverHandles, float AnimationAlpha, float GizmoScale, const float GizmoHoverScale, const float GizmoHoverAnimationDuration, bool& bOutIsHoveringOrDraggingThisHandleGroup )
{
	// Call parent implementation (updates hover animation)
	Super::UpdateGizmoHandleGroup( LocalToWorld, LocalBounds, ViewLocation, bAllHandlesVisible, DraggingHandle, HoveringOverHandles, AnimationAlpha,
		GizmoScale, GizmoHoverScale, GizmoHoverAnimationDuration, bOutIsHoveringOrDraggingThisHandleGroup );
	
	const float MultipliedGizmoScale = GizmoScale * VREd::PivotGizmoTranslationScaleMultiply->GetFloat();
	const float MultipliedGizmoHoverScale = GizmoHoverScale * VREd::PivotGizmoTranslationHoverScaleMultiply->GetFloat();

	UpdateHandlesRelativeTransformOnAxis( FTransform( FVector( VREd::PivotGizmoTranslationPivotOffsetX->GetFloat(), 0, 0 ) ), AnimationAlpha, MultipliedGizmoScale, MultipliedGizmoHoverScale, ViewLocation, DraggingHandle, HoveringOverHandles );
}

ETransformGizmoInteractionType UPivotTranslationGizmoHandleGroup::GetInteractionType() const
{
	return ETransformGizmoInteractionType::Translate;
}

EGizmoHandleTypes UPivotTranslationGizmoHandleGroup::GetHandleType() const
{
	return EGizmoHandleTypes::Translate;
}

/************************************************************************/
/* Scale	                                                            */
/************************************************************************/
UPivotScaleGizmoHandleGroup::UPivotScaleGizmoHandleGroup() :
	Super()
{
	UStaticMesh* TranslationHandleMesh = nullptr;
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ObjectFinder( TEXT( "/Engine/VREditor/TransformGizmo/UniformScaleHandle" ) );
		TranslationHandleMesh = ObjectFinder.Object;
		check( TranslationHandleMesh != nullptr );
	}

	CreateHandles( TranslationHandleMesh, FString( "PivotScaleHandle" ) );	
}

void UPivotScaleGizmoHandleGroup::UpdateGizmoHandleGroup( const FTransform& LocalToWorld, const FBox& LocalBounds, const FVector ViewLocation, bool bAllHandlesVisible, class UActorComponent* DraggingHandle, 
	const TArray< UActorComponent* >& HoveringOverHandles, float AnimationAlpha, float GizmoScale, const float GizmoHoverScale, const float GizmoHoverAnimationDuration, bool& bOutIsHoveringOrDraggingThisHandleGroup )
{
	// Call parent implementation (updates hover animation)
	Super::UpdateGizmoHandleGroup( LocalToWorld, LocalBounds, ViewLocation, bAllHandlesVisible, DraggingHandle, HoveringOverHandles, AnimationAlpha, 
		GizmoScale, GizmoHoverScale, GizmoHoverAnimationDuration, bOutIsHoveringOrDraggingThisHandleGroup );

	UpdateHandlesRelativeTransformOnAxis( FTransform( FVector( VREd::PivotGizmoScalePivotOffsetX->GetFloat(), 0, 0 ) ), AnimationAlpha, GizmoScale, GizmoHoverScale, ViewLocation, DraggingHandle, HoveringOverHandles );
}

ETransformGizmoInteractionType UPivotScaleGizmoHandleGroup::GetInteractionType() const
{
	return ETransformGizmoInteractionType::Scale;
}

EGizmoHandleTypes UPivotScaleGizmoHandleGroup::GetHandleType() const
{
	return EGizmoHandleTypes::Scale;
}

bool UPivotScaleGizmoHandleGroup::SupportsWorldCoordinateSpace() const
{
	return false;
}

/************************************************************************/
/* Plane Translation	                                                */
/************************************************************************/
UPivotPlaneTranslationGizmoHandleGroup::UPivotPlaneTranslationGizmoHandleGroup() :
	Super()
{
	UStaticMesh* TranslationHandleMesh = nullptr;
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ObjectFinder( TEXT( "/Engine/VREditor/TransformGizmo/PlaneTranslationHandle" ) );
		TranslationHandleMesh = ObjectFinder.Object;
		check( TranslationHandleMesh != nullptr );
	}

	CreateHandles( TranslationHandleMesh, FString( "PlaneTranslationHandle" ) );
}

void UPivotPlaneTranslationGizmoHandleGroup::UpdateGizmoHandleGroup( const FTransform& LocalToWorld, const FBox& LocalBounds, const FVector ViewLocation, bool bAllHandlesVisible, class UActorComponent* DraggingHandle,
	const TArray< UActorComponent* >& HoveringOverHandles, float AnimationAlpha, float GizmoScale, const float GizmoHoverScale, const float GizmoHoverAnimationDuration, bool& bOutIsHoveringOrDraggingThisHandleGroup )
{
	// Call parent implementation (updates hover animation)
	Super::UpdateGizmoHandleGroup( LocalToWorld, LocalBounds, ViewLocation, bAllHandlesVisible, DraggingHandle, HoveringOverHandles, AnimationAlpha,
		GizmoScale, GizmoHoverScale, GizmoHoverAnimationDuration, bOutIsHoveringOrDraggingThisHandleGroup );

	UpdateHandlesRelativeTransformOnAxis( FTransform( FVector( 0, VREd::PivotGizmoPlaneTranslationPivotOffsetYZ->GetFloat(), VREd::PivotGizmoPlaneTranslationPivotOffsetYZ->GetFloat() ) ), 
		AnimationAlpha, GizmoScale, GizmoHoverScale, ViewLocation, DraggingHandle, HoveringOverHandles );
}

ETransformGizmoInteractionType UPivotPlaneTranslationGizmoHandleGroup::GetInteractionType() const
{
	return ETransformGizmoInteractionType::TranslateOnPlane;
}

EGizmoHandleTypes UPivotPlaneTranslationGizmoHandleGroup::GetHandleType() const
{
	return EGizmoHandleTypes::Translate;
}

/************************************************************************/
/* Rotation																*/
/************************************************************************/
UPivotRotationGizmoHandleGroup::UPivotRotationGizmoHandleGroup() :
	Super()
{
	UStaticMesh* TranslationHandleMesh = nullptr;
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ObjectFinder(TEXT("/Engine/VREditor/TransformGizmo/RotationHandleFull"));
		TranslationHandleMesh = ObjectFinder.Object;
		check(TranslationHandleMesh != nullptr);
	}

	CreateHandles(TranslationHandleMesh, FString("RotationHandle"));
}

void UPivotRotationGizmoHandleGroup::UpdateGizmoHandleGroup( const FTransform& LocalToWorld, const FBox& LocalBounds, const FVector ViewLocation, bool bAllHandlesVisible, class UActorComponent* DraggingHandle,
	const TArray< UActorComponent* >& HoveringOverHandles, float AnimationAlpha, float GizmoScale, const float GizmoHoverScale, const float GizmoHoverAnimationDuration, bool& bOutIsHoveringOrDraggingThisHandleGroup )
{
	// Call parent implementation (updates hover animation)
	Super::UpdateGizmoHandleGroup(LocalToWorld, LocalBounds, ViewLocation, bAllHandlesVisible, DraggingHandle, HoveringOverHandles, AnimationAlpha,
		GizmoScale, GizmoHoverScale, GizmoHoverAnimationDuration, bOutIsHoveringOrDraggingThisHandleGroup );
	
	UpdateHandlesRelativeTransformOnAxis( FTransform( FVector( 0, 0, 0 ) ), AnimationAlpha, GizmoScale, GizmoScale, ViewLocation, DraggingHandle, HoveringOverHandles );
}

ETransformGizmoInteractionType UPivotRotationGizmoHandleGroup::GetInteractionType() const
{
	return ETransformGizmoInteractionType::RotateOnAngle;
}

EGizmoHandleTypes UPivotRotationGizmoHandleGroup::GetHandleType() const
{
	return EGizmoHandleTypes::Rotate;
}