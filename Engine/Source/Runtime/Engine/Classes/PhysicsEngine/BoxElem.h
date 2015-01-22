// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoxElem.generated.h"

class FMeshElementCollector;

/** Box shape used for collision */
USTRUCT()
struct FKBoxElem
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FMatrix TM_DEPRECATED;

	UPROPERTY(Category=KBoxElem, VisibleAnywhere)
	FVector Center;

	UPROPERTY(Category=KBoxElem, VisibleAnywhere)
	FQuat Orientation;

	UPROPERTY(Category=KBoxElem, VisibleAnywhere)
	float X;

	UPROPERTY(Category=KBoxElem, VisibleAnywhere)
	float Y;

	/** length (not radius) */
	UPROPERTY(Category=KBoxElem, VisibleAnywhere)
	float Z;


	FKBoxElem()
	: Center( FVector::ZeroVector )
	, Orientation( FQuat::Identity )
	, X(1), Y(1), Z(1)
	{

	}

	FKBoxElem( float s )
	: Center( FVector::ZeroVector )
	, Orientation( FQuat::Identity )
	, X(s), Y(s), Z(s)
	{

	}

	FKBoxElem( float InX, float InY, float InZ ) 
	: Center( FVector::ZeroVector )
	, Orientation( FQuat::Identity )
	, X(InX), Y(InY), Z(InZ)

	{

	}

	void Serialize( const FArchive& Ar );

	friend bool operator==( const FKBoxElem& LHS, const FKBoxElem& RHS )
	{
		return ( LHS.Center == RHS.Center &&
			LHS.Orientation == RHS.Orientation &&
			LHS.X == RHS.X &&
			LHS.Y == RHS.Y &&
			LHS.Z == RHS.Z );
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

	FORCEINLINE float GetVolume(const FVector& Scale) const { float MinScale = Scale.GetMin(); return (X * MinScale) * (Y * MinScale) * (Z * MinScale); }

	ENGINE_API void	DrawElemWire(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, float Scale, const FColor Color) const;
	ENGINE_API void	DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM, float Scale, const FMaterialRenderProxy* MaterialRenderProxy) const;
	ENGINE_API void GetElemSolid(const FTransform& ElemTM, float Scale, const FMaterialRenderProxy* MaterialRenderProxy, int32 ViewIndex, FMeshElementCollector& Collector) const;
	ENGINE_API FBox CalcAABB(const FTransform& BoneTM, float Scale) const;

	ENGINE_API void ScaleElem(FVector DeltaSize, float MinSize);
};
