// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BoxSphereBounds.cpp: Implements the FBoxSphereBounds structure.
=============================================================================*/

#include "CorePrivatePCH.h"


/* FBoxSphereBounds interface
 *****************************************************************************/

FBoxSphereBounds FBoxSphereBounds::TransformBy(const FMatrix& M) const
{
	FBoxSphereBounds Result;

	Result.Origin = M.TransformPosition(Origin);
	Result.BoxExtent = FVector::ZeroVector;

	float Signs[2] = { -1.0f, 1.0f };

	for (int32 X = 0; X < 2; X++)
	{
		for (int32 Y = 0; Y < 2; Y++)
		{
			for (int32 Z = 0; Z < 2; Z++)
			{
				FVector	Corner = M.TransformVector(FVector(Signs[X] * BoxExtent.X,Signs[Y] * BoxExtent.Y,Signs[Z] * BoxExtent.Z));

				Result.BoxExtent.X = FMath::Max(Corner.X,Result.BoxExtent.X);
				Result.BoxExtent.Y = FMath::Max(Corner.Y,Result.BoxExtent.Y);
				Result.BoxExtent.Z = FMath::Max(Corner.Z,Result.BoxExtent.Z);
			}
		}
	}

	const FVector XAxis(M.M[0][0], M.M[0][1], M.M[0][2]);
	const FVector YAxis(M.M[1][0], M.M[1][1], M.M[1][2]);
	const FVector ZAxis(M.M[2][0], M.M[2][1], M.M[2][2]);

	Result.SphereRadius = FMath::Sqrt(FMath::Max(XAxis | XAxis, FMath::Max(YAxis | YAxis, ZAxis | ZAxis))) * SphereRadius;

	return Result;
}


FBoxSphereBounds FBoxSphereBounds::TransformBy(const FTransform& M) const
{
	FBoxSphereBounds Result;

	Result.Origin = M.TransformPosition(Origin);
	Result.BoxExtent = FVector::ZeroVector;

	float Signs[2] = { -1.0f, 1.0f };

	for (int32 X = 0; X < 2; X++)
	{
		for (int32 Y = 0; Y < 2; Y++)
		{
			for (int32 Z = 0; Z < 2; Z++)
			{
				FVector	Corner = M.TransformVector(FVector(Signs[X] * BoxExtent.X,Signs[Y] * BoxExtent.Y,Signs[Z] * BoxExtent.Z));

				Result.BoxExtent.X = FMath::Max(Corner.X, Result.BoxExtent.X);
				Result.BoxExtent.Y = FMath::Max(Corner.Y, Result.BoxExtent.Y);
				Result.BoxExtent.Z = FMath::Max(Corner.Z, Result.BoxExtent.Z);
			}
		}
	}

	Result.SphereRadius = SphereRadius * M.GetMaximumAxisScale();

	return Result;
}