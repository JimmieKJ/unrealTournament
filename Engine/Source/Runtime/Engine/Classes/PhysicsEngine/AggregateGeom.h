// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShapeElem.h"
#include "ConvexElem.h"
#include "BoxElem.h"
#include "SphereElem.h"
#include "SphylElem.h"
#include "AggregateGeom.generated.h"

class FMaterialRenderProxy;
class FPrimitiveDrawInterface;


/** Container for an aggregate of collision shapes */
USTRUCT()
struct ENGINE_API FKAggregateGeom
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, editfixedsize, Category = KAggregateGeom)
	TArray<FKSphereElem> SphereElems;

	UPROPERTY(EditAnywhere, editfixedsize, Category = KAggregateGeom)
	TArray<FKBoxElem> BoxElems;

	UPROPERTY(EditAnywhere, editfixedsize, Category = KAggregateGeom)
	TArray<FKSphylElem> SphylElems;

	UPROPERTY(EditAnywhere, editfixedsize, Category = KAggregateGeom)
	TArray<FKConvexElem> ConvexElems;

	class FKConvexGeomRenderInfo* RenderInfo;

	FKAggregateGeom()
		: RenderInfo(NULL)
	{}
	int32 GetElementCount() const
	{
		return SphereElems.Num() + SphylElems.Num() + BoxElems.Num() + ConvexElems.Num();
	}

	int32 GetElementCount(int32 Type) const;

	void EmptyElements()
	{
		BoxElems.Empty();
		ConvexElems.Empty();
		SphylElems.Empty();
		SphereElems.Empty();

		FreeRenderInfo();
	}



	void Serialize(const FArchive& Ar);

	void GetAggGeom(const FTransform& Transform, const FColor Color, const FMaterialRenderProxy* MatInst, bool bPerHullColor, bool bDrawSolid, bool bUseEditorDepthTest, int32 ViewIndex, class FMeshElementCollector& Collector) const;

	/** Release the RenderInfo (if its there) and safely clean up any resources. Call on the game thread. */
	void FreeRenderInfo();

	FBox CalcAABB(const FTransform& Transform) const;

	/**
	* Calculates a tight box-sphere bounds for the aggregate geometry; this is more expensive than CalcAABB
	* (tight meaning the sphere may be smaller than would be required to encompass the AABB, but all individual components lie within both the box and the sphere)
	*
	* @param Output The output box-sphere bounds calculated for this set of aggregate geometry
	*	@param LocalToWorld Transform
	*/
	void CalcBoxSphereBounds(FBoxSphereBounds& Output, const FTransform& LocalToWorld) const;

	/** Returns the volume of this element */
	float GetVolume(const FVector& Scale3D) const;
};