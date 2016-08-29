// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShapeElem.h"
#include "SphylElem.generated.h"

class FMaterialRenderProxy;
class FPrimitiveDrawInterface;

/** Capsule shape used for collision */
USTRUCT()
struct FKSphylElem : public FKShapeElem
{
	GENERATED_USTRUCT_BODY()

	/** The transform assumes the sphyl axis points down Z. */
	UPROPERTY()
	FMatrix TM_DEPRECATED;

	/** Position of the capsule's origin */
	UPROPERTY(Category=Capsule, EditAnywhere)
	FVector Center;

	/** Orientation of the capsule */
	UPROPERTY(Category= Capsule, EditAnywhere)
	FQuat Orientation;

	/** Radius of the capsule */
	UPROPERTY(Category= Capsule, EditAnywhere)
	float Radius;

	/** This is of line-segment ie. add Radius to both ends to find total length. */
	UPROPERTY(Category= Capsule, EditAnywhere)
	float Length;

	FKSphylElem()
	: FKShapeElem(EAggCollisionShape::Sphyl)
	, Center( FVector::ZeroVector )
	, Orientation( FQuat::Identity )
	, Radius(1), Length(1)

	{

	}

	FKSphylElem( float InRadius, float InLength )
	: FKShapeElem(EAggCollisionShape::Sphyl)
	, Center( FVector::ZeroVector )
	, Orientation( FQuat::Identity )
	, Radius(InRadius), Length(InLength)
	{

	}

	void Serialize( const FArchive& Ar );

	friend bool operator==( const FKSphylElem& LHS, const FKSphylElem& RHS )
	{
		return ( LHS.Center == RHS.Center &&
			LHS.Orientation == RHS.Orientation &&
			LHS.Radius == RHS.Radius &&
			LHS.Length == RHS.Length );
	};

	// Utility function that builds an FTransform from the current data
	FTransform GetTransform() const
	{
		return FTransform( Orientation, Center );
	};

	void SetTransform( const FTransform& InTransform )
	{
		ensure(InTransform.IsValid());
		Orientation = InTransform.GetRotation();
		Center = InTransform.GetLocation();
	}

	FORCEINLINE float GetVolume(const FVector& Scale) const { float ScaledRadius = Radius * Scale.GetMin(); return PI * FMath::Square(ScaledRadius) * ( 1.3333f * ScaledRadius + (Length * Scale.GetMin())); }

	ENGINE_API void	DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FVector& Scale3D, const FColor Color) const;
	ENGINE_API void GetElemSolid(const FTransform& ElemTM, const FVector& Scale3D, const FMaterialRenderProxy* MaterialRenderProxy, int32 ViewIndex, FMeshElementCollector& Collector) const;
	ENGINE_API void	DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, const FVector& Scale3D, const FMaterialRenderProxy* MaterialRenderProxy) const;
	ENGINE_API FBox CalcAABB(const FTransform& BoneTM, float Scale) const;

	ENGINE_API void ScaleElem(FVector DeltaSize, float MinSize);

	ENGINE_API FKSphylElem GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const;

	/**	
	 * Finds the shortest distance between the element and a world position. Input and output are given in world space
	 * @param	WorldPosition	The point we are trying to get close to
	 * @param	BodyToWorldTM	The transform to convert BodySetup into world space
	 * @return					The distance between WorldPosition and the shape. 0 indicates WorldPosition is inside one of the shapes.
	 */

	ENGINE_API float GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const;

	/**	
	 * Finds the closest point on the shape given a world position. Input and output are given in world space
	 * @param	WorldPosition			The point we are trying to get close to
	 * @param	BodyToWorldTM			The transform to convert BodySetup into world space
	 * @param	ClosestWorldPosition	The closest point on the shape in world space
	 * @param	Normal					The normal of the feature associated with ClosestWorldPosition.
	 * @return							The distance between WorldPosition and the shape. 0 indicates WorldPosition is inside the shape.
	 */
	ENGINE_API float GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const;

	ENGINE_API static EAggCollisionShape::Type StaticShapeType;
};
