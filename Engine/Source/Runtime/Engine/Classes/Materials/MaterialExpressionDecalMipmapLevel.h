// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionDecalMipmapLevel.generated.h"


UCLASS(collapsecategories, hidecategories = Object)
class UMaterialExpressionDecalMipmapLevel : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** The texture's size */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to '(Const Width, Const Height)' if not specified"))
	FExpressionInput TextureSize;

	/** only used if TextureSize is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionDecalMipmapLevel, meta=(OverridingInputProperty = "TextureSize"))
	float ConstWidth;
	UPROPERTY(EditAnywhere, Category = MaterialExpressionDecalMipmapLevel, meta=(OverridingInputProperty = "TextureSize"))
	float ConstHeight;


	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface
};
