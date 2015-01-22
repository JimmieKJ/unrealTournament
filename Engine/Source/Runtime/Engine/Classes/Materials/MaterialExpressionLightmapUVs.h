// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 *	A material expression that routes LightmapUVs to the material.
 */

#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionLightmapUVs.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionLightmapUVs : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()


	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface
};



