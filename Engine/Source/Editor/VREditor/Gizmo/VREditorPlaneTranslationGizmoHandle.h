// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VIGizmoHandle.h"
#include "VREditorPlaneTranslationGizmoHandle.generated.h"

/**
 * Gizmo handle for translating on a plane
 */
UCLASS()
class UVREditorPlaneTranslationGizmoHandleGroup : public UGizmoHandleGroup
{
	GENERATED_BODY()

public:

	/** Default constructor that sets up CDO properties */
	UVREditorPlaneTranslationGizmoHandleGroup();
	
	/** Updates the gizmo handles */
	void UpdateGizmoHandleGroup(const FTransform& LocalToWorld, const FBox& LocalBounds, const FVector ViewLocation, bool bAllHandlesVisible, class UActorComponent* DraggingHandle,
		const TArray< UActorComponent* >& HoveringOverHandles, float AnimationAlpha, float GizmoScale, const float GizmoHoverScale, const float GizmoHoverAnimationDuration, bool& bOutIsHoveringOrDraggingThisHandleGroup ) override;

	/** Gets the InteractionType for this Gizmo handle */
	virtual ETransformGizmoInteractionType GetInteractionType() const override;

	/** Gets the GizmoType for this Gizmo handle */
	virtual EGizmoHandleTypes GetHandleType() const override;
};