// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorRotationGizmoHandle.h"
#include "UnitConversion.h"
#include "VREditorTransformGizmo.h"

UVREditorRotationGizmoHandleGroup::UVREditorRotationGizmoHandleGroup()
	: Super()
{
	UStaticMesh* RotationHandleMesh = nullptr;
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ObjectFinder( TEXT( "/Engine/VREditor/TransformGizmo/RotationHandle" ) );
		RotationHandleMesh = ObjectFinder.Object;
		check( RotationHandleMesh != nullptr );
	}

	const bool bAllowGizmoLighting = false;	// @todo vreditor: Not sure if we want this for gizmos or not yet.  Needs feedback.  Also they're translucent right now.

	for (int32 X = 0; X < 3; ++X)
	{
		for (int32 Y = 0; Y < 3; ++Y)
		{
			for (int32 Z = 0; Z < 3; ++Z)
			{
				FTransformGizmoHandlePlacement HandlePlacement = GetHandlePlacement( X, Y, Z );

				int32 CenterHandleCount, FacingAxisIndex, CenterAxisIndex;
				HandlePlacement.GetCenterHandleCountAndFacingAxisIndex( /* Out */ CenterHandleCount, /* Out */ FacingAxisIndex, /* Out */ CenterAxisIndex );

				// Don't allow translation/stretching/rotation from the origin
				if (CenterHandleCount < 3)
				{
					const FString HandleName = MakeHandleName( HandlePlacement );

					// Rotation handle
					// Don't bother with rotation handles on corners or faces.  They don't make too much sense.  Only edges.
					if (CenterHandleCount == 1)	// Edge?
					{
						FString ComponentName = HandleName + TEXT( "RotationHandle" );
						CreateAndAddMeshHandle( RotationHandleMesh, ComponentName, HandlePlacement );
					}
				}
			}
		}
	}
}

