// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 * Gives the depth of the current pixel being drawn for use in a material
 */

#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionPixelDepth.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionPixelDepth : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};



