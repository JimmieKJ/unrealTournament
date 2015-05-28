// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionMakeMaterialAttributes.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionMakeMaterialAttributes : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput BaseColor;

	UPROPERTY()
	FExpressionInput Metallic;

	UPROPERTY()
	FExpressionInput Specular;

	UPROPERTY()
	FExpressionInput Roughness;

	UPROPERTY()
	FExpressionInput EmissiveColor;

	UPROPERTY()
	FExpressionInput Opacity;

	UPROPERTY()
	FExpressionInput OpacityMask;

	UPROPERTY()
	FExpressionInput Normal;

	UPROPERTY()
	FExpressionInput WorldPositionOffset;

	UPROPERTY()
	FExpressionInput WorldDisplacement;

	UPROPERTY()
	FExpressionInput TessellationMultiplier;

	UPROPERTY()
	FExpressionInput SubsurfaceColor;

	UPROPERTY()
	FExpressionInput ClearCoat;

	UPROPERTY()
	FExpressionInput ClearCoatRoughness;

	UPROPERTY()
	FExpressionInput AmbientOcclusion;

	UPROPERTY()
	FExpressionInput Refraction;

	UPROPERTY()
	FExpressionInput CustomizedUVs[8];

	UPROPERTY()
	FExpressionInput PixelDepthOffset;

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override {return true;}
	// End UMaterialExpression Interface
};



