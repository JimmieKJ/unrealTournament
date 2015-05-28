// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Node which outputs a texture object itself, instead of sampling the texture first.
 * This is used with material functions to provide a preview value for a texture function input.
 */

#pragma once
#include "Materials/MaterialExpressionTextureBase.h"
#include "MaterialExpressionTextureObject.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionTextureObject : public UMaterialExpressionTextureBase
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#if WITH_EDITOR
	virtual uint32 GetOutputType(int32 OutputIndex) override;
#endif // WITH_EDITOR
	// End UMaterialExpression Interface
	virtual int32 CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
};



