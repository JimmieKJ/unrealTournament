// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Box.cpp: Implements the FBox class.
=============================================================================*/

#include "CorePrivatePCH.h"


/* FBox structors
 *****************************************************************************/

FBox::FBox(const FVector* Points, int32 Count)
	: Min(0, 0, 0)
	, Max(0, 0, 0)
	, IsValid(0)
{
	for (int32 i = 0; i < Count; i++)
	{
		*this += Points[i];
	}
}


FBox::FBox(const TArray<FVector>& Points)
	: Min(0, 0, 0)
	, Max(0, 0, 0)
	, IsValid(0)
{
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		*this += Points[i];
	}
}


/* FBox interface
 *****************************************************************************/

FBox FBox::TransformBy(const FMatrix& M) const
{
	// if we are not valid, return another invalid box.
	if (!IsValid)
	{
		return FBox(0);
	}

	VectorRegister Vertices[8];

	VectorRegister m0 = VectorLoadAligned(M.M[0]);
	VectorRegister m1 = VectorLoadAligned(M.M[1]);
	VectorRegister m2 = VectorLoadAligned(M.M[2]);
	VectorRegister m3 = VectorLoadAligned(M.M[3]);

	Vertices[0] = VectorLoadFloat3(&Min);
	Vertices[1] = VectorSetFloat3(Min.X, Min.Y, Max.Z);
	Vertices[2] = VectorSetFloat3(Min.X, Max.Y, Min.Z);
	Vertices[3] = VectorSetFloat3(Max.X, Min.Y, Min.Z);
	Vertices[4] = VectorSetFloat3(Max.X, Max.Y, Min.Z);
	Vertices[5] = VectorSetFloat3(Max.X, Min.Y, Max.Z);
	Vertices[6] = VectorSetFloat3(Min.X, Max.Y, Max.Z);
	Vertices[7] = VectorLoadFloat3(&Max);

	VectorRegister r0 = VectorMultiply(VectorReplicate(Vertices[0],0), m0);
	VectorRegister r1 = VectorMultiply(VectorReplicate(Vertices[1],0), m0);
	VectorRegister r2 = VectorMultiply(VectorReplicate(Vertices[2],0), m0);
	VectorRegister r3 = VectorMultiply(VectorReplicate(Vertices[3],0), m0);
	VectorRegister r4 = VectorMultiply(VectorReplicate(Vertices[4],0), m0);
	VectorRegister r5 = VectorMultiply(VectorReplicate(Vertices[5],0), m0);
	VectorRegister r6 = VectorMultiply(VectorReplicate(Vertices[6],0), m0);
	VectorRegister r7 = VectorMultiply(VectorReplicate(Vertices[7],0), m0);

	r0 = VectorMultiplyAdd( VectorReplicate(Vertices[0],1), m1, r0);
	r1 = VectorMultiplyAdd( VectorReplicate(Vertices[1],1), m1, r1);
	r2 = VectorMultiplyAdd( VectorReplicate(Vertices[2],1), m1, r2);
	r3 = VectorMultiplyAdd( VectorReplicate(Vertices[3],1), m1, r3);
	r4 = VectorMultiplyAdd( VectorReplicate(Vertices[4],1), m1, r4);
	r5 = VectorMultiplyAdd( VectorReplicate(Vertices[5],1), m1, r5);
	r6 = VectorMultiplyAdd( VectorReplicate(Vertices[6],1), m1, r6);
	r7 = VectorMultiplyAdd( VectorReplicate(Vertices[7],1), m1, r7);

	r0 = VectorMultiplyAdd( VectorReplicate(Vertices[0],2), m2, r0);
	r1 = VectorMultiplyAdd( VectorReplicate(Vertices[1],2), m2, r1);
	r2 = VectorMultiplyAdd( VectorReplicate(Vertices[2],2), m2, r2);
	r3 = VectorMultiplyAdd( VectorReplicate(Vertices[3],2), m2, r3);
	r4 = VectorMultiplyAdd( VectorReplicate(Vertices[4],2), m2, r4);
	r5 = VectorMultiplyAdd( VectorReplicate(Vertices[5],2), m2, r5);
	r6 = VectorMultiplyAdd( VectorReplicate(Vertices[6],2), m2, r6);
	r7 = VectorMultiplyAdd( VectorReplicate(Vertices[7],2), m2, r7);

	r0 = VectorAdd(r0, m3);
	r1 = VectorAdd(r1, m3);
	r2 = VectorAdd(r2, m3);
	r3 = VectorAdd(r3, m3);
	r4 = VectorAdd(r4, m3);
	r5 = VectorAdd(r5, m3);
	r6 = VectorAdd(r6, m3);
	r7 = VectorAdd(r7, m3);

	FBox NewBox;
	
	VectorRegister min0 = VectorMin(r0, r1);
	VectorRegister min1 = VectorMin(r2, r3);
	VectorRegister min2 = VectorMin(r4, r5);
	VectorRegister min3 = VectorMin(r6, r7);
	VectorRegister max0 = VectorMax(r0, r1);
	VectorRegister max1 = VectorMax(r2, r3);
	VectorRegister max2 = VectorMax(r4, r5);
	VectorRegister max3 = VectorMax(r6, r7);
	
	min0 = VectorMin(min0, min1);
	min1 = VectorMin(min2, min3);
	max0 = VectorMax(max0, max1);
	max1 = VectorMax(max2, max3);
	min0 = VectorMin(min0, min1);
	max0 = VectorMax(max0, max1);

	VectorStoreFloat3(min0, &NewBox.Min);
	VectorStoreFloat3(max0, &NewBox.Max);

	NewBox.IsValid = 1;

	return NewBox;
}

FBox FBox::TransformBy(const FTransform& M) const
{
	return TransformBy(M.ToMatrixWithScale());
}

FBox FBox::InverseTransformBy(const FTransform& M) const
{
	FVector Vertices[8] = 
	{
		FVector(Min),
		FVector(Min.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Min.Z),
		FVector(Max.X, Min.Y, Min.Z),
		FVector(Max.X, Max.Y, Min.Z),
		FVector(Max.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Max.Z),
		FVector(Max)
	};

	FBox NewBox(0);

	for (int32 VertexIndex = 0; VertexIndex < ARRAY_COUNT(Vertices); VertexIndex++)
	{
		FVector4 ProjectedVertex = M.InverseTransformPosition(Vertices[VertexIndex]);
		NewBox += ProjectedVertex;
	}

	return NewBox;
}


FBox FBox::TransformProjectBy(const FMatrix& ProjM) const
{
	FVector Vertices[8] = 
	{
		FVector(Min),
		FVector(Min.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Min.Z),
		FVector(Max.X, Min.Y, Min.Z),
		FVector(Max.X, Max.Y, Min.Z),
		FVector(Max.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Max.Z),
		FVector(Max)
	};

	FBox NewBox(0);

	for (int32 VertexIndex = 0; VertexIndex < ARRAY_COUNT(Vertices); VertexIndex++)
	{
		FVector4 ProjectedVertex = ProjM.TransformPosition(Vertices[VertexIndex]);
		NewBox += ((FVector)ProjectedVertex) / ProjectedVertex.W;
	}

	return NewBox;
}