void UVREditorRotationGizmoHandleGroup::UpdateGizmoHandleGroup( const FTransform& LocalToWorld, const FBox& LocalBounds, const FVector ViewLocation, bool bAllHandlesVisible, class UActorComponent* DraggingHandle, 
	const TArray< UActorComponent* >& HoveringOverHandles, float AnimationAlpha, float GizmoScale, const float GizmoHoverScale, const float GizmoHoverAnimationDuration, bool& bOutIsHoveringOrDraggingThisHandleGroup )
{
	// Call parent implementation (updates hover animation)
	Super::UpdateGizmoHandleGroup(LocalToWorld, LocalBounds, ViewLocation, bAllHandlesVisible, DraggingHandle, HoveringOverHandles,
		AnimationAlpha, GizmoScale, GizmoHoverScale, GizmoHoverAnimationDuration, bOutIsHoveringOrDraggingThisHandleGroup);

	// Place the rotation handles
	for (int32 HandleIndex = 0; HandleIndex < Handles.Num(); ++HandleIndex)
	{
		FGizmoHandle& Handle = Handles[ HandleIndex ];

		UStaticMeshComponent* RotationHandle = Handle.HandleMesh;
		if (RotationHandle != nullptr)	// Can be null if no handle for this specific placement
		{
			const FTransformGizmoHandlePlacement HandlePlacement = MakeHandlePlacementForIndex( HandleIndex );

			float GizmoHandleScale = GizmoScale;

			const float OffsetFromSide = GizmoHandleScale *
				(1.0f +	// @todo vreditor tweak: Hard coded handle offset from side of primitive
				(1.0f - AnimationAlpha) * 10.0f);	// @todo vreditor tweak: Animation offset

			// Make the handle bigger while hovered (but don't affect the offset -- we want it to scale about it's origin)
			GizmoHandleScale *= FMath::Lerp( 1.0f, GizmoHoverScale, Handle.HoverAlpha );

			FVector HandleRelativeLocation = FVector::ZeroVector;
			for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
			{
				if (HandlePlacement.Axes[AxisIndex] == ETransformGizmoHandleDirection::Negative)	// Negative direction
				{
					HandleRelativeLocation[AxisIndex] = LocalBounds.Min[AxisIndex] - OffsetFromSide;
				}
				else if (HandlePlacement.Axes[AxisIndex] == ETransformGizmoHandleDirection::Positive)	// Positive direction
				{
					HandleRelativeLocation[AxisIndex] = LocalBounds.Max[AxisIndex] + OffsetFromSide;
				}
				else // ETransformGizmoHandleDirection::Center
				{
					HandleRelativeLocation[AxisIndex] = LocalBounds.GetCenter()[AxisIndex];
				}
			}

			RotationHandle->SetRelativeLocation( HandleRelativeLocation );

			FRotator Rotator = FRotator::ZeroRotator;

			// Back bottom edge
			if (HandlePlacement.Axes[0] == ETransformGizmoHandleDirection::Negative && HandlePlacement.Axes[2] == ETransformGizmoHandleDirection::Negative)
			{
				Rotator.Yaw = 0.0f;
				Rotator.Pitch = 0.0f;
				Rotator.Roll = 0.0f;
			}

			// Back left edge
			else if (HandlePlacement.Axes[0] == ETransformGizmoHandleDirection::Negative && HandlePlacement.Axes[1] == ETransformGizmoHandleDirection::Negative)
			{
				Rotator.Yaw = 0.0f;
				Rotator.Pitch = 0.0f;
				Rotator.Roll = 90.0f;
			}

			// Back top edge
			else if (HandlePlacement.Axes[0] == ETransformGizmoHandleDirection::Negative && HandlePlacement.Axes[2] == ETransformGizmoHandleDirection::Positive)
			{
				Rotator.Yaw = 0.0f;
				Rotator.Pitch = -90.0f;
				Rotator.Roll = 0.0f;
			}

			// Back right edge
			else if (HandlePlacement.Axes[0] == ETransformGizmoHandleDirection::Negative && HandlePlacement.Axes[1] == ETransformGizmoHandleDirection::Positive)
			{
				Rotator.Yaw = 0.0f;
				Rotator.Pitch = 0.0f;
				Rotator.Roll = -90.0f;
			}

			// Front bottom edge
			else if (HandlePlacement.Axes[0] == ETransformGizmoHandleDirection::Positive && HandlePlacement.Axes[2] == ETransformGizmoHandleDirection::Negative)
			{
				Rotator.Yaw = 180.0f;
				Rotator.Pitch = 0.0f;
				Rotator.Roll = 0.0f;
			}

			// Front left edge
			else if (HandlePlacement.Axes[0] == ETransformGizmoHandleDirection::Positive && HandlePlacement.Axes[1] == ETransformGizmoHandleDirection::Negative)
			{
				Rotator.Yaw = 180.0f;
				Rotator.Pitch = 0.0f;
				Rotator.Roll = -90.0f;
			}

			// Front top edge
			else if (HandlePlacement.Axes[0] == ETransformGizmoHandleDirection::Positive && HandlePlacement.Axes[2] == ETransformGizmoHandleDirection::Positive)
			{
				Rotator.Yaw = 180.0f;
				Rotator.Pitch = -90.0f;
				Rotator.Roll = 0.0f;
			}

			// Front right edge
			else if (HandlePlacement.Axes[0] == ETransformGizmoHandleDirection::Positive && HandlePlacement.Axes[1] == ETransformGizmoHandleDirection::Positive)
			{
				Rotator.Yaw = 180.0f;
				Rotator.Pitch = 0.0f;
				Rotator.Roll = 90.0f;
			}

			// Left bottom edge
			else if (HandlePlacement.Axes[1] == ETransformGizmoHandleDirection::Negative && HandlePlacement.Axes[2] == ETransformGizmoHandleDirection::Negative)
			{
				Rotator.Yaw = 90.0f;
				Rotator.Pitch = 0.0f;
				Rotator.Roll = 0.0f;
			}

			// Left top edge
			else if (HandlePlacement.Axes[1] == ETransformGizmoHandleDirection::Negative && HandlePlacement.Axes[2] == ETransformGizmoHandleDirection::Positive)
			{
				Rotator.Yaw = 90.0f;
				Rotator.Pitch = -90.0f;
				Rotator.Roll = 0.0f;
			}

			// Right bottom edge
			else if (HandlePlacement.Axes[1] == ETransformGizmoHandleDirection::Positive && HandlePlacement.Axes[2] == ETransformGizmoHandleDirection::Negative)
			{
				Rotator.Yaw = -90.0f;
				Rotator.Pitch = 0.0f;
				Rotator.Roll = 0.0f;
			}

			// Right top edge
			else if (HandlePlacement.Axes[1] == ETransformGizmoHandleDirection::Positive && HandlePlacement.Axes[2] == ETransformGizmoHandleDirection::Positive)
			{
				Rotator.Yaw = -90.0f;
				Rotator.Pitch = -90.0f;
				Rotator.Roll = 0.0f;
			}

			else
			{
				// No other valid edges
				ensure( 0 );
			}

			RotationHandle->SetRelativeRotation( Rotator );

			RotationHandle->SetRelativeScale3D( FVector( GizmoHandleScale ) );

			int32 CenterHandleCount, FacingAxisIndex, CenterAxisIndex;
			HandlePlacement.GetCenterHandleCountAndFacingAxisIndex( /* Out */ CenterHandleCount, /* Out */ FacingAxisIndex, /* Out */ CenterAxisIndex );

			// Update material
			UpdateHandleColor( CenterAxisIndex, Handle, DraggingHandle, HoveringOverHandles);
		}
	}
}

ETransformGizmoInteractionType UVREditorRotationGizmoHandleGroup::GetInteractionType() const
{
	return ETransformGizmoInteractionType::Rotate;
}

EGizmoHandleTypes UVREditorRotationGizmoHandleGroup::GetHandleType() const
{
	return EGizmoHandleTypes::Rotate;
}