// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionLinearInterpolate.generated.h"

UCLASS(MinimalAPI)
class UMaterialExpressionLinearInterpolate : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstA' if not specified"))
	FExpressionInput A;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstB' if not specified"))
	FExpressionInput B;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstAlpha' if not specified"))
	FExpressionInput Alpha;

	/** only used if A is not hooked up */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLinearInterpolate, meta=(OverridingInputProperty = "A"))
	float ConstA;

	/** only used if B is not hooked up */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLinearInterpolate, meta=(OverridingInputProperty = "B"))
	float ConstB;

	/** only used if Alpha is not hooked up */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLinearInterpolate, meta=(OverridingInputProperty = "Alpha"))
	float ConstAlpha;


	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#if WITH_EDITOR
	virtual FText GetKeywords() const override {return FText::FromString(TEXT("lerp"));}
#endif // WITH_EDITOR
	// End UMaterialExpression Interface
};



