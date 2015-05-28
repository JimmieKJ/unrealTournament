// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Camera/CameraStackTypes.h"
#include "UnrealClient.h"
#include "SceneView.h"

//////////////////////////////////////////////////////////////////////////
// FMinimalViewInfo

bool FMinimalViewInfo::Equals(const FMinimalViewInfo& OtherInfo) const
{
	return 
		(Location == OtherInfo.Location) &&
		(Rotation == OtherInfo.Rotation) &&
		(FOV == OtherInfo.FOV) &&
		(OrthoWidth == OtherInfo.OrthoWidth) &&
		(OrthoNearClipPlane == OtherInfo.OrthoNearClipPlane) &&
		(OrthoFarClipPlane == OtherInfo.OrthoFarClipPlane) &&
		(AspectRatio == OtherInfo.AspectRatio) &&
		(bConstrainAspectRatio == OtherInfo.bConstrainAspectRatio) &&
		(bUseFieldOfViewForLOD == OtherInfo.bUseFieldOfViewForLOD) &&
		(ProjectionMode == OtherInfo.ProjectionMode);
}

void FMinimalViewInfo::BlendViewInfo(FMinimalViewInfo& OtherInfo, float OtherWeight)
{
	Location = FMath::Lerp(Location, OtherInfo.Location, OtherWeight);

	const FRotator DeltaAng = (OtherInfo.Rotation - Rotation).GetNormalized();
	Rotation = Rotation + OtherWeight * DeltaAng;

	FOV = FMath::Lerp(FOV, OtherInfo.FOV, OtherWeight);
	OrthoWidth = FMath::Lerp(OrthoWidth, OtherInfo.OrthoWidth, OtherWeight);
	OrthoNearClipPlane = FMath::Lerp(OrthoNearClipPlane, OtherInfo.OrthoNearClipPlane, OtherWeight);
	OrthoFarClipPlane = FMath::Lerp(OrthoFarClipPlane, OtherInfo.OrthoFarClipPlane, OtherWeight);

	AspectRatio = FMath::Lerp(AspectRatio, OtherInfo.AspectRatio, OtherWeight);
	bConstrainAspectRatio |= OtherInfo.bConstrainAspectRatio;
	bUseFieldOfViewForLOD |= OtherInfo.bUseFieldOfViewForLOD;
}

void FMinimalViewInfo::ApplyBlendWeight(const float& Weight)
{
	Location *= Weight;
	Rotation.Normalize();
	Rotation *= Weight;
	FOV *= Weight;
	OrthoWidth *= Weight;
	OrthoNearClipPlane *= Weight;
	OrthoFarClipPlane *= Weight;
	AspectRatio *= Weight;
}

void FMinimalViewInfo::AddWeightedViewInfo(const FMinimalViewInfo& OtherView, const float& Weight)
{
	FMinimalViewInfo OtherViewWeighted = OtherView;
	OtherViewWeighted.ApplyBlendWeight(Weight);

	Location += OtherViewWeighted.Location;
	Rotation += OtherViewWeighted.Rotation;
	FOV += OtherViewWeighted.FOV;
	OrthoWidth += OtherViewWeighted.OrthoWidth;
	OrthoNearClipPlane += OtherViewWeighted.OrthoNearClipPlane;
	OrthoFarClipPlane += OtherViewWeighted.OrthoFarClipPlane;
	AspectRatio += OtherViewWeighted.AspectRatio;

	bConstrainAspectRatio |= OtherViewWeighted.bConstrainAspectRatio;
	bUseFieldOfViewForLOD |= OtherViewWeighted.bUseFieldOfViewForLOD;
}

void FMinimalViewInfo::CalculateProjectionMatrixGivenView(const FMinimalViewInfo& ViewInfo, TEnumAsByte<enum EAspectRatioAxisConstraint> AspectRatioAxisConstraint, FViewport* Viewport, FSceneViewProjectionData& InOutProjectionData)
{
	// Create the projection matrix (and possibly constrain the view rectangle)
	if (ViewInfo.bConstrainAspectRatio)
	{			
		// Enforce a particular aspect ratio for the render of the scene. 
		// Results in black bars at top/bottom etc.
		InOutProjectionData.SetConstrainedViewRectangle(Viewport->CalculateViewExtents(ViewInfo.AspectRatio, InOutProjectionData.GetViewRect()));

		//
		if (ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic)
		{
			const float YScale = 1.0f / ViewInfo.AspectRatio;
	
			const float OrthoWidth = ViewInfo.OrthoWidth / 2.0f;
			const float OrthoHeight = ViewInfo.OrthoWidth / 2.0f * YScale;

			const float NearPlane = ViewInfo.OrthoNearClipPlane;
			const float FarPlane = ViewInfo.OrthoFarClipPlane;

			const float ZScale = 1.0f / (FarPlane - NearPlane);
			const float ZOffset = -NearPlane;

			InOutProjectionData.ProjectionMatrix = FReversedZOrthoMatrix(
				OrthoWidth,
				OrthoHeight,
				ZScale,
				ZOffset
				);
		}
		else
		{
			// Avoid divide by zero in the projection matrix calculation by clamping FOV
			InOutProjectionData.ProjectionMatrix = FReversedZPerspectiveMatrix(
				FMath::Max(0.001f, ViewInfo.FOV) * (float)PI / 360.0f,
				ViewInfo.AspectRatio,
				1.0f,
				GNearClippingPlane );
		}
	}
	else 
	{
		// Avoid divide by zero in the projection matrix calculation by clamping FOV
		float MatrixFOV = FMath::Max(0.001f, ViewInfo.FOV) * (float)PI / 360.0f;
		float XAxisMultiplier;
		float YAxisMultiplier;

		const FIntRect& ViewRect = InOutProjectionData.GetViewRect();
		const int32 SizeX = ViewRect.Width();
		const int32 SizeY = ViewRect.Height();

		// if x is bigger, and we're respecting x or major axis, AND mobile isn't forcing us to be Y axis aligned
		if (((SizeX > SizeY) && (AspectRatioAxisConstraint == AspectRatio_MajorAxisFOV)) || (AspectRatioAxisConstraint == AspectRatio_MaintainXFOV) || (ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic))
		{
			//if the viewport is wider than it is tall
			XAxisMultiplier = 1.0f;
			YAxisMultiplier = SizeX / (float)SizeY;
		}
		else
		{
			//if the viewport is taller than it is wide
			XAxisMultiplier = SizeY / (float)SizeX;
			YAxisMultiplier = 1.0f;
		}
	
		if (ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic)
		{
			const float OrthoWidth = ViewInfo.OrthoWidth / 2.0f * XAxisMultiplier;
			const float OrthoHeight = (ViewInfo.OrthoWidth / 2.0f) / YAxisMultiplier;

			const float NearPlane = ViewInfo.OrthoNearClipPlane;
			const float FarPlane = ViewInfo.OrthoFarClipPlane;

			const float ZScale = 1.0f / (FarPlane - NearPlane);
			const float ZOffset = -NearPlane;

			InOutProjectionData.ProjectionMatrix = FReversedZOrthoMatrix(
				OrthoWidth, 
				OrthoHeight,
				ZScale,
				ZOffset
				);		
		}
		else
		{
			InOutProjectionData.ProjectionMatrix = FReversedZPerspectiveMatrix(
				MatrixFOV,
				MatrixFOV,
				XAxisMultiplier,
				YAxisMultiplier,
				GNearClippingPlane,
				GNearClippingPlane
				);
		}
	}
}
