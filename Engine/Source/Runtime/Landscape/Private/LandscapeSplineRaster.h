// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class ULandscapeInfo;
class ULandscapeLayerInfoObject;
struct FLandscapeSplineInterpPoint;

namespace LandscapeSplineRaster
{
#if WITH_EDITOR
	bool FixSelfIntersection(TArray<FLandscapeSplineInterpPoint>& Points, FVector FLandscapeSplineInterpPoint::* Side);

	void Pointify(const FInterpCurveVector& SplineInfo, TArray<FLandscapeSplineInterpPoint>& OutPoints, int32 NumSubdivisions,
		float StartFalloffFraction, float EndFalloffFraction,
		const float StartWidth, const float EndWidth,
		const float StartSideFalloff, const float EndSideFalloff,
		const float StartRollDegrees, const float EndRollDegrees);

	void RasterizeSegmentPoints(ULandscapeInfo* LandscapeInfo, TArray<FLandscapeSplineInterpPoint> Points, const FTransform& SplineToWorld, bool bRaiseTerrain, bool bLowerTerrain, ULandscapeLayerInfoObject* LayerInfo);
#endif
}
