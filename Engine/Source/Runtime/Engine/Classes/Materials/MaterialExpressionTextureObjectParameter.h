// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Node which creates a texture parameter and outputs the texture object itself, instead of sampling the texture first.
 * This is used with material functions to implement texture parameters without actually putting the parameter in the function.
 */

#pragma once
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "MaterialExpressionTextureObjectParameter.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionTextureObjectParameter : public UMaterialExpressionTextureSampleParameter
{
	GENERATED_UCLASS_BODY()


	// Begin UMaterialExpression Interface
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual const TArray<FExpressionInput*> GetInputs() override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual int32 CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
#if WITH_EDITOR
	virtual uint32 GetOutputType(int32 InputIndex) override {return MCT_Texture;}
#endif
	// End UMaterialExpression Interface

	// Begin UMaterialExpressionTextureSampleParameter Interface
	virtual const TCHAR* GetRequirements() override;
	// End UMaterialExpressionTextureSampleParameter Interface

};



