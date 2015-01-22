// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysicsEngine/AggregateGeometry2D.h"

static const int32 DrawCollisionSides2D = 16;

//@TODO: PAPER2D: These axes should be renamed, and set when the Paper2D ones are loaded
FVector Box2D_AxisX(1.0f, 0.0f, 0.0f);
FVector Box2D_AxisY(0.0f, 0.0f, 1.0f);
FVector Box2D_AxisZ(0.0f, -1.0f, 0.0f);

//////////////////////////////////////////////////////////////////////////
// FCircleElement2D

float FCircleElement2D::GetArea(const FVector& Scale) const
{
	return PI * FMath::Square(Radius * Scale.Size());
}

// Note: ElemTM is assumed to have no scaling in it!
void FCircleElement2D::DrawElemWire(FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, float Scale, const FColor Color)
{
	const FVector ElemCenter = ElemTM.GetLocation();
	const FVector X = ElemTM.TransformVector(Box2D_AxisX);
	const FVector Y = ElemTM.TransformVector(Box2D_AxisY);

	DrawCircle(PDI, ElemCenter, X, Y, Color, Scale * Radius, DrawCollisionSides2D, SDPG_World);
}

void FCircleElement2D::DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, float Scale, const FMaterialRenderProxy* MaterialRenderProxy)
{
	const FVector ElemCenter = ElemTM.GetLocation();
	const FVector X = ElemTM.TransformVector(Box2D_AxisX);
	const FVector Y = ElemTM.TransformVector(Box2D_AxisY);

	DrawDisc(PDI, ElemCenter, X, Y, FColor::White, Scale * Radius, DrawCollisionSides2D, MaterialRenderProxy, SDPG_World);
}

//////////////////////////////////////////////////////////////////////////
// FBoxElement2D

float FBoxElement2D::GetArea(const FVector& Scale) const
{
	const float Scale1D = Scale.Size();
	return (Width * Scale1D) * (Height * Scale1D);
}

// Note: ElemTM is assumed to have no scaling in it!
void FBoxElement2D::DrawElemWire(FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, float Scale, const FColor Color)
{
	//@TODO: PAPER2D: Implement this method

// 	FVector	B[2], P, Q, Radii;
// 
// 	// X,Y,Z member variables are LENGTH not RADIUS
// 	Radii.X = Scale*0.5f*X;
// 	Radii.Y = Scale*0.5f*Y;
// 	Radii.Z = Scale*0.5f*Z;
// 
// 	B[0] = Radii; // max
// 	B[1] = -1.0f * Radii; // min
// 
// 	for (int32 i = 0; i < 2; i++)
// 	{
// 		for (int32 j = 0; j < 2; j++)
// 		{
// 			P.X = B[i].X; Q.X = B[i].X;
// 			P.Y = B[j].Y; Q.Y = B[j].Y;
// 			P.Z = B[0].Z; Q.Z = B[1].Z;
// 			PDI->DrawLine(ElemTM.TransformPosition(P), ElemTM.TransformPosition(Q), Color, SDPG_World);
// 
// 			P.Y = B[i].Y; Q.Y = B[i].Y;
// 			P.Z = B[j].Z; Q.Z = B[j].Z;
// 			P.X = B[0].X; Q.X = B[1].X;
// 			PDI->DrawLine(ElemTM.TransformPosition(P), ElemTM.TransformPosition(Q), Color, SDPG_World);
// 
// 			P.Z = B[i].Z; Q.Z = B[i].Z;
// 			P.X = B[j].X; Q.X = B[j].X;
// 			P.Y = B[0].Y; Q.Y = B[1].Y;
// 			PDI->DrawLine(ElemTM.TransformPosition(P), ElemTM.TransformPosition(Q), Color, SDPG_World);
// 		}
// 	}
}

void FBoxElement2D::DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, float Scale, const FMaterialRenderProxy* MaterialRenderProxy)
{
	//@TODO: PAPER2D: Implement this method
	// 	DrawBox(PDI, ElemTM.ToMatrixWithScale(), 0.5f * FVector(X, Y, Z), MaterialRenderProxy, SDPG_World);
}

//////////////////////////////////////////////////////////////////////////
// FConvexElement2D

float FConvexElement2D::GetArea(const FVector& Scale) const
{
	//@TODO: PAPER2D: Implement this method properly
	return 1.0f * Scale.Size();
}

void FConvexElement2D::DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FColor Color)
{
	//@TODO: PAPER2D: Implement this method
}

void FConvexElement2D::DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, float Scale, const FMaterialRenderProxy* MaterialRenderProxy)
{
	//@TODO: PAPER2D: Implement this method
}

//////////////////////////////////////////////////////////////////////////
// FAggregateGeometry2D

void FAggregateGeometry2D::EmptyElements()
{
	CircleElements.Empty();
	BoxElements.Empty();
	ConvexElements.Empty();
}

float FAggregateGeometry2D::GetArea(const FVector& Scale) const
{
	float SumArea = 0.0f;

	for (const FCircleElement2D& Circle : CircleElements)
	{
		SumArea += Circle.GetArea(Scale);
	}

	for (const FBoxElement2D& Box : BoxElements)
	{
		SumArea += Box.GetArea(Scale);
	}

	for (const FConvexElement2D& Convex : ConvexElements)
	{
		SumArea += Convex.GetArea(Scale);
	}

	return SumArea;
}
