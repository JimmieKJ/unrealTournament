// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshBuild.h: Contains commonly used functions and classes for building
	mesh data into engine usable form.

=============================================================================*/

#pragma once

/**
 * Returns true if the specified points are about equal
 */
inline bool PointsEqual(const FVector& V1,const FVector& V2, bool bUseEpsilonCompare = true )
{
	const float Epsilon = bUseEpsilonCompare ? THRESH_POINTS_ARE_SAME : 0.0f;
	if( FMath::Abs(V1.X - V2.X) > Epsilon || FMath::Abs(V1.Y - V2.Y) > Epsilon || FMath::Abs(V1.Z - V2.Z) > Epsilon )
	{
		return false;
	}

	return true;
}


/**
 * Returns true if the specified normal vectors are about equal
 */
inline bool NormalsEqual(const FVector& V1,const FVector& V2)
{
	const float Epsilon = THRESH_NORMALS_ARE_SAME;
	if( FMath::Abs(V1.X - V2.X) > Epsilon || FMath::Abs(V1.Y - V2.Y) > Epsilon || FMath::Abs(V1.Z - V2.Z) > Epsilon )
	{
		return false;
	}

	return true;
}
