// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShapeElem.h"
#include "ConvexElem.generated.h"

struct FDynamicMeshVertex;

namespace physx
{
	class PxConvexMesh;
}

/** One convex hull, used for simplified collision. */
USTRUCT()
struct FKConvexElem : public FKShapeElem
{
	GENERATED_USTRUCT_BODY()

	/** Array of indices that make up the convex hull. */
	UPROPERTY()
	TArray<FVector> VertexData;

	/** Bounding box of this convex hull. */
	UPROPERTY()
		FBox ElemBox;

	/** Transform of this element */
	UPROPERTY()
		FTransform Transform;

	/** Convex mesh for this body, created from cooked data in CreatePhysicsMeshes */
	physx::PxConvexMesh*   ConvexMesh;

	/** Convex mesh for this body, flipped across X, created from cooked data in CreatePhysicsMeshes */
	physx::PxConvexMesh*   ConvexMeshNegX;

	FKConvexElem()
		: FKShapeElem(EAggCollisionShape::Convex)
		, ElemBox(0)
		, Transform(FTransform::Identity)
		, ConvexMesh(NULL)
		, ConvexMeshNegX(NULL)
	{}

	DEPRECATED(4.8, "Please call DrawElemWire which takes in a Scale parameter")
	ENGINE_API void	DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FColor Color) const;
	
	ENGINE_API void	DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const float Scale, const FColor Color) const;

	ENGINE_API void AddCachedSolidConvexGeom(TArray<FDynamicMeshVertex>& VertexBuffer, TArray<int32>& IndexBuffer, const FColor VertexColor) const;

	/** Reset the hull to empty all arrays */
	ENGINE_API void	Reset();

	/** Updates internal ElemBox based on current value of VertexData */
	ENGINE_API void	UpdateElemBox();

	/** Calculate a bounding box for this convex element with the specified transform and scale */
	ENGINE_API FBox	CalcAABB(const FTransform& BoneTM, const FVector& Scale3D) const;

	/** Get set of planes that define this convex hull */
	ENGINE_API void GetPlanes(TArray<FPlane>& Planes) const;

	/** Utility for creating a convex hull from a set of planes. Will reset current state of this elem. */
	ENGINE_API bool HullFromPlanes(const TArray<FPlane>& InPlanes, const TArray<FVector>& SnapVerts);

	/** Returns the volume of this element */
	float GetVolume(const FVector& Scale) const;

	FTransform GetTransform() const
	{
		return Transform;
	};

	void SetTransform(const FTransform& InTransform)
	{
		ensure(InTransform.IsValid());
		Transform = InTransform;
	}

	friend FArchive& operator<<(FArchive& Ar, FKConvexElem& Elem);

	ENGINE_API void ScaleElem(FVector DeltaSize, float MinSize);

	ENGINE_API static EAggCollisionShape::Type StaticShapeType;
};