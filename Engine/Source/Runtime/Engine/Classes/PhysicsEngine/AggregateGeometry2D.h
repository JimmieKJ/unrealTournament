// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AggregateGeometry2D.generated.h"


/** One convex hull, used for simplified collision. */
USTRUCT()
struct ENGINE_API FConvexElement2D
{
	GENERATED_USTRUCT_BODY()

	// Vertices that lie on the convex hull
	UPROPERTY()

	TArray<FVector2D> VertexData;

	FConvexElement2D()
	{}

	void DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FColor Color);
	void DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, float Scale, const FMaterialRenderProxy* MaterialRenderProxy);

	// Returns the area of this element
	float GetArea(const FVector& Scale) const;
};

/** Sphere shape used for collision */
USTRUCT()
struct ENGINE_API FCircleElement2D
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(Category=KSphereElem, VisibleAnywhere)
	FVector2D Center;

	UPROPERTY(Category=KSphereElem, VisibleAnywhere)
	float Radius;

public:
	FCircleElement2D()
		: Center(FVector2D::ZeroVector)
		, Radius(1.0f)
	{

	}

	FCircleElement2D(float InitialRadius)
		: Center(FVector2D::ZeroVector)
		, Radius(InitialRadius)
	{
	}

	friend bool operator==(const FCircleElement2D& LHS, const FCircleElement2D& RHS)
	{
		return (LHS.Center == RHS.Center) && (LHS.Radius == RHS.Radius);
	}

	// Returns the area of this element
	float GetArea(const FVector& Scale) const;
	
	void DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, float Scale, const FColor Color);
	void DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, float Scale, const FMaterialRenderProxy* MaterialRenderProxy);
};

/** Box shape used for collision */
USTRUCT()
struct ENGINE_API FBoxElement2D
{
	GENERATED_USTRUCT_BODY()

	// Center of the box
	UPROPERTY(Category=KBoxElem, VisibleAnywhere)
	FVector2D Center;

	// Width of the box
	UPROPERTY(Category=KBoxElem, VisibleAnywhere)
	float Width;

	// Height of the box
	UPROPERTY(Category=KBoxElem, VisibleAnywhere)
	float Height;

	// Rotation of the box (in degrees)
	UPROPERTY(Category = KBoxElem, VisibleAnywhere)
	float Angle;

	FBoxElement2D()
		: Center(FVector::ZeroVector)
		, Width(1.0f)
		, Height(1.0f)
		, Angle(0.0f)
	{
	}

	friend bool operator==(const FBoxElement2D& LHS, const FBoxElement2D& RHS)
	{
		return (LHS.Center == RHS.Center) &&
			(LHS.Width == RHS.Width) &&
			(LHS.Height == RHS.Height) &&
			(LHS.Angle == RHS.Angle);
	};

	// Returns the area of this element
	float GetArea(const FVector& Scale) const;

	void DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, float Scale, const FColor Color);
	void DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, float Scale, const FMaterialRenderProxy* MaterialRenderProxy);
};


// Container for an aggregate of 2D collision shapes
USTRUCT()
struct ENGINE_API FAggregateGeometry2D
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, editfixedsize, Category = KAggregateGeom)
	TArray<struct FCircleElement2D> CircleElements;

	UPROPERTY(EditAnywhere, editfixedsize, Category = KAggregateGeom)
	TArray<struct FBoxElement2D> BoxElements;

	UPROPERTY(EditAnywhere, editfixedsize, Category = KAggregateGeom)
	TArray<struct FConvexElement2D> ConvexElements;

public:
	void EmptyElements();

	// Returns the area of all contained elements
 	float GetArea(const FVector& Scale) const;
};
