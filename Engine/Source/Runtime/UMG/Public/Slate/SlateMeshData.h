// Cop// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateMeshData.generated.h"

class UStaticMesh;
class UMaterialInterface;

USTRUCT()
struct FSlateMeshVertex
{
	GENERATED_USTRUCT_BODY()

	static const int32 MaxNumUVs = 6;

	FSlateMeshVertex()
	{
	}

	FSlateMeshVertex(
		  FVector2D InPos
		, FColor InColor
		, FVector2D InUV0
		, FVector2D InUV1
		, FVector2D InUV2
		, FVector2D InUV3
		, FVector2D InUV4
		, FVector2D InUV5
		)
		: Position(InPos)
		, Color(InColor)
		, UV0(InUV0)
		, UV1(InUV1)
		, UV2(InUV2)
		, UV3(InUV3)
		, UV4(InUV4)
		, UV5(InUV5)	
	{
	}

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FColor Color;

	UPROPERTY()
	FVector2D UV0;

	UPROPERTY()
	FVector2D UV1;

	UPROPERTY()
	FVector2D UV2;

	UPROPERTY()
	FVector2D UV3;

	UPROPERTY()
	FVector2D UV4;

	UPROPERTY()
	FVector2D UV5;
};

UCLASS()
class UMG_API USlateMeshData : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	void InitFromStaticMesh( const UStaticMesh& InSourceMesh );

	const TArray<FSlateMeshVertex>& GetVertexData() const { return VertexData; }
	const TArray<uint32>& GetIndexData() const { return IndexData; }
	UMaterialInterface* GetMaterial() const { return Material; }
	UMaterialInstanceDynamic* ConvertToMaterialInstanceDynamic();

protected:

	UPROPERTY()
	TArray<FSlateMeshVertex> VertexData;

	UPROPERTY()
	TArray<uint32> IndexData;

	UPROPERTY()
	UMaterialInterface* Material;
};