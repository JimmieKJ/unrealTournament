// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionLandscapeLayerCoords.generated.h"

UENUM()
enum ETerrainCoordMappingType
{
	TCMT_Auto,
	TCMT_XY,
	TCMT_XZ,
	TCMT_YZ,
	TCMT_MAX,
};

UENUM()
enum ELandscapeCustomizedCoordType
{
	LCCT_None, // Don't use customized UV, just use original UV
	LCCT_CustomUV0,
	LCCT_CustomUV1,
	LCCT_CustomUV2,
	LCCT_WeightMapUV, // Use original WeightMapUV, which could not be customized
	LCCT_MAX,
};

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionLandscapeLayerCoords : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Determines the mapping place to use on the terrain. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerCoords)
	TEnumAsByte<enum ETerrainCoordMappingType> MappingType;

	/** Determines the mapping place to use on the terrain. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerCoords)
	TEnumAsByte<enum ELandscapeCustomizedCoordType> CustomUVType;

	/** Uniform scale to apply to the mapping. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerCoords)
	float MappingScale;

	/** Rotation to apply to the mapping. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerCoords)
	float MappingRotation;

	/** Offset to apply to the mapping along U. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerCoords)
	float MappingPanU;

	/** Offset to apply to the mapping along V. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerCoords)
	float MappingPanV;

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface
};



