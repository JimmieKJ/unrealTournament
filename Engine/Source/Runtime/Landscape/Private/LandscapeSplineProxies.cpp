// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LandscapePrivatePCH.h"
#include "Landscape.h"
#include "LandscapeSplineProxies.h"
#include "LandscapeSplineSegment.h"

LANDSCAPE_API void HLandscapeSplineProxy_Tangent::Serialize(FArchive& Ar)
{
	Ar << SplineSegment;
}