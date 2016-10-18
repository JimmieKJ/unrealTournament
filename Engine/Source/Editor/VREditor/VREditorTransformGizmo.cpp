// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorTransformGizmo.h"
#include "UnitConversion.h"
#include "Gizmo/VREditorTranslationGizmoHandle.h"
#include "Gizmo/VREditorRotationGizmoHandle.h"
#include "Gizmo/VREditorPlaneTranslationGizmoHandle.h"
#include "VIStretchGizmoHandle.h"
#include "VIUniformScaleGizmoHandle.h"
#include "VREditorMode.h"

namespace VREd
{
	// @todo vreditor tweak: Tweak out console variables
	static FAutoConsoleVariable GizmoScale( TEXT( "VREd.GizmoScale" ), 0.80f, TEXT( "How big the gizmo handles should be" ) );
	static FAutoConsoleVariable GizmoDistanceScaleFactor( TEXT( "VREd.GizmoDistanceScaleFactor" ), 0.002f, TEXT( "How much the gizmo handles should increase in size with distance from the camera, to make it easier to select" ) );
	static FAutoConsoleVariable MinActorSizeForTransformGizmo( TEXT( "VREd.MinActorSizeForTransformGizmo" ), 50.0f, TEXT( "How big an object must be in scaled world units before we'll start to shrink the gizmo" ) );
}


