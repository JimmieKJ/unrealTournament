// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Sphere.cpp: Implements the FSphere class.
=============================================================================*/

#include "CorePrivatePCH.h"


/* FSphere structors
 *****************************************************************************/

FSphere::FSphere(const FVector* Pts, int32 Count)
	: Center(0, 0, 0)
	, W(0)
{
	if (Count)
	{
		const FBox Box(Pts, Count);

		*this = FSphere((Box.Min + Box.Max) / 2, 0);

		for (int32 i = 0; i < Count; i++)
		{
			const float Dist = FVector::DistSquared(Pts[i], Center);

			if (Dist > W)
			{
				W = Dist;
			}
		}

		W = FMath::Sqrt(W) * 1.001f;
	}
}


/* FSphere interface
 *****************************************************************************/

FSphere FSphere::TransformBy(const FMatrix& M) const
{
	FSphere	Result;

	Result.Center = M.TransformPosition(this->Center);

	const FVector XAxis(M.M[0][0], M.M[0][1], M.M[0][2]);
	const FVector YAxis(M.M[1][0], M.M[1][1], M.M[1][2]);
	const FVector ZAxis(M.M[2][0], M.M[2][1], M.M[2][2]);

	Result.W = FMath::Sqrt(FMath::Max(XAxis | XAxis, FMath::Max(YAxis | YAxis, ZAxis | ZAxis))) * W;

	return Result;
}


FSphere FSphere::TransformBy(const FTransform& M) const
{
	FSphere	Result;

	Result.Center = M.TransformPosition(this->Center);
	Result.W = M.GetMaximumAxisScale();

	return Result;
}