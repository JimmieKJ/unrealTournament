// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "MaterialExpressionTextureSampleParameterSubUV.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionTextureSampleParameterSubUV : public UMaterialExpressionTextureSampleParameter2D
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureSampleParameterSubUV)
	uint32 bBlend:1;


	//~ Begin UMaterialExpressionTextureSampleParameter Interface
	virtual bool TextureIsValid( UTexture* InTexture ) override;
	virtual const TCHAR* GetRequirements() override;
	//~ End UMaterialExpressionTextureSampleParameter Interface

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};



