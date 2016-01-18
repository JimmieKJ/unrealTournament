// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShapeElem.h"
#include "SphereElem.generated.h"


/** Sphere shape used for collision */
USTRUCT()
struct FKSphereElem : public FKShapeElem
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FMatrix TM_DEPRECATED;

	UPROPERTY(Category=KSphereElem, VisibleAnywhere)
	FVector Center;

	UPROPERTY(Category=KSphereElem, VisibleAnywhere)
	float Radius;

	FKSphereElem() 
	: FKShapeElem(EAggCollisionShape::Sphere)
	, Center( FVector::ZeroVector )
	, Radius(1)
	{

	}

	FKSphereElem( float r ) 
	: FKShapeElem(EAggCollisionShape::Sphere)
	, Center( FVector::ZeroVector )
	, Radius(r)
	{

	}

	void Serialize( const FArchive& Ar );

	friend bool operator==( const FKSphereElem& LHS, const FKSphereElem& RHS )
	{
		return ( LHS.Center == RHS.Center &&
			LHS.Radius == RHS.Radius );
	}

	// Utility function that builds an FTransform from the current data
	FTransform GetTransform() const
	{
		return FTransform( Center );
	};

	void SetTransform(const FTransform& InTransform)
	{
		ensure(InTransform.IsValid());
		Center = InTransform.GetLocation();
	}

	FORCEINLINE float GetVolume(const FVector& Scale) const { return 1.3333f * PI * FMath::Pow(Radius * Scale.GetMin(), 3); }
	
	ENGINE_API void	DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FVector& Scale3D, const FColor Color) const;

	ENGINE_API void	DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FVector& Scale3D, const FMaterialRenderProxy* MaterialRenderProxy) const;
	ENGINE_API void GetElemSolid(const FTransform& ElemTM, const FVector& Scale3D, const FMaterialRenderProxy* MaterialRenderProxy, int32 ViewIndex, class FMeshElementCollector& Collector) const;
	ENGINE_API FBox CalcAABB(const FTransform& BoneTM, float Scale) const;

	ENGINE_API void ScaleElem(FVector DeltaSize, float MinSize);

	ENGINE_API static EAggCollisionShape::Type StaticShapeType;
};