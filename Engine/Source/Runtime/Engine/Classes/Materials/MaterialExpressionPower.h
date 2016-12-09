// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionPower.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionPower : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput Base;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstExponent' if not specified"))
	FExpressionInput Exponent;

	/** only used if Exponent is not hooked up */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionPower, meta=(OverridingInputProperty = "Exponent"))
	float ConstExponent;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface

};



