// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LandscapeBlueprintSupport.cpp: Landscape blueprint functions
  =============================================================================*/

#include "Landscape.h"
#include "LandscapeSplineRaster.h"
#include "LandscapeProxy.h"
#include "LandscapeSplineRaster.h"
#include "Components/SplineComponent.h"
#include "LandscapeSplineSegment.h"

void ALandscapeProxy::EditorApplySpline(USplineComponent* InSplineComponent, float StartWidth, float EndWidth, float StartSideFalloff, float EndSideFalloff, float StartRoll, float EndRoll, int32 NumSubdivisions, bool bRaiseHeights, bool bLowerHeights, ULandscapeLayerInfoObject* PaintLayer)
{
	check(GIsEditor);

#if WITH_EDITOR
	TArray<FLandscapeSplineInterpPoint> Points;
	LandscapeSplineRaster::Pointify(InSplineComponent->SplineInfo, Points, NumSubdivisions, 0.0f, 0.0f, StartWidth, EndWidth, StartSideFalloff, EndSideFalloff, StartRoll, EndRoll);

	FTransform SplineToWorld = InSplineComponent->GetComponentTransform();
	LandscapeSplineRaster::RasterizeSegmentPoints(GetLandscapeInfo(), MoveTemp(Points), SplineToWorld, bRaiseHeights, bLowerHeights, PaintLayer);
#endif
}