ATransformGizmo::ATransformGizmo()
{
	TranslationGizmoHandleGroup = CreateDefaultSubobject<UVREditorTranslationGizmoHandleGroup>( TEXT( "TranslationHandles" ), true );
	TranslationGizmoHandleGroup->SetTranslucentGizmoMaterial( TranslucentGizmoMaterial );
	TranslationGizmoHandleGroup->SetGizmoMaterial( GizmoMaterial );
	TranslationGizmoHandleGroup->SetupAttachment( SceneComponent );
	AllHandleGroups.Add( TranslationGizmoHandleGroup );

	PlaneTranslationGizmoHandleGroup = CreateDefaultSubobject<UVREditorPlaneTranslationGizmoHandleGroup>( TEXT( "PlaneTranslationHandles" ), true );
	PlaneTranslationGizmoHandleGroup->SetTranslucentGizmoMaterial( TranslucentGizmoMaterial );
	PlaneTranslationGizmoHandleGroup->SetGizmoMaterial( GizmoMaterial );
	PlaneTranslationGizmoHandleGroup->SetupAttachment( SceneComponent );
	AllHandleGroups.Add( PlaneTranslationGizmoHandleGroup );

	RotationGizmoHandleGroup = CreateDefaultSubobject<UVREditorRotationGizmoHandleGroup>( TEXT( "RotationHandles" ), true );
	RotationGizmoHandleGroup->SetTranslucentGizmoMaterial( TranslucentGizmoMaterial );
	RotationGizmoHandleGroup->SetGizmoMaterial( GizmoMaterial );
	RotationGizmoHandleGroup->SetupAttachment( SceneComponent );
	AllHandleGroups.Add( RotationGizmoHandleGroup );

	StretchGizmoHandleGroup = CreateDefaultSubobject<UStretchGizmoHandleGroup>( TEXT( "StretchHandles" ), true );
	StretchGizmoHandleGroup->SetTranslucentGizmoMaterial( TranslucentGizmoMaterial );
	StretchGizmoHandleGroup->SetGizmoMaterial( GizmoMaterial );
	StretchGizmoHandleGroup->SetupAttachment( SceneComponent );
	AllHandleGroups.Add( StretchGizmoHandleGroup );

	UniformScaleGizmoHandleGroup = CreateDefaultSubobject<UUniformScaleGizmoHandleGroup>( TEXT( "UniformScaleHandles" ), true );
	UniformScaleGizmoHandleGroup->SetTranslucentGizmoMaterial( TranslucentGizmoMaterial );
	UniformScaleGizmoHandleGroup->SetGizmoMaterial( GizmoMaterial );
	UniformScaleGizmoHandleGroup->SetUsePivotPointAsLocation( false );
	UniformScaleGizmoHandleGroup->SetupAttachment( SceneComponent );
	AllHandleGroups.Add( UniformScaleGizmoHandleGroup );

	const bool bAllowGizmoLighting = false;	// @todo vreditor: Not sure if we want this for gizmos or not yet.  Needs feedback.  Also they're translucent right now.

	// Setup measurements
	{
		UMaterialInterface* GizmoTextMaterial = nullptr;
		{
			static ConstructorHelpers::FObjectFinder<UMaterial> ObjectFinder( TEXT( "/Engine/VREditor/Fonts/VRTextMaterial" ) );
			GizmoTextMaterial = ObjectFinder.Object;
			check( GizmoTextMaterial != nullptr );
		}

		UFont* GizmoTextFont = nullptr;
		{
			static ConstructorHelpers::FObjectFinder<UFont> ObjectFinder( TEXT( "/Engine/VREditor/Fonts/VRText_RobotoLarge" ) );
			GizmoTextFont = ObjectFinder.Object;
			check( GizmoTextFont != nullptr );
		}

		for( int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex )
		{
			const FString AxisName = AxisIndex == 0 ? TEXT( "X" ) : ( AxisIndex == 1 ? TEXT( "Y" ) : TEXT( "Z" ) );

			FString ComponentName = AxisName + TEXT( "Measurement" );

			UTextRenderComponent* MeasurementText = CreateDefaultSubobject<UTextRenderComponent>( *ComponentName );
			check( MeasurementText != nullptr );

			MeasurementText->SetMobility( EComponentMobility::Movable );
			MeasurementText->SetupAttachment( SceneComponent );

			MeasurementText->SetCollisionProfileName( UCollisionProfile::NoCollision_ProfileName );

			MeasurementText->bGenerateOverlapEvents = false;
			MeasurementText->SetCanEverAffectNavigation( false );
			MeasurementText->bCastDynamicShadow = bAllowGizmoLighting;
			MeasurementText->bCastStaticShadow = false;
			MeasurementText->bAffectDistanceFieldLighting = bAllowGizmoLighting;
			MeasurementText->bAffectDynamicIndirectLighting = bAllowGizmoLighting;

			// Use a custom font.  The text will be visible up close.
			MeasurementText->SetFont( GizmoTextFont );

			MeasurementText->SetWorldSize( 8.0f );	// @todo vreditor tweak: Size of text in world units

			// Assign our custom text rendering material.  It should be Unlit and have Depth Test disabled
			// so that it draws on top of overlapping geometry.
			MeasurementText->SetTextMaterial( GizmoTextMaterial );

			// Color the text based on the axis it represents
			switch( AxisIndex )
			{
			case 0:
				MeasurementText->SetTextRenderColor( FLinearColor( 0.4f, 0.075f, 0.075f ).ToFColor( false ) );
				break;
			case 1:
				MeasurementText->SetTextRenderColor( FLinearColor( 0.075f, 0.4f, 0.075f ).ToFColor( false ) );
				break;
			case 2:
				MeasurementText->SetTextRenderColor( FLinearColor( 0.075f, 0.075f, 0.4f ).ToFColor( false ) );
				break;
			}

			// Center the text horizontally
			MeasurementText->SetHorizontalAlignment( EHTA_Center );

			FTransformGizmoMeasurement& Measurement = Measurements[ AxisIndex ];
			Measurement.MeasurementText = MeasurementText;
		}
	}

	// There may already be some objects selected as we switch into VR mode, so we'll pretend that just happened so
	// that we can make sure all transitions complete properly
	OnNewObjectsSelected();
}


