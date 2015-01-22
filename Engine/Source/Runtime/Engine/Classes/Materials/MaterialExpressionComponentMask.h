// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionComponentMask.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionComponentMask : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput Input;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionComponentMask)
	uint32 R:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionComponentMask)
	uint32 G:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionComponentMask)
	uint32 B:1;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionComponentMask)
	uint32 A:1;


	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	// End UMaterialExpression Interface
};



