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

	const VectorRegister VecOrigin = VectorLoadFloat3(&Origin);
	const VectorRegister VecExtent = VectorLoadFloat3(&BoxExtent);

	const VectorRegister m0 = VectorLoadAligned(M.M[0]);
	const VectorRegister m1 = VectorLoadAligned(M.M[1]);
	const VectorRegister m2 = VectorLoadAligned(M.M[2]);
	const VectorRegister m3 = VectorLoadAligned(M.M[3]);

	VectorRegister NewOrigin = VectorMultiply(VectorReplicate(VecOrigin, 0), m0);
	NewOrigin = VectorMultiplyAdd(VectorReplicate(VecOrigin, 1), m1, NewOrigin);
	NewOrigin = VectorMultiplyAdd(VectorReplicate(VecOrigin, 2), m2, NewOrigin);
	NewOrigin = VectorAdd(NewOrigin, m3);

	VectorRegister NewExtent = VectorAbs(VectorMultiply(VectorReplicate(VecExtent, 0), m0));
	NewExtent = VectorAdd(NewExtent, VectorAbs(VectorMultiply(VectorReplicate(VecExtent, 1), m1)));
	NewExtent = VectorAdd(NewExtent, VectorAbs(VectorMultiply(VectorReplicate(VecExtent, 2), m2)));

	VectorStoreFloat3(NewExtent, &Result.BoxExtent);
	VectorStoreFloat3(NewOrigin, &Result.Origin);

	VectorRegister MaxRadius = VectorMultiply(m0, m0);
	MaxRadius = VectorMultiplyAdd(m1, m1, MaxRadius);
	MaxRadius = VectorMultiplyAdd(m2, m2, MaxRadius);
	MaxRadius = VectorMax(VectorMax(MaxRadius, VectorReplicate(MaxRadius, 1)), VectorReplicate(MaxRadius, 2));
	Result.SphereRadius = FMath::Sqrt(VectorGetComponent( MaxRadius, 0) ) * SphereRadius;

	return Result;
}

FBoxSphereBounds FBoxSphereBounds::TransformBy(const FTransform& M) const
{
	const FMatrix Mat = M.ToMatrixWithScale();
	FBoxSphereBounds Result = TransformBy(Mat);
	return Result;
}