void ATransformGizmo::UpdateGizmo( const EGizmoHandleTypes GizmoType, const ECoordSystem GizmoCoordinateSpace, const FTransform& LocalToWorld, const FBox& LocalBounds, const FVector ViewLocation, bool bAllHandlesVisible, 
	UActorComponent* DraggingHandle, const TArray< UActorComponent* >& HoveringOverHandles, const float GizmoHoverScale, const float GizmoHoverAnimationDuration )
{
	Super::UpdateGizmo( GizmoType, GizmoCoordinateSpace, LocalToWorld, LocalBounds, ViewLocation, bAllHandlesVisible,
		DraggingHandle, HoveringOverHandles, GizmoHoverScale, GizmoHoverAnimationDuration );

	const float WorldScaleFactor = GetWorld()->GetWorldSettings()->WorldToMeters / 100.0f;

	// Position the gizmo at the location of the first selected actor
	const bool bSweep = false;
	this->SetActorTransform( LocalToWorld, bSweep );

	const FBox WorldSpaceBounds = LocalBounds.TransformBy( LocalToWorld );
	float ScaleCompensationForTinyGizmo = 1.0f;
	{
		const float AvgBounds = FMath::Lerp( WorldSpaceBounds.GetSize().GetAbsMin(), WorldSpaceBounds.GetSize().GetAbsMax(), 0.5f );
		const float GizmoSizeInRoomSpace = ( AvgBounds / WorldScaleFactor );
		if( GizmoSizeInRoomSpace < VREd::MinActorSizeForTransformGizmo->GetFloat() )
		{
			ScaleCompensationForTinyGizmo = GizmoSizeInRoomSpace / VREd::MinActorSizeForTransformGizmo->GetFloat();
		}
	}

	// Increase scale with distance, to make gizmo handles easier to click on
	// @todo vreditor: Should probably be a curve, not linear
	// @todo vreditor: Should take FOV into account (especially in non-stereo/HMD mode)
	const float WorldSpaceDistanceToGizmoBounds = FMath::Sqrt( WorldSpaceBounds.ComputeSquaredDistanceToPoint( ViewLocation ) );
	const float GizmoScaleUpClose = VREd::GizmoScale->GetFloat();
	const float GizmoScale( ( GizmoScaleUpClose + ( WorldSpaceDistanceToGizmoBounds / WorldScaleFactor ) * VREd::GizmoDistanceScaleFactor->GetFloat() ) * ScaleCompensationForTinyGizmo * WorldScaleFactor );

	// Update animation
	float AnimationAlpha = GetAnimationAlpha();

	// Update all the handles
	bool bIsHoveringOrDraggingScaleGizmo = false;
	for ( UGizmoHandleGroup* HandleGroup : AllHandleGroups )
	{
		if ( HandleGroup != nullptr )
		{
			bool bIsHoveringOrDraggingThisHandleGroup = false;
			HandleGroup->UpdateGizmoHandleGroup( LocalToWorld, LocalBounds, ViewLocation, bAllHandlesVisible, DraggingHandle, 
				HoveringOverHandles, AnimationAlpha, GizmoScale, GizmoHoverScale, GizmoHoverAnimationDuration, /* Out */ bIsHoveringOrDraggingThisHandleGroup );
			
			if( HandleGroup->GetHandleType() == EGizmoHandleTypes::Scale && bIsHoveringOrDraggingThisHandleGroup )
			{
				bIsHoveringOrDraggingScaleGizmo = true;
			}
		}
	}

	// Place measurements
	{
		for( int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex )
		{
			FTransformGizmoMeasurement& Measurement = Measurements[ AxisIndex ];

			// Update measurement text
			{
				const float LocalSpaceLengthOfBoundsAlongAxis = LocalBounds.GetSize()[ AxisIndex ];

				FVector LocalSpaceAxisLengthVector = FVector::ZeroVector;
				LocalSpaceAxisLengthVector[ AxisIndex ] = LocalSpaceLengthOfBoundsAlongAxis;

				// Transform to world space
				const FVector WorldSpaceAxisLengthVector = LocalToWorld.TransformVector( LocalSpaceAxisLengthVector );
				const float WorldSpaceLengthOfBoundsAlongAxis = WorldSpaceAxisLengthVector.Size();

				FText BestSizeString;
				{
					const FNumericUnit<float> BestUnit = FUnitConversion::QuantizeUnitsToBestFit( WorldSpaceLengthOfBoundsAlongAxis, EUnit::Centimeters );

					FNumberFormattingOptions NumberFormattingOptions;
					NumberFormattingOptions.MaximumFractionalDigits = 2;	// @todo sizemap: We could make the number of digits customizable in the UI
					NumberFormattingOptions.MinimumFractionalDigits = 0;
					NumberFormattingOptions.MinimumIntegralDigits = 1;

					BestSizeString =
						FText::FromString( FString::Printf( TEXT( "%s %s" ),
							*FText::AsNumber( BestUnit.Value, &NumberFormattingOptions ).ToString(),
							FUnitConversion::GetUnitDisplayString( BestUnit.Units ) ) );
				}

				Measurement.MeasurementText->SetText( BestSizeString );
			}


			// Position the measurement in the center of the axis along the top of the bounds
			{
				// Figure which bounding box edge on this axis is closest to the viewer.  There are four edges to choose from!
				FVector GizmoSpaceClosestEdge0, GizmoSpaceClosestEdge1;
				{
					float ClosestSquaredDistance = TNumericLimits<float>::Max();
					for( int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex )
					{
						FVector GizmoSpaceEdge0, GizmoSpaceEdge1;
						GetBoundingBoxEdge( LocalBounds, AxisIndex, EdgeIndex, /* Out */ GizmoSpaceEdge0, /* Out */ GizmoSpaceEdge1 );

						const FVector WorldSpaceEdge0 = LocalToWorld.TransformPosition( GizmoSpaceEdge0 );
						const FVector WorldSpaceEdge1 = LocalToWorld.TransformPosition( GizmoSpaceEdge1 );

						const float SquaredDistance = FMath::PointDistToSegmentSquared( ViewLocation, WorldSpaceEdge0, WorldSpaceEdge1 );
						if( SquaredDistance < ClosestSquaredDistance )
						{
							ClosestSquaredDistance = SquaredDistance;

							GizmoSpaceClosestEdge0 = GizmoSpaceEdge0;
							GizmoSpaceClosestEdge1 = GizmoSpaceEdge1;
						}
					}

				}

				const FVector EdgeVector = GizmoSpaceClosestEdge1 - GizmoSpaceClosestEdge0;
				const FVector GizmoSpaceEdgeCenter = GizmoSpaceClosestEdge0 + EdgeVector * 0.5f;

				const float UpOffsetAmount = GizmoScale * 5.0f;	// Push the text up a bit so that it doesn't overlap so much with our handles
				const FVector GizmoSpaceUpVector = LocalToWorld.InverseTransformVectorNoScale( FVector::UpVector );

				Measurement.MeasurementText->SetRelativeLocation( GizmoSpaceEdgeCenter + GizmoSpaceUpVector * UpOffsetAmount );

				const int32 TextFacingAxisIndex = AxisIndex == 0 ? 1 : ( AxisIndex == 1 ? 0 : 0 );
				FVector TextFacingDirection = UGizmoHandleGroup::GetAxisVector( TextFacingAxisIndex, ETransformGizmoHandleDirection::Positive );

				// Make sure to face the camera
				const FVector WorldSpaceEdgeCenterToViewDirection = ( LocalToWorld.TransformPosition( GizmoSpaceEdgeCenter ) - ViewLocation ).GetSafeNormal();
				if( FVector::DotProduct( LocalToWorld.TransformVectorNoScale( TextFacingDirection ), WorldSpaceEdgeCenterToViewDirection ) > 0.0f )
				{
					TextFacingDirection *= -1.0f;
				}

				const FQuat GizmoSpaceOrientation = TextFacingDirection.ToOrientationQuat();
				Measurement.MeasurementText->SetRelativeRotation( GizmoSpaceOrientation );

				Measurement.MeasurementText->SetRelativeScale3D( FVector( GizmoScale ) );
			}
		}
	}

	// Update visibility
	{
		const bool bShouldShowMeasurements =
			GetShowMeasurementText() ||
			bIsHoveringOrDraggingScaleGizmo;

		for( int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex )
		{
			FTransformGizmoMeasurement& Measurement = Measurements[ AxisIndex ];

			Measurement.MeasurementText->SetVisibility( bShouldShowMeasurements );
		}
	}
}
